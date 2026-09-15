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
#include "google/cloud/testing_util/mock_opentelemetry_metrics.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
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
using ::google::cloud::testing_util::MockCounter;
using ::google::cloud::testing_util::MockMeter;
using ::google::cloud::testing_util::MockMeterProvider;
using ::google::cloud::testing_util::StatusIs;
using ::testing::A;
using ::testing::Eq;
using ::testing::Return;

// Large enough that no test read is treated as oversized.
std::size_t constexpr kUnlimitedBuffer = std::size_t{1} << 30;

std::shared_ptr<ThreadPool> MakeUnlimitedReadPool() {
  return std::make_shared<ThreadPool>(/*max_threads=*/4);
}

std::shared_ptr<HedgingThreadPool> MakeUnlimitedHedgePool() {
  return std::make_shared<HedgingThreadPool>(
      /*max_threads=*/4, /*rate_limit=*/0.0, /*capacity=*/0.0,
      /*max_concurrent=*/0);
}

ReadSourceResult MakeReadResult(std::string const& payload) {
  return ReadSourceResult{payload.size(),
                          HttpResponse{HttpStatusCode::kOk, {}, {}}};
}

auto MakeStallingPrimaryFactory(
    std::shared_ptr<std::promise<void>> const& unblock_primary,
    std::shared_ptr<std::promise<void>> const& primary_closed,
    std::shared_ptr<std::atomic<int>> const& calls) {
  return [unblock_primary, primary_closed,
          calls]() -> StatusOr<std::unique_ptr<ObjectReadSource>> {
    auto mock = std::make_unique<MockObjectReadSource>();
    if (++*calls == 1) {
      EXPECT_CALL(*mock, Read)
          .WillOnce([unblock_primary](char* buf, std::size_t) {
            unblock_primary->get_future().get();
            std::string const payload = "slow";
            std::copy(payload.begin(), payload.end(), buf);
            return MakeReadResult(payload);
          });
      EXPECT_CALL(*mock, Close).WillOnce([primary_closed]() {
        primary_closed->set_value();
        return make_status_or(HttpResponse{HttpStatusCode::kOk, {}, {}});
      });
    } else {
      EXPECT_CALL(*mock, Read).WillOnce([](char* buf, std::size_t) {
        std::string const payload = "hedge";
        std::copy(payload.begin(), payload.end(), buf);
        return MakeReadResult(payload);
      });
    }
    return std::unique_ptr<ObjectReadSource>(std::move(mock));
  };
}

TEST(HedgedObjectReadSourceTest, PrimaryWins) {
  auto factory = []() -> StatusOr<std::unique_ptr<ObjectReadSource>> {
    auto mock = std::make_unique<MockObjectReadSource>();
    EXPECT_CALL(*mock, Read).WillOnce([](char* buf, std::size_t) {
      std::string const payload = "payload";
      std::copy(payload.begin(), payload.end(), buf);
      return MakeReadResult(payload);
    });
    return std::unique_ptr<ObjectReadSource>(std::move(mock));
  };

  HedgedObjectReadSource source(MakeUnlimitedReadPool(),
                                MakeUnlimitedHedgePool(), factory,
                                std::chrono::milliseconds(500),
                                /*max_hedges=*/2, kUnlimitedBuffer);

  std::vector<char> buffer(100);
  auto result = source.Read(buffer.data(), buffer.size());
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(result->bytes_received, Eq(7));
  EXPECT_THAT(std::string(buffer.data(), result->bytes_received),
              Eq("payload"));
}

