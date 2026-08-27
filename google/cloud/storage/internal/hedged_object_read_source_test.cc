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
using ::testing::Eq;

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

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
