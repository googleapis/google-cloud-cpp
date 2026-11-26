// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/internal/async/hedged_open_stream.h"
#include "google/cloud/mocks/mock_async_streaming_read_write_rpc.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/mock_opentelemetry_metrics.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::MockCompletionQueueImpl;
using ::google::cloud::testing_util::MockCounter;
using ::google::cloud::testing_util::MockMeter;
using ::google::cloud::testing_util::MockMeterProvider;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::AnyOf;
using ::testing::ByMove;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::NiceMock;
using ::testing::Return;

using MetricsPtr = std::shared_ptr<storage::internal::HedgedReadMetrics>;
using TimerFuture = future<StatusOr<std::chrono::system_clock::time_point>>;
using MockRpc = google::cloud::mocks::MockAsyncStreamingReadWriteRpc<
    google::storage::v2::BidiReadObjectRequest,
    google::storage::v2::BidiReadObjectResponse>;

MetricsPtr MakeMockHedgeMetrics(int expected_dispatched, int expected_won) {
  opentelemetry::nostd::shared_ptr<MockMeter> meter =
      std::make_shared<MockMeter>();
  auto const expect_counter = [&](char const* name, int expected) {
    auto counter = std::make_unique<MockCounter<std::uint64_t>>();
    EXPECT_CALL(*counter, Add(std::uint64_t{1})).Times(expected);
    EXPECT_CALL(*meter, CreateUInt64Counter(Eq(name), _, _))
        .WillOnce(Return(ByMove(opentelemetry::nostd::unique_ptr<
                                opentelemetry::metrics::Counter<std::uint64_t>>(
            counter.release()))));
  };
  expect_counter("storage.read_hedging.hedges_dispatched", expected_dispatched);
  expect_counter("storage.read_hedging.hedge_won", expected_won);

  auto provider = std::make_shared<MockMeterProvider>();
  EXPECT_CALL(*provider, GetMeter).WillOnce(Return(meter));
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::MeterProvider>
      otel_provider{
          std::shared_ptr<opentelemetry::metrics::MeterProvider>(provider)};
  return std::make_shared<storage::internal::HedgedReadMetrics>(otel_provider);
}

OpenStreamResult MakeOpenResult(std::int64_t generation,
                                std::atomic<int>* stream_cancels) {
  auto rpc = std::make_unique<NiceMock<MockRpc>>();
  ON_CALL(*rpc, Cancel).WillByDefault([stream_cancels] {
    if (stream_cancels != nullptr) ++(*stream_cancels);
  });
  ON_CALL(*rpc, Finish).WillByDefault([] {
    return make_ready_future(Status{});
  });
  google::storage::v2::BidiReadObjectResponse response;
  response.mutable_metadata()->set_generation(generation);
  return OpenStreamResult{std::make_shared<OpenStream>(std::move(rpc)),
                          std::move(response)};
}

google::storage::v2::BidiReadObjectRequest MakeInitialRequest() {
  google::storage::v2::BidiReadObjectRequest req;
  req.mutable_read_object_spec()->set_bucket("projects/_/buckets/test-bucket");
  req.mutable_read_object_spec()->set_object("test-object");
  return req;
}

Status CancelledError() {
  return internal::CancelledError("cancelled", GCP_ERROR_INFO());
}

class HedgedOpenStreamTest : public ::testing::Test {
 protected:
  using StepValue = StatusOr<std::int64_t>;
  using ResultFuture = future<StatusOr<OpenStreamResult>>;

  void ExpectTimers(std::vector<std::string> names) {
    if (names.empty()) {
      EXPECT_CALL(*mock_cq_, MakeRelativeTimer).Times(0);
      return;
    }
    int const count = static_cast<int>(names.size());
    EXPECT_CALL(*mock_cq_, MakeRelativeTimer)
        .Times(count)
        .WillRepeatedly(
            [this, names = std::move(names)](std::chrono::nanoseconds d) {
              std::size_t const idx = timer_delays_.size();
              timer_delays_.push_back(
                  std::chrono::duration_cast<std::chrono::milliseconds>(d));
              std::string const name =
                  idx < names.size() ? names[idx] : std::string("ExtraTimer");
              future<StepValue> step = sequencer_.PushBack(name);
              auto timer_promise = std::make_shared<
                  promise<StatusOr<std::chrono::system_clock::time_point>>>(
                  [this] { ++cancelled_timers_; });
              TimerFuture timer_future = timer_promise->get_future();
              step.then([timer_promise](future<StepValue> f) mutable {
                if (!f.get().ok()) {
                  return timer_promise->set_value(CancelledError());
                }
                timer_promise->set_value(std::chrono::system_clock::now());
              });
              return timer_future;
            });
  }