TEST(HedgedObjectReadSourceTest, SubsequentReadsContinueOnWinner) {
  // The factory must be called exactly once: after the open race is decided,
  // reads must continue on the winning child without creating new children,
  // otherwise the stream would restart at the wrong offset.
  auto factory_calls = std::make_shared<std::atomic<int>>(0);
  auto factory =
      [factory_calls]() -> StatusOr<std::unique_ptr<ObjectReadSource>> {
    ++*factory_calls;
    auto mock = std::make_unique<MockObjectReadSource>();
    EXPECT_CALL(*mock, Read)
        .WillOnce([](char* buf, std::size_t) {
          std::string const payload = "chunk-1";
          std::copy(payload.begin(), payload.end(), buf);
          return MakeReadResult(payload);
        })
        .WillOnce([](char* buf, std::size_t) {
          std::string const payload = "chunk-2";
          std::copy(payload.begin(), payload.end(), buf);
          return MakeReadResult(payload);
        });
    return std::unique_ptr<ObjectReadSource>(std::move(mock));
  };

  HedgedObjectReadSource source(MakeUnlimitedReadPool(),
                                MakeUnlimitedHedgePool(), factory,
                                std::chrono::milliseconds(500),
                                /*max_hedges=*/2, kUnlimitedBuffer);

  std::vector<char> buffer(100);
  EXPECT_THAT(source.Read(buffer.data(), buffer.size()), IsOk());
  EXPECT_THAT(source.Read(buffer.data(), buffer.size()), IsOk());
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
      MakeUnlimitedReadPool(), MakeUnlimitedHedgePool(), factory,
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
                                MakeUnlimitedHedgePool(), factory,
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
    EXPECT_CALL(*mock, Read).WillOnce([](char* buf, std::size_t) {
      std::string const payload = "primary_only";
      std::copy(payload.begin(), payload.end(), buf);
      return MakeReadResult(payload);
    });
    return std::unique_ptr<ObjectReadSource>(std::move(mock));
  };

  HedgedObjectReadSource source(read_pool, hedge_pool, factory,
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

  HedgedObjectReadSource source(MakeUnlimitedReadPool(), hedge_pool, factory,
                                std::chrono::milliseconds(10),
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
  GTEST_SKIP() << "Flaky test: "
                  "https://github.com/googleapis/google-cloud-cpp/issues/16413";

  // Verify that if a hedge attempt fails during stream opening (factory()
  // error), the hedge concurrency slot is released via RAII (SlotGuard) and is
  // not leaked.
  auto unblock_primary = std::make_shared<std::promise<void>>();
  auto calls = std::make_shared<std::atomic<int>>(0);
  auto factory = [unblock_primary,
                  calls]() -> StatusOr<std::unique_ptr<ObjectReadSource>> {
    int call_count = ++*calls;
    if (call_count == 1) {
      // Primary attempt: stalls until unblocked.
      auto mock = std::make_unique<MockObjectReadSource>();
      EXPECT_CALL(*mock, Read)
          .WillOnce([unblock_primary](char* buf, std::size_t) {
            unblock_primary->get_future().get();
            std::string const payload = "primary";
            std::copy(payload.begin(), payload.end(), buf);
            return MakeReadResult(payload);
          });
      return std::unique_ptr<ObjectReadSource>(std::move(mock));
    }
    // Hedge attempt: fails to open.
    return Status(StatusCode::kUnavailable, "open failed");
  };

  auto hedge_pool = std::make_shared<HedgingThreadPool>(
      /*max_threads=*/2, /*rate_limit=*/0.0, /*capacity=*/0.0,
      /*max_concurrent=*/1);

  HedgedObjectReadSource source(MakeUnlimitedReadPool(), hedge_pool, factory,
                                std::chrono::milliseconds(1),
                                /*max_hedges=*/1, kUnlimitedBuffer);

  std::vector<char> buffer(100);
  std::thread unblocker([unblock_primary] {
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    unblock_primary->set_value();
  });

  auto result = source.Read(buffer.data(), buffer.size());
  unblocker.join();

  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(std::string(buffer.data(), result->bytes_received),
              Eq("primary"));
  EXPECT_THAT(calls->load(), Eq(2));

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
        .WillOnce([unblock_primary](char* buf, std::size_t) {
          unblock_primary->get_future().get();
          std::string const payload = "primary_data";
          std::copy(payload.begin(), payload.end(), buf);
          return MakeReadResult(payload);
        });
    return std::unique_ptr<ObjectReadSource>(std::move(mock));
  };

  auto hedge_pool = std::make_shared<HedgingThreadPool>(
      /*max_threads=*/1, /*rate_limit=*/0.0, /*capacity=*/0.0,
      /*max_concurrent=*/1);
  // Exhaust all hedge slots so TryAcquireHedgeToken fails.
  ASSERT_TRUE(hedge_pool->TryAcquireHedgeToken());

  HedgedObjectReadSource source(MakeUnlimitedReadPool(), hedge_pool, factory,
                                std::chrono::milliseconds(0),
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
    EXPECT_CALL(*mock, Read).WillOnce([](char* buf, std::size_t) {
      std::string const payload = "primary_only";
      std::copy(payload.begin(), payload.end(), buf);
      return MakeReadResult(payload);
    });
    return std::unique_ptr<ObjectReadSource>(std::move(mock));
  };

  HedgedObjectReadSource source(MakeUnlimitedReadPool(),
                                MakeUnlimitedHedgePool(), factory,
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
                                MakeUnlimitedHedgePool(), factory,
                                std::chrono::milliseconds(500),
                                /*max_hedges=*/2, kUnlimitedBuffer);

  std::vector<char> buffer(100);
  EXPECT_THAT(source.Read(buffer.data(), buffer.size()),
              StatusIs(StatusCode::kPermissionDenied));
}

