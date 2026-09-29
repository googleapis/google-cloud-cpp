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

#include "google/cloud/storage/internal/hedged_object_read_source.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::storage::testing::MockObjectReadSource;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AtMost;
using ::testing::Eq;
using ::testing::Return;

// Large enough that no test read is treated as oversized.
std::size_t constexpr kUnlimitedBuffer = std::size_t{1} << 30;

// A hedge delay no test read exceeds, so a stream that answers immediately is
// never considered stalled and never hedged after the open.
auto constexpr kLongDelay = std::chrono::seconds(30);

// The hedge delay for tests that drive a stall: `kStall` is comfortably above
// it, while an immediate answer is comfortably below it.
auto constexpr kDelay = std::chrono::milliseconds(100);
auto constexpr kStall = std::chrono::milliseconds(200);

std::shared_ptr<ThreadPool> MakeUnlimitedReadPool() {
  return std::make_shared<ThreadPool>(/*max_threads=*/4);
}

std::shared_ptr<HedgingThreadPool> MakeUnlimitedHedgePool() {
  return std::make_shared<HedgingThreadPool>(
      /*max_threads=*/4, /*rate_limit=*/0.0, /*capacity=*/0.0,
      /*max_concurrent=*/0);
}

// Single-worker read pool that records its worker's `std::thread::id`.
//
// Because `ReadRaced()` enqueues the primary attempt onto `read_pool_`
// asynchronously, a hedge attempt on `hedge_pool_` can enter `factory()`
// before the primary attempt does on the initial read. Tests that configure
// distinct mock expectations for the primary and hedge attempts must
// distinguish them by executing thread ID rather than by `factory()`
// invocation order.
class PrimaryReadPool {
 public:
  PrimaryReadPool() {
    // Eagerly spawn the single worker thread and capture its ID. Every
    // subsequent primary attempt enqueued onto `pool_` will execute on this
    // worker thread. The promise can live on the stack: `get()` below blocks
    // until the worker calls `set_value()`, and the worker never touches the
    // promise afterwards.
    std::promise<std::thread::id> worker_id;
    pool_->Enqueue(
        [&worker_id] { worker_id.set_value(std::this_thread::get_id()); });
    worker_id_ = worker_id.get_future().get();
  }

  std::shared_ptr<ThreadPool> const& pool() const { return pool_; }

  /// Returns the thread ID of the dedicated primary worker thread.
  std::thread::id worker_id() const { return worker_id_; }

 private:
  std::shared_ptr<ThreadPool> pool_ = std::make_shared<ThreadPool>(1);
  std::thread::id worker_id_;
};

/// Returns true if the caller is executing on the primary read pool worker.
bool OnPrimaryThread(std::thread::id primary_thread) {
  return std::this_thread::get_id() == primary_thread;
}

// Most tests do not care about the offset or generation a child is opened at.
template <typename F>
HedgedObjectReadSource::ChildFactory Adapt(F f) {
  return [f = std::move(f)](std::int64_t, std::optional<std::int64_t>) {
    return f();
  };
}

ReadSourceResult MakeReadResult(std::string const& payload) {
  return ReadSourceResult{payload.size(),
                          HttpResponse{HttpStatusCode::kOk, {}, {}}};
}

// A `Read()` action that returns @p payload immediately.
auto ImmediateRead(std::string payload) {
  return [payload = std::move(payload)](char* buf, std::size_t n) {
    EXPECT_LE(payload.size(), n);
    std::copy(payload.begin(), payload.end(), buf);
    return MakeReadResult(payload);
  };
}

// A `Read()` action that returns @p payload after @p delay. With a delay above
// the source's hedge delay this marks the stream as stalled.
auto DelayedRead(std::string payload, std::chrono::milliseconds delay) {
  return [payload = std::move(payload), delay](char* buf, std::size_t n) {
    EXPECT_LE(payload.size(), n);
    std::this_thread::sleep_for(delay);
    std::copy(payload.begin(), payload.end(), buf);
    return MakeReadResult(payload);
  };
}

// A `Read()` action that returns @p payload once @p unblock is set.
auto BlockedRead(std::shared_ptr<std::promise<void>> unblock,
                 std::string payload) {
  return [unblock = std::move(unblock), payload = std::move(payload)](
             char* buf, std::size_t n) {
    EXPECT_LE(payload.size(), n);
    unblock->get_future().get();
    std::copy(payload.begin(), payload.end(), buf);
    return MakeReadResult(payload);
  };
}

// Blocks until @p signal is set, or until a generous timeout expires.
//
// Tests that need an attempt to outlive the hedge dispatch must wait for the
// hedge itself rather than sleep: the two run on different threads, and a
// sleep long enough on an idle machine can still be too short on a loaded one.
// The timeout keeps a regression a test failure instead of a hang.
void WaitForSignal(std::shared_ptr<std::promise<void>> const& signal) {
  EXPECT_EQ(signal->get_future().wait_for(std::chrono::seconds(10)),
            std::future_status::ready);
}

// Records that a hedge reached the factory. `Signal()` is safe to call from
// several attempts, only the first one sets the promise.
struct HedgeSignal {
  std::shared_ptr<std::promise<void>> reached =
      std::make_shared<std::promise<void>>();
  std::shared_ptr<std::atomic<bool>> signalled =
      std::make_shared<std::atomic<bool>>(false);

  void Signal() const {
    if (!signalled->exchange(true)) reached->set_value();
  }
  void Wait() const { WaitForSignal(reached); }
};

// A `Close()` action that sets @p closed.
auto NotifyClose(std::shared_ptr<std::promise<void>> closed) {
  return [closed = std::move(closed)]() {
    closed->set_value();
    return make_status_or(HttpResponse{HttpStatusCode::kOk, {}, {}});
  };
}

auto MakeStallingPrimaryFactory(
    std::shared_ptr<std::promise<void>> const& unblock_primary,
    std::shared_ptr<std::promise<void>> const& primary_closed,
    std::shared_ptr<std::atomic<int>> const& calls) {
  return [unblock_primary, primary_closed,
          calls]() -> StatusOr<std::unique_ptr<ObjectReadSource>> {
    auto mock = std::make_unique<MockObjectReadSource>();
    if (++*calls == 1) {
      EXPECT_CALL(*mock, Read).WillOnce(BlockedRead(unblock_primary, "slow"));
      EXPECT_CALL(*mock, Close).WillOnce(NotifyClose(primary_closed));
    } else {
      EXPECT_CALL(*mock, Read).WillOnce(ImmediateRead("hedge"));
    }
    return std::unique_ptr<ObjectReadSource>(std::move(mock));
  };
}

TEST(HedgedObjectReadSourceTest, PrimaryWins) {
  auto factory = []() -> StatusOr<std::unique_ptr<ObjectReadSource>> {
    auto mock = std::make_unique<MockObjectReadSource>();
    EXPECT_CALL(*mock, Read).WillOnce(ImmediateRead("payload"));
    return std::unique_ptr<ObjectReadSource>(std::move(mock));
  };

  HedgedObjectReadSource source(MakeUnlimitedReadPool(),
                                MakeUnlimitedHedgePool(), Adapt(factory),
                                kLongDelay, /*max_hedges=*/2, kUnlimitedBuffer);

  std::vector<char> buffer(100);
  auto result = source.Read(buffer.data(), buffer.size());
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(result->bytes_received, Eq(7));
  EXPECT_THAT(std::string(buffer.data(), result->bytes_received),
              Eq("payload"));
}

TEST(HedgedObjectReadSourceTest, SubsequentReadsContinueOnWinner) {
  // Once the open race is decided, reads on a healthy stream continue on the
  // winning child without opening new children: the factory is called exactly
  // once.
  auto factory_calls = std::make_shared<std::atomic<int>>(0);
  auto factory =
      [factory_calls]() -> StatusOr<std::unique_ptr<ObjectReadSource>> {
    ++*factory_calls;
    auto mock = std::make_unique<MockObjectReadSource>();
    EXPECT_CALL(*mock, Read)
        .WillOnce(ImmediateRead("chunk-1"))
        .WillOnce(ImmediateRead("chunk-2"))
        .WillOnce(ImmediateRead("chunk-3"));
    return std::unique_ptr<ObjectReadSource>(std::move(mock));
  };

  HedgedObjectReadSource source(MakeUnlimitedReadPool(),
                                MakeUnlimitedHedgePool(), Adapt(factory),
                                kLongDelay, /*max_hedges=*/2, kUnlimitedBuffer);

  std::vector<char> buffer(100);
  for (auto const* expected : {"chunk-1", "chunk-2", "chunk-3"}) {
    auto result = source.Read(buffer.data(), buffer.size());
    ASSERT_THAT(result, IsOk());
    EXPECT_THAT(std::string(buffer.data(), result->bytes_received),
                Eq(expected));
  }
  EXPECT_THAT(factory_calls->load(), Eq(1));
}