  OpenStreamFactory SequencedFactory() {
    return [this, n = std::make_shared<std::atomic<int>>(0)](
               google::storage::v2::BidiReadObjectRequest const& req) {
      EXPECT_EQ(req.read_object_spec().object(), "test-object");
      int const id = (*n)++;
      std::string const label =
          id == 0 ? "Primary" : "Hedge" + std::to_string(id);
      future<StepValue> step = sequencer_.PushBack(label);
      auto attempt_promise =
          std::make_shared<promise<StatusOr<OpenStreamResult>>>(
              [this] { ++cancelled_attempts_; });
      ResultFuture out = attempt_promise->get_future();
      step.then([this, attempt_promise](future<StepValue> f) mutable {
        StepValue val = f.get();
        if (!val) return attempt_promise->set_value(std::move(val).status());
        attempt_promise->set_value(MakeOpenResult(*val, &cancelled_streams_));
      });
      return out;
    };
  }

  promise<StepValue> PopStep(char const* expected) {
    auto [p, name] = sequencer_.PopFrontWithName();
    EXPECT_EQ(name, expected);
    return std::move(p);
  }

  ResultFuture RunHedgedOpen(
      HedgedOpenParams params,
      std::shared_ptr<storage::internal::HedgeLimiter> limiter,
      MetricsPtr metrics) {
    return HedgedOpenStream(
        CompletionQueue(mock_cq_), SequencedFactory(), MakeInitialRequest(),
        storage::LimitedErrorCountAsyncRetryPolicy(0).clone(), params,
        std::move(limiter), std::move(metrics),
        google::cloud::internal::MakeImmutableOptions(Options{}));
  }

  static std::shared_ptr<storage::internal::HedgeLimiter> UnlimitedLimiter() {
    return std::make_shared<storage::internal::HedgeLimiter>(0.0, 0.0, 0, 0);
  }

  AsyncSequencer<StepValue> sequencer_;
  std::atomic<int> cancelled_timers_{0};
  std::atomic<int> cancelled_attempts_{0};
  std::atomic<int> cancelled_streams_{0};
  std::vector<std::chrono::milliseconds> timer_delays_;
  std::shared_ptr<MockCompletionQueueImpl> mock_cq_ =
      std::make_shared<MockCompletionQueueImpl>();
};

TEST_F(HedgedOpenStreamTest, FastPrimaryCancelsTimer) {
  MetricsPtr metrics = MakeMockHedgeMetrics(/*expected_dispatched=*/0,
                                            /*expected_won=*/0);
  ExpectTimers({"Timer1"});

  ResultFuture pending = RunHedgedOpen({std::chrono::milliseconds(250), 2},
                                       UnlimitedLimiter(), metrics);
  promise<StepValue> primary = PopStep("Primary");
  promise<StepValue> timer1 = PopStep("Timer1");
  EXPECT_THAT(timer_delays_, ElementsAre(std::chrono::milliseconds(250)));

  primary.set_value(101);
  StatusOr<OpenStreamResult> result = pending.get();
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result->first_response.metadata().generation(), 101);
  EXPECT_EQ(cancelled_timers_.load(), 1);
  EXPECT_EQ(cancelled_attempts_.load(), 0);

  timer1.set_value(CancelledError());
}

TEST_F(HedgedOpenStreamTest, SlowPrimaryHedgeWinsAndCancelsPrimary) {
  MetricsPtr metrics = MakeMockHedgeMetrics(/*expected_dispatched=*/1,
                                            /*expected_won=*/1);
  ExpectTimers({"Timer1", "Timer2"});

  ResultFuture pending = RunHedgedOpen({std::chrono::milliseconds(100), 2},
                                       UnlimitedLimiter(), metrics);
  promise<StepValue> primary = PopStep("Primary");
  PopStep("Timer1").set_value(0);
  promise<StepValue> hedge1 = PopStep("Hedge1");
  promise<StepValue> timer2 = PopStep("Timer2");

  hedge1.set_value(202);
  StatusOr<OpenStreamResult> result = pending.get();
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result->first_response.metadata().generation(), 202);
  EXPECT_EQ(cancelled_attempts_.load(), 1);
  EXPECT_EQ(cancelled_timers_.load(), 1);

  timer2.set_value(CancelledError());
  // Late-completing losing streams are immediately cancelled rather than
  // leaked.
  primary.set_value(999);
  EXPECT_EQ(cancelled_streams_.load(), 1);
}