TEST(HedgedObjectReadSourceTest, CloseWithoutReadSucceeds) {
  auto factory = []() -> StatusOr<std::unique_ptr<ObjectReadSource>> {
    return Status(StatusCode::kUnimplemented, "never called");
  };
  HedgedObjectReadSource source(MakeUnlimitedReadPool(),
                                MakeUnlimitedHedgePool(), factory,
                                std::chrono::milliseconds(500),
                                /*max_hedges=*/2, kUnlimitedBuffer);
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
  HedgedObjectReadSource source(read_pool, hedge_pool, factory,
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
    EXPECT_CALL(*mock, Read).WillOnce([](char* buf, std::size_t) {
      std::string const payload = "direct";
      std::copy(payload.begin(), payload.end(), buf);
      return MakeReadResult(payload);
    });
    return std::unique_ptr<ObjectReadSource>(std::move(mock));
  };

  // A zero delay would let a hedge start immediately if the limit were not
  // honored, so any race would be observable as extra factory calls.
  HedgedObjectReadSource source(MakeUnlimitedReadPool(),
                                MakeUnlimitedHedgePool(), factory,
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
                                MakeUnlimitedHedgePool(), factory,
                                std::chrono::milliseconds(0),
                                /*max_hedges=*/2, /*max_buffer=*/8);

  std::vector<char> buffer(64);
  EXPECT_THAT(source.Read(buffer.data(), buffer.size()),
              StatusIs(StatusCode::kPermissionDenied));
}

TEST(HedgedObjectReadSourceTest, SubsequentReadsIgnoreBufferLimit) {
  // The limit only decides whether the *open* is hedged. Once a child exists,
  // reads of any size continue on it without staging buffers.
  auto calls = std::make_shared<std::atomic<int>>(0);
  auto factory = [calls]() -> StatusOr<std::unique_ptr<ObjectReadSource>> {
    ++*calls;
    auto mock = std::make_unique<MockObjectReadSource>();
    EXPECT_CALL(*mock, Read)
        .WillOnce([](char* buf, std::size_t) {
          std::string const payload = "small";
          std::copy(payload.begin(), payload.end(), buf);
          return MakeReadResult(payload);
        })
        .WillOnce([](char* buf, std::size_t) {
          std::string const payload = "large";
          std::copy(payload.begin(), payload.end(), buf);
          return MakeReadResult(payload);
        });
    return std::unique_ptr<ObjectReadSource>(std::move(mock));
  };

  HedgedObjectReadSource source(MakeUnlimitedReadPool(),
                                MakeUnlimitedHedgePool(), factory,
                                std::chrono::milliseconds(500),
                                /*max_hedges=*/2, /*max_buffer=*/64);

  std::vector<char> small(8);
  EXPECT_THAT(source.Read(small.data(), small.size()), IsOk());
  // Well past the limit, but the winner is already open.
  std::vector<char> large(4096);
  EXPECT_THAT(source.Read(large.data(), large.size()), IsOk());
  EXPECT_THAT(calls->load(), Eq(1));
}

TEST(HedgedObjectReadSourceTest, MetricsInterface) {
  HedgedReadMetrics metrics;
  metrics.IncrementHedgesDispatched();
  metrics.IncrementHedgeWon();
}