TEST(HedgedObjectReadSourceTest, HedgeWinsWhenPrimaryStalls) {
  // The primary blocks until the end of the test, the hedge answers
  // immediately. The read must complete with the hedge's data, and the
  // (losing) primary must be closed once it completes.
  auto unblock_primary = std::make_shared<std::promise<void>>();
  auto primary_closed = std::make_shared<std::promise<void>>();
  auto calls = std::make_shared<std::atomic<int>>(0);
  auto factory =
      MakeStallingPrimaryFactory(unblock_primary, primary_closed, calls);

  auto source = std::make_unique<HedgedObjectReadSource>(
      MakeUnlimitedReadPool(), MakeUnlimitedHedgePool(), Adapt(factory),
      std::chrono::milliseconds(1),
      /*max_hedges=*/2, kUnlimitedBuffer);

  std::vector<char> buffer(100);
  auto result = source->Read(buffer.data(), buffer.size());
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(result->bytes_received, Eq(5));
  EXPECT_THAT(std::string(buffer.data(), result->bytes_received), Eq("hedge"));

  unblock_primary->set_value();
  primary_closed->get_future().get();
}

TEST(HedgedObjectReadSourceTest, ReadPoolSaturationDoesNotBlockHedges) {
  // Verify thread pool isolation: If the read pool is busy with slow reads,
  // speculative hedge attempts on hedge_pool_ can still execute immediately.
  auto unblock_primary = std::make_shared<std::promise<void>>();
  auto primary_closed = std::make_shared<std::promise<void>>();
  auto calls = std::make_shared<std::atomic<int>>(0);
  auto factory =
      MakeStallingPrimaryFactory(unblock_primary, primary_closed, calls);

  HedgedObjectReadSource source(std::make_shared<ThreadPool>(/*max_threads=*/1),
                                MakeUnlimitedHedgePool(), Adapt(factory),
                                std::chrono::milliseconds(1),
                                /*max_hedges=*/2, kUnlimitedBuffer);

  std::vector<char> buffer(100);
  auto result = source.Read(buffer.data(), buffer.size());
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(result->bytes_received, Eq(5));
  EXPECT_THAT(std::string(buffer.data(), result->bytes_received), Eq("hedge"));

  unblock_primary->set_value();
  primary_closed->get_future().get();
}

TEST(HedgedObjectReadSourceTest, HedgePoolExhaustionDoesNotBlockPrimary) {
  // Verify that if the hedge pool is fully exhausted / rate limited (0 tokens),
  // the primary attempt on read_pool still completes successfully.
  auto read_pool = MakeUnlimitedReadPool();
  auto hedge_pool = std::make_shared<HedgingThreadPool>(
      /*max_threads=*/1, /*rate_limit=*/0.0, /*capacity=*/0.0,
      /*max_concurrent=*/1);
  // Acquire the only slot so hedge pool has 0 available capacity.
  ASSERT_TRUE(hedge_pool->TryAcquireHedgeToken());

  auto calls = std::make_shared<std::atomic<int>>(0);
  auto factory = [calls]() -> StatusOr<std::unique_ptr<ObjectReadSource>> {
    ++*calls;
    auto mock = std::make_unique<MockObjectReadSource>();
    EXPECT_CALL(*mock, Read).WillOnce(ImmediateRead("primary_only"));
    return std::unique_ptr<ObjectReadSource>(std::move(mock));
  };

  HedgedObjectReadSource source(read_pool, hedge_pool, Adapt(factory),
                                std::chrono::milliseconds(10),
                                /*max_hedges=*/2, kUnlimitedBuffer);

  std::vector<char> buffer(100);
  auto result = source.Read(buffer.data(), buffer.size());
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(result->bytes_received, Eq(12));
  EXPECT_THAT(std::string(buffer.data(), result->bytes_received),
              Eq("primary_only"));
  EXPECT_THAT(calls->load(), Eq(1));

  hedge_pool->ReleaseHedgeSlot();
}

TEST(HedgedObjectReadSourceTest,
     TransientHedgePoolExhaustionDoesNotBurnHedgeAttempt) {
  // Verify that if TryAcquireHedgeToken() fails on an initial tick due to
  // transient exhaustion, the hedge attempt slot is not burned and a hedge is
  // successfully dispatched on a subsequent tick once capacity becomes
  // available.
  auto unblock_primary = std::make_shared<std::promise<void>>();
  auto primary_closed = std::make_shared<std::promise<void>>();
  auto calls = std::make_shared<std::atomic<int>>(0);
  auto factory =
      MakeStallingPrimaryFactory(unblock_primary, primary_closed, calls);

  auto hedge_pool = std::make_shared<HedgingThreadPool>(
      /*max_threads=*/1, /*rate_limit=*/0.0, /*capacity=*/0.0,
      /*max_concurrent=*/1);
  // Acquire the only slot so hedge pool has 0 available capacity initially.
  ASSERT_TRUE(hedge_pool->TryAcquireHedgeToken());

  // In a background thread, release the slot after a brief delay so it is
  // available on a subsequent tick.
  std::thread releaser([hedge_pool] {
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    hedge_pool->ReleaseHedgeSlot();
  });

  HedgedObjectReadSource source(MakeUnlimitedReadPool(), hedge_pool,
                                Adapt(factory), std::chrono::milliseconds(10),
                                /*max_hedges=*/1, kUnlimitedBuffer);

  std::vector<char> buffer(100);
  auto result = source.Read(buffer.data(), buffer.size());
  releaser.join();

  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(result->bytes_received, Eq(5));
  EXPECT_THAT(std::string(buffer.data(), result->bytes_received), Eq("hedge"));
  EXPECT_THAT(calls->load(), Eq(2));

  unblock_primary->set_value();
  primary_closed->get_future().get();
}

TEST(HedgedObjectReadSourceTest, HedgeOpenFailureReleasesSlot) {
  // Verify that if a hedge attempt fails during stream opening (factory()
  // error), the hedge concurrency slot is released via RAII (SlotGuard) and is
  // not leaked.
  auto unblock_primary = std::make_shared<std::promise<void>>();
  // Signaled when the hedge attempt returns an open error from `factory()`,
  // ensuring the primary attempt remains blocked until the hedge has run.
  auto hedge_open_failed = std::make_shared<std::promise<void>>();
  auto hedge_signalled = std::make_shared<std::atomic<bool>>(false);
  auto calls = std::make_shared<std::atomic<int>>(0);
  PrimaryReadPool read_pool;
  std::thread::id const primary_thread = read_pool.worker_id();
  auto factory =
      [unblock_primary, hedge_open_failed, hedge_signalled, calls,
       primary_thread]() -> StatusOr<std::unique_ptr<ObjectReadSource>> {
    ++*calls;
    if (OnPrimaryThread(primary_thread)) {
      // Primary attempt: stalls until unblocked.
      auto mock = std::make_unique<MockObjectReadSource>();
      EXPECT_CALL(*mock, Read)
          .WillOnce(BlockedRead(unblock_primary, "primary"));
      return std::unique_ptr<ObjectReadSource>(std::move(mock));
    }
    // Hedge attempt: simulate an open failure and notify the unblocker thread.
    if (!hedge_signalled->exchange(true)) hedge_open_failed->set_value();
    return Status(StatusCode::kUnavailable, "open failed");
  };

  // Configure a single worker thread so tasks enqueued on `hedge_pool` execute
  // strictly in FIFO order, allowing a barrier task to synchronize with the
  // completion of `RunAttempt()`.
  auto hedge_pool = std::make_shared<HedgingThreadPool>(
      /*max_threads=*/1, /*rate_limit=*/0.0, /*capacity=*/0.0,
      /*max_concurrent=*/1);

  HedgedObjectReadSource source(read_pool.pool(), hedge_pool, Adapt(factory),
                                std::chrono::milliseconds(1),
                                /*max_hedges=*/1, kUnlimitedBuffer);

  std::vector<char> buffer(100);
  std::thread unblocker([unblock_primary, hedge_open_failed] {
    // Wait (with a timeout) for the hedge open attempt to fail before
    // unblocking the primary read.
    hedge_open_failed->get_future().wait_for(std::chrono::seconds(10));
    unblock_primary->set_value();
  });

  auto result = source.Read(buffer.data(), buffer.size());
  unblocker.join();

  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(std::string(buffer.data(), result->bytes_received),
              Eq("primary"));
  EXPECT_THAT(calls->load(), Eq(2));

  // `SlotGuard` releases the concurrency slot when `RunAttempt()` exits, which
  // occurs after `factory()` returns. Enqueue a barrier task on the
  // single-worker `hedge_pool` to wait until `RunAttempt()` has finished
  // unwinding and released the slot.
  auto hedge_slot_released = std::make_shared<std::promise<void>>();
  hedge_pool->Enqueue(
      [hedge_slot_released] { hedge_slot_released->set_value(); });
  WaitForSignal(hedge_slot_released);

  // If the slot leaked on open failure, TryAcquireHedgeToken would fail because
  // max_concurrent is 1.
  EXPECT_TRUE(hedge_pool->TryAcquireHedgeToken());
  hedge_pool->ReleaseHedgeSlot();
}