TEST_F(HedgedOpenStreamTest, PrimaryWinsAfterHedgeDispatched) {
  MetricsPtr metrics = MakeMockHedgeMetrics(/*expected_dispatched=*/1,
                                            /*expected_won=*/0);
  ExpectTimers({"Timer1"});

  ResultFuture pending = RunHedgedOpen({std::chrono::milliseconds(100), 1},
                                       UnlimitedLimiter(), metrics);
  promise<StepValue> primary = PopStep("Primary");
  PopStep("Timer1").set_value(0);
  promise<StepValue> hedge1 = PopStep("Hedge1");

  primary.set_value(1);
  StatusOr<OpenStreamResult> result = pending.get();
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result->first_response.metadata().generation(), 1);
  EXPECT_EQ(cancelled_attempts_.load(), 1);

  hedge1.set_value(CancelledError());
}

class HedgedOpenRescueTest : public HedgedOpenStreamTest,
                             public ::testing::WithParamInterface<Status> {};

TEST_P(HedgedOpenRescueTest, PrimaryTransientOrStallErrorRescuedByHedge) {
  MetricsPtr metrics = MakeMockHedgeMetrics(/*expected_dispatched=*/1,
                                            /*expected_won=*/1);
  ExpectTimers({"Timer1"});

  ResultFuture pending = RunHedgedOpen({std::chrono::milliseconds(100), 1},
                                       UnlimitedLimiter(), metrics);
  promise<StepValue> primary = PopStep("Primary");
  PopStep("Timer1").set_value(0);
  promise<StepValue> hedge1 = PopStep("Hedge1");

  primary.set_value(GetParam());
  hedge1.set_value(303);

  StatusOr<OpenStreamResult> result = pending.get();
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result->first_response.metadata().generation(), 303);
}

INSTANTIATE_TEST_SUITE_P(PrimaryErrors, HedgedOpenRescueTest,
                         ::testing::Values(TransientError(),
                                           Status(StatusCode::kCancelled,
                                                  "Stream stall timeout")));

class HedgedOpenPermanentAbortTest
    : public HedgedOpenStreamTest,
      public ::testing::WithParamInterface<bool> {};

TEST_P(HedgedOpenPermanentAbortTest, PermanentFailureAbortsSiblingImmediately) {
  bool const primary_fails = GetParam();
  ExpectTimers({"Timer1", "Timer2"});

  ResultFuture pending = RunHedgedOpen({std::chrono::milliseconds(100), 2},
                                       UnlimitedLimiter(), nullptr);
  promise<StepValue> primary = PopStep("Primary");
  PopStep("Timer1").set_value(0);
  promise<StepValue> hedge1 = PopStep("Hedge1");
  promise<StepValue> timer2 = PopStep("Timer2");

  if (primary_fails) {
    primary.set_value(PermanentError());
  } else {
    hedge1.set_value(PermanentError());
  }
  EXPECT_THAT(pending.get(), StatusIs(PermanentError().code()));
  EXPECT_EQ(cancelled_attempts_.load(), 1);
  EXPECT_EQ(cancelled_timers_.load(), 1);

  timer2.set_value(CancelledError());
  if (primary_fails) {
    hedge1.set_value(CancelledError());
  } else {
    primary.set_value(CancelledError());
  }
}

INSTANTIATE_TEST_SUITE_P(FailureOrigin, HedgedOpenPermanentAbortTest,
                         ::testing::Bool());

TEST_F(HedgedOpenStreamTest, AllAttemptsFailReturnsPrimaryError) {
  ExpectTimers({"Timer1"});

  ResultFuture pending = RunHedgedOpen({std::chrono::milliseconds(100), 1},
                                       UnlimitedLimiter(), nullptr);
  promise<StepValue> primary = PopStep("Primary");
  PopStep("Timer1").set_value(0);
  promise<StepValue> hedge1 = PopStep("Hedge1");

  primary.set_value(Status(StatusCode::kUnavailable, "primary unavailable"));
  hedge1.set_value(Status(StatusCode::kDeadlineExceeded, "hedge deadline"));

  EXPECT_THAT(pending.get(),
              StatusIs(StatusCode::kUnavailable, HasSubstr("primary")));
}