TEST(HedgedObjectReadSourceTest, PrimaryWinsMetrics) {
  auto mock_hedges_dispatched = std::make_unique<MockCounter<std::uint64_t>>();
  EXPECT_CALL(*mock_hedges_dispatched, Add(A<std::uint64_t>())).Times(0);

  auto mock_hedge_won = std::make_unique<MockCounter<std::uint64_t>>();
  EXPECT_CALL(*mock_hedge_won, Add(A<std::uint64_t>())).Times(0);

  auto mock_meter = std::make_shared<MockMeter>();
  EXPECT_CALL(*mock_meter, CreateUInt64Counter)
      .WillOnce([mock = std::move(mock_hedges_dispatched)](
                    opentelemetry::nostd::string_view name,
                    opentelemetry::nostd::string_view,
                    opentelemetry::nostd::string_view) mutable {
        EXPECT_THAT(name, Eq("storage.read_hedging.hedges_dispatched"));
        return std::move(mock);
      })
      .WillOnce([mock = std::move(mock_hedge_won)](
                    opentelemetry::nostd::string_view name,
                    opentelemetry::nostd::string_view,
                    opentelemetry::nostd::string_view) mutable {
        EXPECT_THAT(name, Eq("storage.read_hedging.hedge_won"));
        return std::move(mock);
      });

  auto mock_provider = std::make_shared<MockMeterProvider>();
  EXPECT_CALL(*mock_provider, GetMeter).WillOnce(Return(mock_meter));

  auto metrics = std::make_shared<HedgedReadMetrics>(mock_provider);

  auto factory = []() -> StatusOr<std::unique_ptr<ObjectReadSource>> {
    auto mock = std::make_unique<MockObjectReadSource>();
    EXPECT_CALL(*mock, Read).WillOnce([](char* buf, std::size_t) {
      std::string const payload = "payload";
      std::copy(payload.begin(), payload.end(), buf);
      return MakeReadResult(payload);
    });
    return std::unique_ptr<ObjectReadSource>(std::move(mock));
  };

  HedgedObjectReadSource source(MakeUnlimitedReadPool(),
                                MakeUnlimitedHedgePool(), factory,
                                std::chrono::milliseconds(500),
                                /*max_hedges=*/2, kUnlimitedBuffer, metrics);

  std::vector<char> buffer(100);
  StatusOr<ReadSourceResult> result = source.Read(buffer.data(), buffer.size());
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(result->bytes_received, Eq(7));
}

TEST(HedgedObjectReadSourceTest, HedgeWinsMetrics) {
  auto mock_hedges_dispatched = std::make_unique<MockCounter<std::uint64_t>>();
  EXPECT_CALL(*mock_hedges_dispatched, Add(std::uint64_t{1}));

  auto mock_hedge_won = std::make_unique<MockCounter<std::uint64_t>>();
  EXPECT_CALL(*mock_hedge_won, Add(std::uint64_t{1}));

  auto mock_meter = std::make_shared<MockMeter>();
  EXPECT_CALL(*mock_meter, CreateUInt64Counter)
      .WillOnce([mock = std::move(mock_hedges_dispatched)](
                    opentelemetry::nostd::string_view name,
                    opentelemetry::nostd::string_view,
                    opentelemetry::nostd::string_view) mutable {
        EXPECT_THAT(name, Eq("storage.read_hedging.hedges_dispatched"));
        return std::move(mock);
      })
      .WillOnce([mock = std::move(mock_hedge_won)](
                    opentelemetry::nostd::string_view name,
                    opentelemetry::nostd::string_view,
                    opentelemetry::nostd::string_view) mutable {
        EXPECT_THAT(name, Eq("storage.read_hedging.hedge_won"));
        return std::move(mock);
      });

  auto mock_provider = std::make_shared<MockMeterProvider>();
  EXPECT_CALL(*mock_provider, GetMeter).WillOnce(Return(mock_meter));

  auto metrics = std::make_shared<HedgedReadMetrics>(mock_provider);

  auto unblock_primary = std::make_shared<std::promise<void>>();
  auto primary_closed = std::make_shared<std::promise<void>>();
  auto calls = std::make_shared<std::atomic<int>>(0);
  auto factory =
      MakeStallingPrimaryFactory(unblock_primary, primary_closed, calls);

  HedgedObjectReadSource source(MakeUnlimitedReadPool(),
                                MakeUnlimitedHedgePool(), factory,
                                std::chrono::milliseconds(10),
                                /*max_hedges=*/1, kUnlimitedBuffer, metrics);

  std::vector<char> buffer(100);
  StatusOr<ReadSourceResult> result = source.Read(buffer.data(), buffer.size());
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(result->bytes_received, Eq(5));
  EXPECT_THAT(std::string(buffer.data(), result->bytes_received), Eq("hedge"));

  unblock_primary->set_value();
  primary_closed->get_future().get();
}