TEST(HedgedObjectReadSourceTest, ZeroDelayBacksOffOnHedgeTokenExhaustion) {
  // Verify that when delay_ == 0ms and TryAcquireHedgeToken() returns false,
  // the hedging loop backs off instead of busy-spinning, allowing the primary
  // read to complete normally.
  auto unblock_primary = std::make_shared<std::promise<void>>();
  auto calls = std::make_shared<std::atomic<int>>(0);
  auto factory = [unblock_primary,
                  calls]() -> StatusOr<std::unique_ptr<ObjectReadSource>> {
    ++*calls;
    auto mock = std::make_unique<MockObjectReadSource>();
    EXPECT_CALL(*mock, Read)
        .WillOnce(BlockedRead(unblock_primary, "primary_data"));
    return std::unique_ptr<ObjectReadSource>(std::move(mock));
  };

  auto hedge_pool = std::make_shared<HedgingThreadPool>(
      /*max_threads=*/1, /*rate_limit=*/0.0, /*capacity=*/0.0,
      /*max_concurrent=*/1);
  // Exhaust all hedge slots so TryAcquireHedgeToken fails.
  ASSERT_TRUE(hedge_pool->TryAcquireHedgeToken());

  HedgedObjectReadSource source(MakeUnlimitedReadPool(), hedge_pool,
                                Adapt(factory), std::chrono::milliseconds(0),
                                /*max_hedges=*/2, kUnlimitedBuffer);

  std::vector<char> buffer(100);
  std::thread unblocker([unblock_primary] {
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    unblock_primary->set_value();
  });

  auto result = source.Read(buffer.data(), buffer.size());
  unblocker.join();

  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(std::string(buffer.data(), result->bytes_received),
              Eq("primary_data"));
  EXPECT_THAT(calls->load(), Eq(1));

  hedge_pool->ReleaseHedgeSlot();
}

TEST(HedgedObjectReadSourceTest, NonPositiveMaxHedgesDoesNotHedge) {
  // Verify that negative or zero max_hedges values defensively result in 0
  // hedge attempts, running only the primary attempt.
  auto calls = std::make_shared<std::atomic<int>>(0);
  auto factory = [calls]() -> StatusOr<std::unique_ptr<ObjectReadSource>> {
    ++*calls;
    auto mock = std::make_unique<MockObjectReadSource>();
    EXPECT_CALL(*mock, Read).WillOnce(ImmediateRead("primary_only"));
    return std::unique_ptr<ObjectReadSource>(std::move(mock));
  };

  HedgedObjectReadSource source(MakeUnlimitedReadPool(),
                                MakeUnlimitedHedgePool(), Adapt(factory),
                                std::chrono::milliseconds(0),
                                /*max_hedges=*/-1, kUnlimitedBuffer);

  std::vector<char> buffer(100);
  auto result = source.Read(buffer.data(), buffer.size());
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(std::string(buffer.data(), result->bytes_received),
              Eq("primary_only"));
  EXPECT_THAT(calls->load(), Eq(1));
}

TEST(HedgedObjectReadSourceTest, PrimaryOpenErrorPropagates) {
  auto factory = []() -> StatusOr<std::unique_ptr<ObjectReadSource>> {
    return Status(StatusCode::kPermissionDenied, "uh-oh");
  };

  HedgedObjectReadSource source(MakeUnlimitedReadPool(),
                                MakeUnlimitedHedgePool(), Adapt(factory),
                                kLongDelay, /*max_hedges=*/2, kUnlimitedBuffer);

  std::vector<char> buffer(100);
  EXPECT_THAT(source.Read(buffer.data(), buffer.size()),
              StatusIs(StatusCode::kPermissionDenied));
  // Nothing was opened, the stream must not report itself as open.
  EXPECT_FALSE(source.IsOpen());
}

TEST(HedgedObjectReadSourceTest, PermanentPrimaryErrorResolvesImmediately) {
  // The primary fails with a permanent error while a hedge is in flight and
  // stalled. Waiting for the hedge cannot change the outcome, so the error
  // must be reported at once, and the hedge must be closed when it completes.
  auto unblock_hedge = std::make_shared<std::promise<void>>();
  auto hedge_closed = std::make_shared<std::promise<void>>();
  auto calls = std::make_shared<std::atomic<int>>(0);
  HedgeSignal hedge_started;
  PrimaryReadPool read_pool;
  std::thread::id const primary_thread = read_pool.worker_id();
  auto factory =
      [unblock_hedge, hedge_closed, calls, hedge_started,
       primary_thread]() -> StatusOr<std::unique_ptr<ObjectReadSource>> {
    auto mock = std::make_unique<MockObjectReadSource>();
    ++*calls;
    // Assign mock behavior by executing thread ID so the primary attempt
    // deterministically receives `kNotFound` while the hedge attempt blocks on
    // `unblock_hedge`.
    if (OnPrimaryThread(primary_thread)) {
      // Fail only once the hedge has been dispatched, otherwise the race is
      // over before there is anything to hedge. The failed child reports
      // itself as already closed, so it must not be closed again.
      EXPECT_CALL(*mock, Read).WillOnce([hedge_started](char*, std::size_t) {
        hedge_started.Wait();
        return StatusOr<ReadSourceResult>(
            Status(StatusCode::kNotFound, "object deleted"));
      });
      EXPECT_CALL(*mock, IsOpen).WillRepeatedly(Return(false));
      EXPECT_CALL(*mock, Close).Times(0);
    } else {
      hedge_started.Signal();
      EXPECT_CALL(*mock, Read).WillOnce(BlockedRead(unblock_hedge, "hedge"));
      EXPECT_CALL(*mock, Close).WillOnce(NotifyClose(hedge_closed));
    }
    return std::unique_ptr<ObjectReadSource>(std::move(mock));
  };

  HedgedObjectReadSource source(read_pool.pool(), MakeUnlimitedHedgePool(),
                                Adapt(factory), kDelay, /*max_hedges=*/1,
                                kUnlimitedBuffer);

  std::vector<char> buffer(100);
  auto result = source.Read(buffer.data(), buffer.size());
  EXPECT_THAT(result, StatusIs(StatusCode::kNotFound));
  EXPECT_FALSE(source.IsOpen());

  unblock_hedge->set_value();
  WaitForSignal(hedge_closed);
  // Asserted only once the hedge has run to completion. A permanent primary
  // error resolves the race without waiting for the hedge to retire, so
  // `Read()` returning says nothing about how far the hedge has progressed.
  EXPECT_THAT(calls->load(), Eq(2));
}