TEST_F(HedgedOpenStreamTest, CallerCancelCancelsAllAttemptsAndTimer) {
  ExpectTimers({"Timer1", "Timer2"});

  ResultFuture pending = RunHedgedOpen({std::chrono::milliseconds(100), 2},
                                       UnlimitedLimiter(), nullptr);
  promise<StepValue> primary = PopStep("Primary");
  PopStep("Timer1").set_value(0);
  promise<StepValue> hedge1 = PopStep("Hedge1");
  promise<StepValue> timer2 = PopStep("Timer2");

  pending.cancel();
  EXPECT_EQ(cancelled_timers_.load(), 1);
  EXPECT_EQ(cancelled_attempts_.load(), 2);

  timer2.set_value(CancelledError());
  primary.set_value(TransientError());
  hedge1.set_value(TransientError());

  EXPECT_THAT(pending.get(), StatusIs(StatusCode::kCancelled));
}

TEST_F(HedgedOpenStreamTest, CallerCancelWithInlineAttemptCompletion) {
  ExpectTimers({"Timer1"});
  promise<StatusOr<OpenStreamResult>> attempt_promise{null_promise_t{}};
  attempt_promise = promise<StatusOr<OpenStreamResult>>(
      [&attempt_promise] { attempt_promise.set_value(CancelledError()); });
  OpenStreamFactory inline_cancel_factory =
      [&attempt_promise](google::storage::v2::BidiReadObjectRequest const&) {
        return attempt_promise.get_future();
      };

  ResultFuture pending = HedgedOpenStream(
      CompletionQueue(mock_cq_), std::move(inline_cancel_factory),
      MakeInitialRequest(),
      storage::LimitedErrorCountAsyncRetryPolicy(0).clone(),
      {std::chrono::milliseconds(100), 1}, UnlimitedLimiter(), nullptr,
      google::cloud::internal::MakeImmutableOptions(Options{}));
  promise<StepValue> timer1 = PopStep("Timer1");

  pending.cancel();
  EXPECT_THAT(pending.get(), StatusIs(StatusCode::kCancelled));
  timer1.set_value(CancelledError());
}

TEST_F(HedgedOpenStreamTest,
       LimiterDenialRetriesAtMinDelayAndRespectsLifetimeBudget) {
  using ms = std::chrono::milliseconds;
  auto limiter = std::make_shared<storage::internal::HedgeLimiter>(
      0.0, 0.0, /*max_concurrent=*/1, /*max_total=*/2);
  MetricsPtr metrics = MakeMockHedgeMetrics(/*expected_dispatched=*/2,
                                            /*expected_won=*/2);
  ExpectTimers(
      {"Call1Timer1", "Call2Timer1", "Call1Timer2", "Call2RetryTimer"});

  ResultFuture call1 = RunHedgedOpen({ms(1), 2}, limiter, metrics);
  ResultFuture call2 = RunHedgedOpen({ms(1), 2}, limiter, metrics);
  promise<StepValue> c1_primary = PopStep("Primary");
  promise<StepValue> c1_timer1 = PopStep("Call1Timer1");
  promise<StepValue> c2_primary = PopStep("Primary");
  promise<StepValue> c2_timer1 = PopStep("Call2Timer1");

  // Call 1 acquires the single concurrency slot (total 1/2) and arms Timer 2.
  c1_timer1.set_value(0);
  promise<StepValue> c1_hedge = PopStep("Hedge1");
  promise<StepValue> c1_timer2 = PopStep("Call1Timer2");

  // Call 2 is denied by `max_concurrent = 1` and re-arms at the 10ms floor.
  c2_timer1.set_value(0);
  promise<StepValue> c2_retry = PopStep("Call2RetryTimer");
  EXPECT_THAT(timer_delays_, ElementsAre(ms(1), ms(1), ms(1), ms(10)));

  // Finishing Call 1's hedge releases its concurrency slot.
  c1_hedge.set_value(601);
  ASSERT_TRUE(call1.get().ok());
  c1_timer2.set_value(CancelledError());

  // Call 2's retry timer acquires the freed slot and spends total budget (2/2),
  // so no further timer is armed on Call 2 or on a subsequent Call 3.
  c2_retry.set_value(0);
  PopStep("Hedge1").set_value(602);
  ASSERT_TRUE(call2.get().ok());

  ResultFuture call3 = RunHedgedOpen({ms(1), 2}, limiter, metrics);
  PopStep("Primary").set_value(603);
  ASSERT_TRUE(call3.get().ok());

  c1_primary.set_value(CancelledError());
  c2_primary.set_value(CancelledError());
}