TEST(HedgedObjectReadSourceTest, NullProviderSafe) {
  HedgedReadMetrics metrics(nullptr);
  metrics.IncrementHedgesDispatched();
  metrics.IncrementHedgeWon();
}

TEST(HedgedObjectReadSourceTest, NullMeterSafe) {
  auto mock_provider = std::make_shared<MockMeterProvider>();
  EXPECT_CALL(*mock_provider, GetMeter).WillOnce(Return(nullptr));

  HedgedReadMetrics metrics(mock_provider);
  metrics.IncrementHedgesDispatched();
  metrics.IncrementHedgeWon();
}

TEST(HedgedObjectReadSourceTest, NullMetricsSafe) {
  auto factory = []() -> StatusOr<std::unique_ptr<ObjectReadSource>> {
    auto mock = std::make_unique<MockObjectReadSource>();
    EXPECT_CALL(*mock, Read).WillOnce([](char* buf, std::size_t) {
      std::string const payload = "payload";
      std::copy(payload.begin(), payload.end(), buf);
      return MakeReadResult(payload);
    });
    return std::unique_ptr<ObjectReadSource>(std::move(mock));
  };

  HedgedObjectReadSource source(MakeUnlimitedReadPool(),
                                MakeUnlimitedHedgePool(), factory,
                                std::chrono::milliseconds(500),
                                /*max_hedges=*/2, kUnlimitedBuffer,
                                /*metrics=*/nullptr);

  std::vector<char> buffer(100);
  StatusOr<ReadSourceResult> result = source.Read(buffer.data(), buffer.size());
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(result->bytes_received, Eq(7));
}

TEST(HedgedObjectReadSourceTest, NullMetricsSafeHedgeWins) {
  auto unblock_primary = std::make_shared<std::promise<void>>();
  auto primary_closed = std::make_shared<std::promise<void>>();
  auto calls = std::make_shared<std::atomic<int>>(0);
  auto factory =
      MakeStallingPrimaryFactory(unblock_primary, primary_closed, calls);

  HedgedObjectReadSource source(MakeUnlimitedReadPool(),
                                MakeUnlimitedHedgePool(), factory,
                                std::chrono::milliseconds(10),
                                /*max_hedges=*/1, kUnlimitedBuffer,
                                /*metrics=*/nullptr);

  std::vector<char> buffer(100);
  StatusOr<ReadSourceResult> result = source.Read(buffer.data(), buffer.size());
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(result->bytes_received, Eq(5));
  EXPECT_THAT(std::string(buffer.data(), result->bytes_received), Eq("hedge"));

  unblock_primary->set_value();
  primary_closed->get_future().get();
}