TEST(HedgedObjectReadSourceTest, AllAttemptsFailReportsPrimaryError) {
  // The hedge fails first with one error, the primary later with another.
  // The stream the caller is reading is the primary, so its error is the one
  // reported.
  auto calls = std::make_shared<std::atomic<int>>(0);
  HedgeSignal hedge_started;
  PrimaryReadPool read_pool;
  std::thread::id const primary_thread = read_pool.worker_id();
  auto factory =
      [calls, hedge_started,
       primary_thread]() -> StatusOr<std::unique_ptr<ObjectReadSource>> {
    ++*calls;
    // Assign error statuses by executing thread ID so the hedge attempt
    // deterministically returns `kNotFound` and the primary attempt returns
    // `kUnavailable`.
    if (!OnPrimaryThread(primary_thread)) {
      hedge_started.Signal();
      return Status(StatusCode::kNotFound, "hedge error");
    }
    auto mock = std::make_unique<MockObjectReadSource>();
    EXPECT_CALL(*mock, Read).WillOnce([hedge_started](char*, std::size_t) {
      // Outlive the hedge dispatch, otherwise the primary error is reported
      // before there is a hedge error to lose to it.
      hedge_started.Wait();
      return StatusOr<ReadSourceResult>(
          Status(StatusCode::kUnavailable, "retry policy exhausted"));
    });
    // The failed child is still open, e.g. a permanent HTTP error, so the
    // race must close it.
    EXPECT_CALL(*mock, IsOpen).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock, Close)
        .WillOnce(
            Return(make_status_or(HttpResponse{HttpStatusCode::kOk, {}, {}})));
    return std::unique_ptr<ObjectReadSource>(std::move(mock));
  };

  HedgedObjectReadSource source(read_pool.pool(), MakeUnlimitedHedgePool(),
                                Adapt(factory), kDelay, /*max_hedges=*/1,
                                kUnlimitedBuffer);

  std::vector<char> buffer(100);
  auto result = source.Read(buffer.data(), buffer.size());
  EXPECT_THAT(result, StatusIs(StatusCode::kUnavailable));
  EXPECT_THAT(calls->load(), Eq(2));
  EXPECT_FALSE(source.IsOpen());
}

TEST(HedgedObjectReadSourceTest, CloseWithoutReadSucceeds) {
  auto factory = []() -> StatusOr<std::unique_ptr<ObjectReadSource>> {
    return Status(StatusCode::kUnimplemented, "never called");
  };
  HedgedObjectReadSource source(MakeUnlimitedReadPool(),
                                MakeUnlimitedHedgePool(), Adapt(factory),
                                kLongDelay, /*max_hedges=*/2, kUnlimitedBuffer);
  EXPECT_TRUE(source.IsOpen());
  EXPECT_THAT(source.Close(), IsOk());
}

TEST(HedgedObjectReadSourceTest, CloseBeforeRead) {
  auto read_pool = std::make_shared<ThreadPool>(1);
  auto hedge_pool = std::make_shared<HedgingThreadPool>(1, 0.0, 0.0, 0);
  auto factory = []() {
    return std::unique_ptr<ObjectReadSource>(
        std::make_unique<MockObjectReadSource>());
  };
  HedgedObjectReadSource source(read_pool, hedge_pool, Adapt(factory),
                                std::chrono::milliseconds(10), 2,
                                kUnlimitedBuffer);
  EXPECT_TRUE(source.IsOpen());
  EXPECT_THAT(source.Close(), IsOk());
  EXPECT_FALSE(source.IsOpen());
  auto const res = source.Read(nullptr, 1024);
  EXPECT_THAT(res, IsOk());
  EXPECT_THAT(res->bytes_received, Eq(0));
}

TEST(HedgedObjectReadSourceTest, OversizedReadIsNotHedged) {
  // A read larger than the limit must open exactly one child and read into the
  // caller's buffer, with no racing attempts to stage copies of the data.
  auto calls = std::make_shared<std::atomic<int>>(0);
  auto factory = [calls]() -> StatusOr<std::unique_ptr<ObjectReadSource>> {
    ++*calls;
    auto mock = std::make_unique<MockObjectReadSource>();
    EXPECT_CALL(*mock, Read).WillOnce(ImmediateRead("direct"));
    return std::unique_ptr<ObjectReadSource>(std::move(mock));
  };

  // A zero delay would let a hedge start immediately if the limit were not
  // honored, so any race would be observable as extra factory calls.
  HedgedObjectReadSource source(MakeUnlimitedReadPool(),
                                MakeUnlimitedHedgePool(), Adapt(factory),
                                std::chrono::milliseconds(0),
                                /*max_hedges=*/2, /*max_buffer=*/8);

  std::vector<char> buffer(64);
  auto result = source.Read(buffer.data(), buffer.size());
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(result->bytes_received, Eq(6));
  EXPECT_THAT(std::string(buffer.data(), result->bytes_received), Eq("direct"));
  EXPECT_THAT(calls->load(), Eq(1));
}

TEST(HedgedObjectReadSourceTest, OversizedReadPropagatesOpenError) {
  auto factory = []() -> StatusOr<std::unique_ptr<ObjectReadSource>> {
    return Status(StatusCode::kPermissionDenied, "uh-oh");
  };

  HedgedObjectReadSource source(MakeUnlimitedReadPool(),
                                MakeUnlimitedHedgePool(), Adapt(factory),
                                std::chrono::milliseconds(0),
                                /*max_hedges=*/2, /*max_buffer=*/8);

  std::vector<char> buffer(64);
  EXPECT_THAT(source.Read(buffer.data(), buffer.size()),
              StatusIs(StatusCode::kPermissionDenied));
  EXPECT_FALSE(source.IsOpen());
}

TEST(HedgedObjectReadSourceTest, OversizedMidStreamReadIsNotHedged) {
  // The buffer limit applies to every read, not only to the open. Reads 1 and
  // 2 are within the limit and answered promptly, so neither dispatches a
  // hedge. Read 3 is well past the limit and slow: were the limit not honored
  // it would be raced and would dispatch a hedge, opening a second child.
  auto calls = std::make_shared<std::atomic<int>>(0);
  auto factory = [calls]() -> StatusOr<std::unique_ptr<ObjectReadSource>> {
    ++*calls;
    auto mock = std::make_unique<MockObjectReadSource>();
    EXPECT_CALL(*mock, Read)
        .WillOnce(ImmediateRead("open"))
        .WillOnce(ImmediateRead("small"))
        .WillOnce(DelayedRead("large", kStall));
    return std::unique_ptr<ObjectReadSource>(std::move(mock));
  };

  HedgedObjectReadSource source(MakeUnlimitedReadPool(),
                                MakeUnlimitedHedgePool(), Adapt(factory),
                                kDelay, /*max_hedges=*/2, /*max_buffer=*/64);

  std::vector<char> small(8);
  EXPECT_THAT(source.Read(small.data(), small.size()), IsOk());
  EXPECT_THAT(source.Read(small.data(), small.size()), IsOk());
  std::vector<char> large(4096);
  EXPECT_THAT(source.Read(large.data(), large.size()), IsOk());
  EXPECT_THAT(calls->load(), Eq(1));
}