TEST_F(HedgedOpenStreamTest, ClampsNegativeParamsToZero) {
  // Negative `max_hedges` clamps to 0 (no timers armed).
  ExpectTimers({});
  ResultFuture zero_hedges = RunHedgedOpen({std::chrono::milliseconds(-50), -3},
                                           UnlimitedLimiter(), nullptr);
  PopStep("Primary").set_value(42);
  ASSERT_TRUE(zero_hedges.get().ok());

  // Negative `delay` clamps to 0ms.
  timer_delays_.clear();
  ExpectTimers({"Timer1"});
  ResultFuture zero_delay = RunHedgedOpen({std::chrono::milliseconds(-25), 1},
                                          UnlimitedLimiter(), nullptr);
  promise<StepValue> primary = PopStep("Primary");
  promise<StepValue> timer1 = PopStep("Timer1");
  EXPECT_THAT(timer_delays_, ElementsAre(std::chrono::milliseconds(0)));
  primary.set_value(43);
  ASSERT_TRUE(zero_delay.get().ok());
  timer1.set_value(CancelledError());
}

TEST(HedgedOpenStreamConcurrencyTest, ConcurrentCompletionRace) {
  for (int iter = 0; iter != 25; ++iter) {
    std::mutex gate_mu;
    std::condition_variable gate_cv;
    int ready_workers = 0;
    bool release_workers = false;
    std::atomic<int> cancelled_streams{0};
    std::atomic<int> attempt_seq{0};
    std::vector<std::thread> workers;
    workers.reserve(2);

    OpenStreamFactory factory =
        [&](google::storage::v2::BidiReadObjectRequest const&) {
          std::int64_t const gen = ++attempt_seq;
          OpenStreamResult open_result =
              MakeOpenResult(gen, &cancelled_streams);
          auto p = std::make_shared<promise<StatusOr<OpenStreamResult>>>();
          future<StatusOr<OpenStreamResult>> out = p->get_future();
          {
            std::lock_guard<std::mutex> lock(gate_mu);
            workers.emplace_back(
                [&, p = std::move(p), res = std::move(open_result)]() mutable {
                  {
                    std::unique_lock<std::mutex> wlock(gate_mu);
                    ++ready_workers;
                    gate_cv.notify_all();
                    gate_cv.wait(wlock, [&] { return release_workers; });
                  }
                  p->set_value(std::move(res));
                });
          }
          return out;
        };

    auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
    EXPECT_CALL(*mock_cq, MakeRelativeTimer)
        .WillOnce([](std::chrono::nanoseconds) {
          return make_ready_future(
              StatusOr<std::chrono::system_clock::time_point>(
                  std::chrono::system_clock::now()));
        });

    future<StatusOr<OpenStreamResult>> pending = HedgedOpenStream(
        CompletionQueue(mock_cq), std::move(factory), MakeInitialRequest(),
        storage::LimitedErrorCountAsyncRetryPolicy(0).clone(),
        {std::chrono::milliseconds(0), 1},
        std::make_shared<storage::internal::HedgeLimiter>(0.0, 0.0, 0, 0),
        nullptr, google::cloud::internal::MakeImmutableOptions(Options{}));

    {
      std::unique_lock<std::mutex> lock(gate_mu);
      gate_cv.wait(lock, [&] { return ready_workers == 2; });
      release_workers = true;
    }
    gate_cv.notify_all();
    for (std::thread& w : workers) w.join();

    StatusOr<OpenStreamResult> result = pending.get();
    ASSERT_TRUE(result.ok());
    EXPECT_THAT(result->first_response.metadata().generation(),
                AnyOf(Eq(1), Eq(2)));
    // Exactly the losing attempt's `OpenStream` is cancelled, never the winner.
    EXPECT_EQ(cancelled_streams.load(), 1);
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