TEST(HedgedObjectReadSourceTest, MultipleHedgesDispatchedPrimaryWins) {
  auto mock_hedges_dispatched = std::make_unique<MockCounter<std::uint64_t>>();
  EXPECT_CALL(*mock_hedges_dispatched, Add(std::uint64_t{1})).Times(2);

  auto mock_hedge_won = std::make_unique<MockCounter<std::uint64_t>>();
  EXPECT_CALL(*mock_hedge_won, Add(A<std::uint64_t>())).Times(0);

  auto mock_meter = std::make_shared<MockMeter>();
  EXPECT_CALL(*mock_meter, CreateUInt64Counter)
      .WillOnce([mock = std::move(mock_hedges_dispatched)](
                    opentelemetry::nostd::string_view name,
                    opentelemetry::nostd::string_view,
                    opentelemetry::nostd::string_view) mutable {
        EXPECT_THAT(name, Eq("storage.read_hedging.hedges_dispatched"));
        return std::move(mock);
      })
      .WillOnce([mock = std::move(mock_hedge_won)](
                    opentelemetry::nostd::string_view name,
                    opentelemetry::nostd::string_view,
                    opentelemetry::nostd::string_view) mutable {
        EXPECT_THAT(name, Eq("storage.read_hedging.hedge_won"));
        return std::move(mock);
      });

  auto mock_provider = std::make_shared<MockMeterProvider>();
  EXPECT_CALL(*mock_provider, GetMeter).WillOnce(Return(mock_meter));

  auto metrics = std::make_shared<HedgedReadMetrics>(mock_provider);

  auto unblock_primary = std::make_shared<std::promise<void>>();
  auto unblock_hedges = std::make_shared<std::promise<void>>();
  std::shared_future<void> hedge_future = unblock_hedges->get_future().share();
  auto hedges_dispatched_count = std::make_shared<std::atomic<int>>(0);
  auto calls = std::make_shared<std::atomic<int>>(0);

  auto factory = [unblock_primary, hedge_future, hedges_dispatched_count,
                  calls]() -> StatusOr<std::unique_ptr<ObjectReadSource>> {
    int call_count = ++*calls;
    auto mock = std::make_unique<MockObjectReadSource>();
    if (call_count == 1) {
      EXPECT_CALL(*mock, Read)
          .WillOnce([unblock_primary](char* buf, std::size_t) {
            unblock_primary->get_future().get();
            std::string const payload = "primary";
            std::copy(payload.begin(), payload.end(), buf);
            return MakeReadResult(payload);
          });
    } else {
      EXPECT_CALL(*mock, Read)
          .WillOnce(
              [hedge_future, hedges_dispatched_count](char* buf, std::size_t) {
                ++*hedges_dispatched_count;
                hedge_future.get();
                std::string const payload = "hedge";
                std::copy(payload.begin(), payload.end(), buf);
                return MakeReadResult(payload);
              });
      EXPECT_CALL(*mock, Close).WillOnce([]() {
        return make_status_or(HttpResponse{HttpStatusCode::kOk, {}, {}});
      });
    }
    return std::unique_ptr<ObjectReadSource>(std::move(mock));
  };

  HedgedObjectReadSource source(MakeUnlimitedReadPool(),
                                MakeUnlimitedHedgePool(), factory,
                                std::chrono::milliseconds(5),
                                /*max_hedges=*/2, kUnlimitedBuffer, metrics);

  std::vector<char> buffer(100);
  std::thread trigger([unblock_primary, hedges_dispatched_count] {
    while (hedges_dispatched_count->load() < 2) {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    unblock_primary->set_value();
  });

  StatusOr<ReadSourceResult> result = source.Read(buffer.data(), buffer.size());
  trigger.join();

  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(std::string(buffer.data(), result->bytes_received),
              Eq("primary"));

  unblock_hedges->set_value();
}

TEST(HedgedObjectReadSourceTest, HedgeReadErrorDoesNotIncrementHedgeWon) {
  auto mock_hedges_dispatched = std::make_unique<MockCounter<std::uint64_t>>();
  EXPECT_CALL(*mock_hedges_dispatched, Add(std::uint64_t{1})).Times(1);

  auto mock_hedge_won = std::make_unique<MockCounter<std::uint64_t>>();
  EXPECT_CALL(*mock_hedge_won, Add(A<std::uint64_t>())).Times(0);

  auto mock_meter = std::make_shared<MockMeter>();
  EXPECT_CALL(*mock_meter, CreateUInt64Counter)
      .WillOnce([mock = std::move(mock_hedges_dispatched)](
                    opentelemetry::nostd::string_view name,
                    opentelemetry::nostd::string_view,
                    opentelemetry::nostd::string_view) mutable {
        EXPECT_THAT(name, Eq("storage.read_hedging.hedges_dispatched"));
        return std::move(mock);
      })
      .WillOnce([mock = std::move(mock_hedge_won)](
                    opentelemetry::nostd::string_view name,
                    opentelemetry::nostd::string_view,
                    opentelemetry::nostd::string_view) mutable {
        EXPECT_THAT(name, Eq("storage.read_hedging.hedge_won"));
        return std::move(mock);
      });

  auto mock_provider = std::make_shared<MockMeterProvider>();
  EXPECT_CALL(*mock_provider, GetMeter).WillOnce(Return(mock_meter));

  auto metrics = std::make_shared<HedgedReadMetrics>(mock_provider);

  auto unblock_primary = std::make_shared<std::promise<void>>();
  auto calls = std::make_shared<std::atomic<int>>(0);
  auto factory = [unblock_primary,
                  calls]() -> StatusOr<std::unique_ptr<ObjectReadSource>> {
    int call_count = ++*calls;
    auto mock = std::make_unique<MockObjectReadSource>();
    if (call_count == 1) {
      EXPECT_CALL(*mock, Read)
          .WillOnce([unblock_primary](char* buf, std::size_t) {
            unblock_primary->get_future().get();
            std::string const payload = "primary";
            std::copy(payload.begin(), payload.end(), buf);
            return MakeReadResult(payload);
          });
    } else {
      EXPECT_CALL(*mock, Read).WillOnce([](char*, std::size_t) {
        return Status(StatusCode::kUnavailable, "hedge read failed");
      });
      EXPECT_CALL(*mock, Close).WillOnce([]() {
        return make_status_or(HttpResponse{HttpStatusCode::kOk, {}, {}});
      });
    }
    return std::unique_ptr<ObjectReadSource>(std::move(mock));
  };

  HedgedObjectReadSource source(MakeUnlimitedReadPool(),
                                MakeUnlimitedHedgePool(), factory,
                                std::chrono::milliseconds(5),
                                /*max_hedges=*/1, kUnlimitedBuffer, metrics);

  std::vector<char> buffer(100);
  std::thread unblocker([unblock_primary] {
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    unblock_primary->set_value();
  });

  StatusOr<ReadSourceResult> result = source.Read(buffer.data(), buffer.size());
  unblocker.join();

  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(std::string(buffer.data(), result->bytes_received),
              Eq("primary"));
  EXPECT_THAT(calls->load(), Eq(2));
}

TEST(HedgedObjectReadSourceTest, HedgeReadErrorIgnoredWhenPrimarySucceeds) {
  auto unblock_primary = std::make_shared<std::promise<void>>();
  auto calls = std::make_shared<std::atomic<int>>(0);
  auto factory = [unblock_primary,
                  calls]() -> StatusOr<std::unique_ptr<ObjectReadSource>> {
    int call_count = ++*calls;
    auto mock = std::make_unique<MockObjectReadSource>();
    if (call_count == 1) {
      // Primary attempt: stalls until unblocked, then succeeds.
      EXPECT_CALL(*mock, Read)
          .WillOnce([unblock_primary](char* buf, std::size_t) {
            unblock_primary->get_future().get();
            std::string const payload = "primary";
            std::copy(payload.begin(), payload.end(), buf);
            return MakeReadResult(payload);
          });
    } else {
      // Hedge attempt: opens successfully, but Read fails.
      EXPECT_CALL(*mock, Read).WillOnce([](char*, std::size_t) {
        return Status(StatusCode::kUnavailable, "hedge read failed");
      });
      EXPECT_CALL(*mock, Close).WillOnce([]() {
        return make_status_or(HttpResponse{HttpStatusCode::kOk, {}, {}});
      });
    }
    return std::unique_ptr<ObjectReadSource>(std::move(mock));
  };

  auto hedge_pool = std::make_shared<HedgingThreadPool>(
      /*max_threads=*/2, /*rate_limit=*/0.0, /*capacity=*/0.0,
      /*max_concurrent=*/1);

  HedgedObjectReadSource source(MakeUnlimitedReadPool(), hedge_pool, factory,
                                std::chrono::milliseconds(1),
                                /*max_hedges=*/1, kUnlimitedBuffer);

  std::vector<char> buffer(100);
  std::thread unblocker([unblock_primary] {
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    unblock_primary->set_value();
  });

  StatusOr<ReadSourceResult> result = source.Read(buffer.data(), buffer.size());
  unblocker.join();

  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(std::string(buffer.data(), result->bytes_received),
              Eq("primary"));
  EXPECT_THAT(calls->load(), Eq(2));

  // Verify that the hedge concurrency slot was released and not leaked.
  EXPECT_TRUE(hedge_pool->TryAcquireHedgeToken());
  hedge_pool->ReleaseHedgeSlot();
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