TEST(HedgedObjectReadSourceTest, SubsequentReadHedgeWinsWhenPrimaryStalls) {
  // Read 1 opens the stream and read 2 is answered promptly. Every read is
  // raced, so when the primary blocks on read 3 a hedge is opened at the
  // current offset, wins, and serves the rest of the stream.
  auto unblock_primary = std::make_shared<std::promise<void>>();
  auto primary_closed = std::make_shared<std::promise<void>>();
  auto recorded_offset = std::make_shared<std::atomic<std::int64_t>>(-1);
  auto factory_calls = std::make_shared<std::atomic<int>>(0);

  auto factory = [unblock_primary, primary_closed, recorded_offset,
                  factory_calls](std::int64_t offset,
                                 std::optional<std::int64_t>)
      -> StatusOr<std::unique_ptr<ObjectReadSource>> {
    auto mock = std::make_unique<MockObjectReadSource>();
    if (++*factory_calls == 1) {
      EXPECT_CALL(*mock, Read)
          .WillOnce(ImmediateRead("chunk-1"))
          .WillOnce(ImmediateRead("chunk-2"))
          .WillOnce(BlockedRead(unblock_primary, "chunk-3-slow"));
      EXPECT_CALL(*mock, Close).WillOnce(NotifyClose(primary_closed));
    } else {
      recorded_offset->store(offset);
      EXPECT_CALL(*mock, Read)
          .WillOnce(ImmediateRead("chunk-3-hedge"))
          .WillOnce(ImmediateRead("chunk-4"));
    }
    return std::unique_ptr<ObjectReadSource>(std::move(mock));
  };

  HedgedObjectReadSource source(MakeUnlimitedReadPool(),
                                MakeUnlimitedHedgePool(), factory, kDelay,
                                /*max_hedges=*/1, kUnlimitedBuffer);

  std::vector<char> buffer(100);
  for (auto const* expected : {"chunk-1", "chunk-2"}) {
    auto result = source.Read(buffer.data(), buffer.size());
    ASSERT_THAT(result, IsOk());
    EXPECT_THAT(std::string(buffer.data(), result->bytes_received),
                Eq(expected));
  }

  auto r3 = source.Read(buffer.data(), buffer.size());
  ASSERT_THAT(r3, IsOk());
  EXPECT_THAT(std::string(buffer.data(), r3->bytes_received),
              Eq("chunk-3-hedge"));
  EXPECT_THAT(recorded_offset->load(), Eq(14));

  unblock_primary->set_value();
  primary_closed->get_future().get();

  auto r4 = source.Read(buffer.data(), buffer.size());
  ASSERT_THAT(r4, IsOk());
  EXPECT_THAT(std::string(buffer.data(), r4->bytes_received), Eq("chunk-4"));
  EXPECT_THAT(factory_calls->load(), Eq(2));
}

TEST(HedgedObjectReadSourceTest, FirstStallAfterCleanOpenIsHedged) {
  // A stream can open promptly and then stall on its very first body read.
  // Racing is not conditioned on an earlier read having stalled, so this read
  // is raced and the hedge rescues it. Gating on a previous stall would leave
  // this read to run to completion at full cost, since nothing before it was
  // slow.
  auto unblock_primary = std::make_shared<std::promise<void>>();
  auto primary_closed = std::make_shared<std::promise<void>>();
  auto read_returned = std::make_shared<std::promise<void>>();
  auto factory_calls = std::make_shared<std::atomic<int>>(0);

  auto factory =
      [unblock_primary, primary_closed,
       factory_calls]() -> StatusOr<std::unique_ptr<ObjectReadSource>> {
    auto mock = std::make_unique<MockObjectReadSource>();
    if (++*factory_calls == 1) {
      // The open is healthy; the first body read then blocks.
      EXPECT_CALL(*mock, Read)
          .WillOnce(ImmediateRead("chunk-1"))
          .WillOnce(BlockedRead(unblock_primary, "chunk-2-slow"));
      EXPECT_CALL(*mock, Close).WillOnce(NotifyClose(primary_closed));
    } else {
      EXPECT_CALL(*mock, Read).WillOnce(ImmediateRead("chunk-2-hedge"));
    }
    return std::unique_ptr<ObjectReadSource>(std::move(mock));
  };

  HedgedObjectReadSource source(MakeUnlimitedReadPool(),
                                MakeUnlimitedHedgePool(), Adapt(factory),
                                kDelay, /*max_hedges=*/1, kUnlimitedBuffer);

  std::vector<char> buffer(100);
  StatusOr<ReadSourceResult> r1 = source.Read(buffer.data(), buffer.size());
  ASSERT_THAT(r1, IsOk());
  EXPECT_THAT(std::string(buffer.data(), r1->bytes_received), Eq("chunk-1"));

  // Release the primary only after the read has returned, so the hedge wins
  // regardless of scheduling. The bounded wait means a regression that never
  // dispatches the hedge fails the assertions below rather than deadlocking.
  std::thread unblocker([unblock_primary, read_returned] {
    read_returned->get_future().wait_for(std::chrono::seconds(10));
    unblock_primary->set_value();
  });

  StatusOr<ReadSourceResult> r2 = source.Read(buffer.data(), buffer.size());
  read_returned->set_value();
  unblocker.join();

  ASSERT_THAT(r2, IsOk());
  EXPECT_THAT(std::string(buffer.data(), r2->bytes_received),
              Eq("chunk-2-hedge"));
  EXPECT_THAT(factory_calls->load(), Eq(2));

  // The losing primary is closed off the caller's thread. Bound this wait for
  // the same reason as the one above: if no hedge was dispatched there is no
  // primary to lose, and an unbounded wait would hang the test instead of
  // reporting the assertions that already failed.
  EXPECT_THAT(primary_closed->get_future().wait_for(std::chrono::seconds(10)),
              Eq(std::future_status::ready));
}

TEST(HedgedObjectReadSourceTest,
     PromptReadsAfterHedgeWinOpenNoFurtherChildren) {
  // Racing a read is not the same as hedging it. Read 3 blocks, so a hedge is
  // dispatched and wins. Reads 4 and 5 are raced as well, but they answer well
  // inside the delay, so no hedge is dispatched for them and no third child is
  // ever opened.
  auto unblock_primary = std::make_shared<std::promise<void>>();
  auto primary_closed = std::make_shared<std::promise<void>>();
  auto factory_calls = std::make_shared<std::atomic<int>>(0);

  auto factory =
      [unblock_primary, primary_closed,
       factory_calls]() -> StatusOr<std::unique_ptr<ObjectReadSource>> {
    auto mock = std::make_unique<MockObjectReadSource>();
    if (++*factory_calls == 1) {
      EXPECT_CALL(*mock, Read)
          .WillOnce(ImmediateRead("chunk-1"))
          .WillOnce(ImmediateRead("chunk-2"))
          .WillOnce(BlockedRead(unblock_primary, "chunk-3-slow"));
      EXPECT_CALL(*mock, Close).WillOnce(NotifyClose(primary_closed));
    } else {
      EXPECT_CALL(*mock, Read)
          .WillOnce(ImmediateRead("chunk-3-hedge"))
          .WillOnce(ImmediateRead("chunk-4"))
          .WillOnce(ImmediateRead("chunk-5"));
    }
    return std::unique_ptr<ObjectReadSource>(std::move(mock));
  };

  HedgedObjectReadSource source(MakeUnlimitedReadPool(),
                                MakeUnlimitedHedgePool(), Adapt(factory),
                                kDelay, /*max_hedges=*/1, kUnlimitedBuffer);

  std::vector<char> buffer(100);
  for (auto const* expected :
       {"chunk-1", "chunk-2", "chunk-3-hedge", "chunk-4", "chunk-5"}) {
    auto result = source.Read(buffer.data(), buffer.size());
    ASSERT_THAT(result, IsOk());
    EXPECT_THAT(std::string(buffer.data(), result->bytes_received),
                Eq(expected));
  }
  EXPECT_THAT(factory_calls->load(), Eq(2));

  unblock_primary->set_value();
  primary_closed->get_future().get();
}

TEST(HedgedObjectReadSourceTest, SubsequentReadPinsGeneration) {
  auto unblock_primary = std::make_shared<std::promise<void>>();
  auto primary_closed = std::make_shared<std::promise<void>>();
  auto recorded_gen = std::make_shared<std::atomic<std::int64_t>>(-1);
  auto factory_calls = std::make_shared<std::atomic<int>>(0);

  auto factory = [unblock_primary, primary_closed, recorded_gen, factory_calls](
                     std::int64_t, std::optional<std::int64_t> generation)
      -> StatusOr<std::unique_ptr<ObjectReadSource>> {
    auto mock = std::make_unique<MockObjectReadSource>();
    if (++*factory_calls == 1) {
      EXPECT_CALL(*mock, Read)
          .WillOnce([](char* buf, std::size_t) {
            std::string const payload = "chunk-1";
            std::copy(payload.begin(), payload.end(), buf);
            auto r = MakeReadResult(payload);
            r.generation = 987654321;
            return r;
          })
          .WillOnce(ImmediateRead("chunk-2"))
          .WillOnce(BlockedRead(unblock_primary, "chunk-3-slow"));
      EXPECT_CALL(*mock, Close).WillOnce(NotifyClose(primary_closed));
    } else {
      if (generation) recorded_gen->store(*generation);
      EXPECT_CALL(*mock, Read).WillOnce(ImmediateRead("chunk-3-hedge"));
    }
    return std::unique_ptr<ObjectReadSource>(std::move(mock));
  };

  HedgedObjectReadSource source(MakeUnlimitedReadPool(),
                                MakeUnlimitedHedgePool(), factory, kDelay,
                                /*max_hedges=*/1, kUnlimitedBuffer);

  std::vector<char> buffer(100);
  ASSERT_THAT(source.Read(buffer.data(), buffer.size()), IsOk());
  ASSERT_THAT(source.Read(buffer.data(), buffer.size()), IsOk());
  ASSERT_THAT(source.Read(buffer.data(), buffer.size()), IsOk());
  EXPECT_THAT(recorded_gen->load(), Eq(987654321));

  unblock_primary->set_value();
  primary_closed->get_future().get();
}

TEST(HedgedObjectReadSourceTest, SubsequentReadGunzippedBypassesHedging) {
  // Read 1 discovers decompressive transcoding. Read 2 stalls and would
  // otherwise be raced, but under transcoding a hedge cannot resume at an
  // offset, so it must continue directly on the active child.
  auto factory_calls = std::make_shared<std::atomic<int>>(0);
  auto factory =
      [factory_calls]() -> StatusOr<std::unique_ptr<ObjectReadSource>> {
    ++*factory_calls;
    auto mock = std::make_unique<MockObjectReadSource>();
    EXPECT_CALL(*mock, Read)
        .WillOnce([](char* buf, std::size_t) {
          std::string const payload = "chunk-1";
          std::copy(payload.begin(), payload.end(), buf);
          auto r = MakeReadResult(payload);
          r.transformation = "gunzipped";
          return r;
        })
        .WillOnce(DelayedRead("chunk-2", kStall))
        .WillOnce(ImmediateRead("chunk-3"));
    return std::unique_ptr<ObjectReadSource>(std::move(mock));
  };

  HedgedObjectReadSource source(MakeUnlimitedReadPool(),
                                MakeUnlimitedHedgePool(), Adapt(factory),
                                kDelay, /*max_hedges=*/2, kUnlimitedBuffer);

  std::vector<char> buffer(100);
  ASSERT_THAT(source.Read(buffer.data(), buffer.size()), IsOk());
  ASSERT_THAT(source.Read(buffer.data(), buffer.size()), IsOk());
  auto r3 = source.Read(buffer.data(), buffer.size());
  ASSERT_THAT(r3, IsOk());
  EXPECT_THAT(std::string(buffer.data(), r3->bytes_received), Eq("chunk-3"));
  EXPECT_THAT(factory_calls->load(), Eq(1));
}

TEST(HedgedObjectReadSourceTest,
     SubsequentReadHedgeFailureDoesNotAbortPrimary) {
  auto unblock_primary = std::make_shared<std::promise<void>>();
  auto hedge_attempted = std::make_shared<std::promise<void>>();
  auto hedge_signalled = std::make_shared<std::atomic<bool>>(false);
  auto factory_calls = std::make_shared<std::atomic<int>>(0);

  auto factory =
      [unblock_primary, hedge_attempted, hedge_signalled,
       factory_calls]() -> StatusOr<std::unique_ptr<ObjectReadSource>> {
    if (++*factory_calls != 1) {
      // Tell the test the hedge has been dispatched. Guarded because setting a
      // promise twice throws, and only the first hedge needs to be observed.
      if (!hedge_signalled->exchange(true)) hedge_attempted->set_value();
      return Status(StatusCode::kUnavailable, "hedge open error");
    }
    auto mock = std::make_unique<MockObjectReadSource>();
    EXPECT_CALL(*mock, Read)
        .WillOnce(ImmediateRead("chunk-1"))
        .WillOnce(ImmediateRead("chunk-2"))
        .WillOnce(BlockedRead(unblock_primary, "chunk-3-primary"));
    return std::unique_ptr<ObjectReadSource>(std::move(mock));
  };

  HedgedObjectReadSource source(MakeUnlimitedReadPool(),
                                MakeUnlimitedHedgePool(), Adapt(factory),
                                kDelay, /*max_hedges=*/1, kUnlimitedBuffer);

  std::vector<char> buffer(100);
  ASSERT_THAT(source.Read(buffer.data(), buffer.size()), IsOk());
  ASSERT_THAT(source.Read(buffer.data(), buffer.size()), IsOk());

  // Release the primary only once the hedge has actually been attempted.
  // Sleeping instead would race the hedge dispatch against an unrelated clock,
  // and on a slow machine the primary could finish before the hedge is ever
  // dispatched. The bounded wait makes a regression fail the assertions below
  // rather than hang the test.
  std::thread unblocker([unblock_primary, hedge_attempted] {
    hedge_attempted->get_future().wait_for(std::chrono::seconds(10));
    unblock_primary->set_value();
  });

  auto r3 = source.Read(buffer.data(), buffer.size());
  unblocker.join();

  ASSERT_THAT(r3, IsOk());
  EXPECT_THAT(std::string(buffer.data(), r3->bytes_received),
              Eq("chunk-3-primary"));
  EXPECT_THAT(factory_calls->load(), Eq(2));
}

// Returns a factory whose first child answers @p result twice promptly and
// then blocks, and whose second child records the offset it was opened at and
// answers "hedge". Every read is raced, so the hedge is dispatched on the
// blocked third read.
auto MakeOffsetRecordingFactory(
    ReadSourceResult result,
    std::shared_ptr<std::promise<void>> const& unblock_primary,
    std::shared_ptr<std::promise<void>> const& primary_closed,
    std::shared_ptr<std::atomic<std::int64_t>> const& recorded_offset,
    std::shared_ptr<std::atomic<int>> const& factory_calls) {
  return [result = std::move(result), unblock_primary, primary_closed,
          recorded_offset,
          factory_calls](std::int64_t offset, std::optional<std::int64_t>)
             -> StatusOr<std::unique_ptr<ObjectReadSource>> {
    auto mock = std::make_unique<MockObjectReadSource>();
    if (++*factory_calls == 1) {
      EXPECT_CALL(*mock, Read)
          .WillOnce([result](char* buf, std::size_t) {
            std::fill(buf, buf + result.bytes_received, 'x');
            return result;
          })
          .WillOnce([result](char* buf, std::size_t) {
            std::fill(buf, buf + result.bytes_received, 'x');
            return result;
          })
          .WillOnce(BlockedRead(unblock_primary, "slow"));
      EXPECT_CALL(*mock, Close).WillOnce(NotifyClose(primary_closed));
    } else {
      recorded_offset->store(offset);
      EXPECT_CALL(*mock, Read).WillOnce(ImmediateRead("hedge"));
    }
    return std::unique_ptr<ObjectReadSource>(std::move(mock));
  };
}

TEST(HedgedObjectReadSourceTest, SubsequentReadFromEndTracksOffset) {
  auto unblock_primary = std::make_shared<std::promise<void>>();
  auto primary_closed = std::make_shared<std::promise<void>>();
  auto recorded_offset = std::make_shared<std::atomic<std::int64_t>>(-1);
  auto factory_calls = std::make_shared<std::atomic<int>>(0);
  auto factory = MakeOffsetRecordingFactory(MakeReadResult("1234567890"),
                                            unblock_primary, primary_closed,
                                            recorded_offset, factory_calls);

  HedgedObjectReadSource::Position position;
  position.offset = 100;
  position.direction = kFromEnd;
  HedgedObjectReadSource source(MakeUnlimitedReadPool(),
                                MakeUnlimitedHedgePool(), factory, kDelay,
                                /*max_hedges=*/1, kUnlimitedBuffer, position);

  std::vector<char> buffer(100);
  auto r1 = source.Read(buffer.data(), buffer.size());
  ASSERT_THAT(r1, IsOk());
  EXPECT_THAT(r1->bytes_received, Eq(10));
  ASSERT_THAT(source.Read(buffer.data(), buffer.size()), IsOk());

  auto r3 = source.Read(buffer.data(), buffer.size());
  ASSERT_THAT(r3, IsOk());
  EXPECT_THAT(recorded_offset->load(), Eq(80));

  unblock_primary->set_value();
  primary_closed->get_future().get();
}

TEST(HedgedObjectReadSourceTest, ReadLastLargerThanObjectClampsOffset) {
  // `ReadLast(100)` on a 40 byte object returns the whole object. After 20
  // bytes, 20 remain: a hedge asking for the last 80 bytes would receive the
  // whole object again, from the first byte.
  auto unblock_primary = std::make_shared<std::promise<void>>();
  auto primary_closed = std::make_shared<std::promise<void>>();
  auto recorded_offset = std::make_shared<std::atomic<std::int64_t>>(-1);
  auto factory_calls = std::make_shared<std::atomic<int>>(0);
  auto first = MakeReadResult("1234567890");
  first.size = 40;
  auto factory = MakeOffsetRecordingFactory(
      first, unblock_primary, primary_closed, recorded_offset, factory_calls);

  HedgedObjectReadSource::Position position;
  position.offset = 100;
  position.direction = kFromEnd;
  HedgedObjectReadSource source(MakeUnlimitedReadPool(),
                                MakeUnlimitedHedgePool(), factory, kDelay,
                                /*max_hedges=*/1, kUnlimitedBuffer, position);

  std::vector<char> buffer(100);
  ASSERT_THAT(source.Read(buffer.data(), buffer.size()), IsOk());
  ASSERT_THAT(source.Read(buffer.data(), buffer.size()), IsOk());
  ASSERT_THAT(source.Read(buffer.data(), buffer.size()), IsOk());
  EXPECT_THAT(recorded_offset->load(), Eq(20));

  unblock_primary->set_value();
  primary_closed->get_future().get();
}

// Verifies that a stream is *not* raced once it has reached the end of the
// requested data: the drain read at the end must go to the active child, a
// hedge would request an empty or inverted range. The child answers @p chunk
// twice, reaching the end of the data, then answers the drain read slowly. The
// drain is slow enough that a hedge would be dispatched for it were the
// end-of-data guard not honored, which would open a second child.
void ExpectNoRaceAtEnd(HedgedObjectReadSource::Position position,
                       ReadSourceResult chunk) {
  auto factory_calls = std::make_shared<std::atomic<int>>(0);
  auto factory = [factory_calls, chunk = std::move(chunk)]()
      -> StatusOr<std::unique_ptr<ObjectReadSource>> {
    ++*factory_calls;
    auto mock = std::make_unique<MockObjectReadSource>();
    EXPECT_CALL(*mock, Read)
        .WillOnce([chunk](char* buf, std::size_t) {
          std::fill(buf, buf + chunk.bytes_received, 'x');
          return chunk;
        })
        .WillOnce([chunk](char* buf, std::size_t) {
          std::fill(buf, buf + chunk.bytes_received, 'x');
          return chunk;
        })
        .WillOnce(DelayedRead("", kStall));
    return std::unique_ptr<ObjectReadSource>(std::move(mock));
  };

  HedgedObjectReadSource source(
      MakeUnlimitedReadPool(), MakeUnlimitedHedgePool(), Adapt(factory), kDelay,
      /*max_hedges=*/2, kUnlimitedBuffer, position);

  std::vector<char> buffer(100);
  ASSERT_THAT(source.Read(buffer.data(), buffer.size()), IsOk());
  ASSERT_THAT(source.Read(buffer.data(), buffer.size()), IsOk());
  auto drain = source.Read(buffer.data(), buffer.size());
  ASSERT_THAT(drain, IsOk());
  EXPECT_THAT(drain->bytes_received, Eq(0));
  EXPECT_THAT(factory_calls->load(), Eq(1));
}

TEST(HedgedObjectReadSourceTest, NoRaceAtObjectSize) {
  auto chunk = MakeReadResult("12345");
  chunk.size = 10;
  ExpectNoRaceAtEnd(HedgedObjectReadSource::Position{}, chunk);
}

TEST(HedgedObjectReadSourceTest, NoRaceAtRangeEnd) {
  HedgedObjectReadSource::Position position;
  position.offset = 5;
  position.end_offset = 15;
  auto chunk = MakeReadResult("12345");
  chunk.size = 1000;
  ExpectNoRaceAtEnd(position, chunk);
}

TEST(HedgedObjectReadSourceTest, NoRaceAtReadLastEnd) {
  HedgedObjectReadSource::Position position;
  position.offset = 10;
  position.direction = kFromEnd;
  ExpectNoRaceAtEnd(position, MakeReadResult("12345"));
}

TEST(HedgedObjectReadSourceTest, RangeEndTakesPrecedenceOverResponseSize) {
  // On the REST path `ReadSourceResult::size` falls back to `content-length`,
  // which is the length of that response rather than the size of the object.
  // For a ranged read that value is far below the stream's offset, and must
  // not be mistaken for the end of the requested data: the range end wins.
  HedgedObjectReadSource::Position position;
  position.offset = 1000;
  position.end_offset = 1100;

  auto unblock_primary = std::make_shared<std::promise<void>>();
  auto primary_closed = std::make_shared<std::promise<void>>();
  auto factory_calls = std::make_shared<std::atomic<int>>(0);
  auto factory =
      [unblock_primary, primary_closed,
       factory_calls]() -> StatusOr<std::unique_ptr<ObjectReadSource>> {
    auto mock = std::make_unique<MockObjectReadSource>();
    if (++*factory_calls != 1) {
      EXPECT_CALL(*mock, Read).WillOnce(ImmediateRead("chunk-3-hedge"));
      return std::unique_ptr<ObjectReadSource>(std::move(mock));
    }
    // `size` is this response's content-length, well below `position.offset`.
    ReadSourceResult chunk = MakeReadResult("chunk-1");
    chunk.size = 7;
    EXPECT_CALL(*mock, Read)
        .WillOnce([chunk](char* buf, std::size_t n) {
          EXPECT_LE(chunk.bytes_received, n);
          std::fill(buf, buf + chunk.bytes_received, 'x');
          return chunk;
        })
        .WillOnce(ImmediateRead("chunk-2"))
        .WillOnce(BlockedRead(unblock_primary, "chunk-3-slow"));
    EXPECT_CALL(*mock, Close).WillOnce(NotifyClose(primary_closed));
    return std::unique_ptr<ObjectReadSource>(std::move(mock));
  };

  HedgedObjectReadSource source(
      MakeUnlimitedReadPool(), MakeUnlimitedHedgePool(), Adapt(factory), kDelay,
      /*max_hedges=*/1, kUnlimitedBuffer, position);

  std::vector<char> buffer(100);
  // Read 1 opens the stream, read 2 is slow and marks it stalled.
  ASSERT_THAT(source.Read(buffer.data(), buffer.size()), IsOk());
  ASSERT_THAT(source.Read(buffer.data(), buffer.size()), IsOk());

  // Read 3 must still be raced. The stream is 1014 bytes in and the range ends
  // at 1100, so it has not reached the end of the requested data, even though
  // the reported `size` of 7 is long behind it.
  auto r3 = source.Read(buffer.data(), buffer.size());
  ASSERT_THAT(r3, IsOk());
  EXPECT_THAT(std::string(buffer.data(), r3->bytes_received),
              Eq("chunk-3-hedge"));
  EXPECT_THAT(factory_calls->load(), Eq(2));

  unblock_primary->set_value();
  WaitForSignal(primary_closed);
}

TEST(HedgedObjectReadSourceTest, HedgesAreBoundedPerStream) {
  // Every read on this stream is slow enough to look stalled, so without a
  // per-stream budget the source would race, and hedge, forever. The hedges
  // all fail so the primary always wins and stays the active child.
  auto constexpr kShortDelay = std::chrono::milliseconds(5);
  auto constexpr kSlowRead = std::chrono::milliseconds(20);
  int constexpr kMaxHedges = 1;
  int constexpr kReads = 20;
  // One call opens the primary, the rest are hedges. `ShouldRace()` stops
  // racing once the stream has spent `max_hedges * kMaxHedgeRoundsPerStream`.
  int constexpr kExpectedCalls = 1 + kMaxHedges * 8;

  auto mu = std::make_shared<std::mutex>();
  auto cv = std::make_shared<std::condition_variable>();
  auto factory_calls = std::make_shared<int>(0);
  auto raced_rounds = std::make_shared<int>(0);
  auto factory =
      [mu, cv, factory_calls, raced_rounds, kSlowRead,
       expected_calls =
           kExpectedCalls]() -> StatusOr<std::unique_ptr<ObjectReadSource>> {
    int calls = 0;
    {
      std::lock_guard<std::mutex> lock(*mu);
      calls = ++*factory_calls;
    }
    cv->notify_all();
    if (calls != 1) return Status(StatusCode::kUnavailable, "hedge");
    auto mock = std::make_unique<MockObjectReadSource>();
    EXPECT_CALL(*mock, Read)
        .WillRepeatedly([mu, cv, factory_calls, raced_rounds, kSlowRead,
                         expected_calls](char* buf, std::size_t n) {
          // While the per-stream hedge budget remains, each raced primary read
          // waits until its hedge has actually entered `factory` before
          // completing, so a slow CI scheduler cannot let the primary finish
          // ahead of `future.wait_for(kShortDelay)`.
          int const target_calls = 1 + ++*raced_rounds;
          if (target_calls <= expected_calls) {
            std::unique_lock<std::mutex> lock(*mu);
            EXPECT_TRUE(cv->wait_for(lock, std::chrono::seconds(10), [&] {
              return *factory_calls >= target_calls;
            }));
          }
          return DelayedRead("chunk", kSlowRead)(buf, n);
        });
    return std::unique_ptr<ObjectReadSource>(std::move(mock));
  };

  HedgedObjectReadSource source(MakeUnlimitedReadPool(),
                                MakeUnlimitedHedgePool(), Adapt(factory),
                                kShortDelay, kMaxHedges, kUnlimitedBuffer);

  std::vector<char> buffer(100);
  for (int i = 0; i != kReads; ++i) {
    ASSERT_THAT(source.Read(buffer.data(), buffer.size()), IsOk()) << "i=" << i;
  }

  std::lock_guard<std::mutex> lock(*mu);
  EXPECT_THAT(*factory_calls, Eq(kExpectedCalls));
}

TEST(HedgedObjectReadSourceTest, DirectReadFailureClosesStream) {
  // A direct read that fails must tear the stream down the same way a raced
  // read does, otherwise `Close()` would reach a child that has already
  // released its connection.
  auto factory = []() -> StatusOr<std::unique_ptr<ObjectReadSource>> {
    auto mock = std::make_unique<MockObjectReadSource>();
    EXPECT_CALL(*mock, Read)
        .WillOnce(ImmediateRead("payload"))
        .WillOnce(Return(StatusOr<ReadSourceResult>(
            Status(StatusCode::kUnavailable, "retry policy exhausted"))));
    EXPECT_CALL(*mock, IsOpen).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock, Close)
        .WillOnce(
            Return(make_status_or(HttpResponse{HttpStatusCode::kOk, {}, {}})));
    return std::unique_ptr<ObjectReadSource>(std::move(mock));
  };

  // `kLongDelay` keeps the open race from dispatching a hedge, and the fast
  // first read leaves the stream looking healthy, so the second read is
  // direct.
  HedgedObjectReadSource source(MakeUnlimitedReadPool(),
                                MakeUnlimitedHedgePool(), Adapt(factory),
                                kLongDelay, /*max_hedges=*/1, kUnlimitedBuffer);

  std::vector<char> buffer(100);
  ASSERT_THAT(source.Read(buffer.data(), buffer.size()), IsOk());
  EXPECT_THAT(source.Read(buffer.data(), buffer.size()),
              StatusIs(StatusCode::kUnavailable));
  EXPECT_FALSE(source.IsOpen());
  EXPECT_THAT(source.Close(), IsOk());
}

TEST(HedgedObjectReadSourceTest,
     LosingHedgeInFlightOnDestructionDoesNotLeakOrCrash) {
  // Verify that destroying `HedgedObjectReadSource` (and dropping the last
  // external `std::shared_ptr<HedgingThreadPool>`) while a losing hedge is
  // still executing `Read()` cleanly joins the worker thread before returning.
  PrimaryReadPool read_pool;
  std::thread::id const primary_thread = read_pool.worker_id();

  auto unblock_primary = std::make_shared<std::promise<void>>();
  auto primary_closed = std::make_shared<std::promise<void>>();
  // Signaled once the second (losing) hedge enters `Read()`, ensuring the
  // first hedge does not win the race before the second hedge is dispatched.
  auto loser_started = std::make_shared<std::promise<void>>();
  // Signaled once the primary attempt has been fully torn down, releasing the
  // losing hedge to enter its bounded stall. Gating the stall on primary
  // teardown keeps the stall duration from racing teardown latency, which is
  // unbounded under sanitizers and on loaded machines.
  auto release_loser = std::make_shared<std::promise<void>>();
  // Signaled once the losing hedge is actively executing `Read()`, ensuring
  // `source` is destroyed while the losing hedge task is in flight.
  auto loser_in_read = std::make_shared<std::promise<void>>();
  auto hedges = std::make_shared<std::atomic<int>>(0);

  auto factory =
      [unblock_primary, primary_closed, loser_started, release_loser,
       loser_in_read, hedges,
       primary_thread]() -> StatusOr<std::unique_ptr<ObjectReadSource>> {
    auto mock = std::make_unique<MockObjectReadSource>();
    if (OnPrimaryThread(primary_thread)) {
      // Primary attempt: blocks until unblocked after the race concludes.
      EXPECT_CALL(*mock, Read).WillOnce(BlockedRead(unblock_primary, "slow"));
      EXPECT_CALL(*mock, Close).WillOnce(NotifyClose(primary_closed));
      return std::unique_ptr<ObjectReadSource>(std::move(mock));
    }

    // Both hedges execute on `hedge_pool_`: the first hedge waits for the
    // second hedge to start and then wins the race, while the second hedge
    // remains in `Read()` during `~HedgedObjectReadSource()`.
    if (++*hedges == 1) {
      // Wait for the second hedge to enter `Read()` before completing.
      EXPECT_CALL(*mock, Read)
          .WillOnce([loser_started](char* buf, std::size_t) {
            WaitForSignal(loser_started);
            std::string const payload = "winner";
            std::copy(payload.begin(), payload.end(), buf);
            return MakeReadResult(payload);
          });
    } else {
      EXPECT_CALL(*mock, Read)
          .WillOnce([loser_started, release_loser, loser_in_read](char* buf,
                                                                  std::size_t) {
            loser_started->set_value();
            // Park until the primary attempt has been unblocked and closed.
            // Stalling before that teardown completes would race the fixed
            // delay below against teardown latency, letting the stall elapse
            // early and leaving no hedge in flight for the destructor to join.
            WaitForSignal(release_loser);
            loser_in_read->set_value();
            // Hold `Read()` after signaling `loser_in_read` so the worker
            // thread is still executing `Read()` when the test exits the scope
            // below and invokes `~HedgingThreadPool()`.
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::string const payload = "loser";
            std::copy(payload.begin(), payload.end(), buf);
            return MakeReadResult(payload);
          });
    }
    // The losing hedge is closed by `RunAttempt()` after the winner claims the
    // race.
    EXPECT_CALL(*mock, Close)
        .Times(AtMost(1))
        .WillRepeatedly(
            Return(make_status_or(HttpResponse{HttpStatusCode::kOk, {}, {}})));
    return std::unique_ptr<ObjectReadSource>(std::move(mock));
  };

  {
    HedgedObjectReadSource source(read_pool.pool(), MakeUnlimitedHedgePool(),
                                  Adapt(factory), std::chrono::milliseconds(1),
                                  /*max_hedges=*/2, kUnlimitedBuffer);

    std::vector<char> buffer(100);
    StatusOr<ReadSourceResult> result =
        source.Read(buffer.data(), buffer.size());
    ASSERT_THAT(result, IsOk());
    EXPECT_THAT(std::string(buffer.data(), result->bytes_received),
                Eq("winner"));
    EXPECT_THAT(hedges->load(), Eq(2));

    // Tear down the primary attempt first. All of this latency is absorbed
    // while the losing hedge is parked, so it cannot eat into the stall below.
    unblock_primary->set_value();
    WaitForSignal(primary_closed);

    // Release the losing hedge and wait for it to enter `Read()`, so `source`
    // is provably destroyed with a hedge task in flight.
    release_loser->set_value();
    WaitForSignal(loser_in_read);
  }
  // Leaving the scope destroys `source` and the last external reference to
  // `HedgingThreadPool`, which joins the in-flight worker thread and destroys
  // the losing hedge's mock before returning.
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
