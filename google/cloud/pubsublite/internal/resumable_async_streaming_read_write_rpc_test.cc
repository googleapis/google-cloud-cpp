// Copyright 2020 Google LLC
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

#include "google/cloud/pubsublite/internal/resumable_async_streaming_read_write_rpc.h"
#include "google/cloud/async_operation.h"
#include "google/cloud/backoff_policy.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/async_read_write_stream_impl.h"
#include "google/cloud/internal/retry_policy.h"
#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>
#include <chrono>
#include <deque>
#include <memory>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::MockCompletionQueueImpl;
using ::testing::_;
using ::testing::ReturnRef;

using ::google::cloud::internal::RetryPolicy;

struct FakeRequest {
  std::string key;
};

struct FakeResponse {
  std::string key;
  std::string value;
};

// Used as part of the EXPECT_CALL() below:
bool operator==(FakeRequest const& lhs, FakeRequest const& rhs) {
  return lhs.key == rhs.key;
}

using MockAsyncStreamReturnType =
    std::unique_ptr<AsyncStreamingReadWriteRpc<FakeRequest, FakeResponse>>;

class MockAsyncReaderWriter
    : public AsyncStreamingReadWriteRpc<FakeRequest, FakeResponse> {
 public:
  MOCK_METHOD(future<absl::optional<FakeResponse>>, Read, (), (override));
  MOCK_METHOD(future<bool>, Write, (FakeRequest const&, grpc::WriteOptions),
              (override));
  MOCK_METHOD(future<bool>, WritesDone, (), (override));
  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD(future<Status>, Finish, (), (override));
  MOCK_METHOD(future<bool>, Start, (), (override));
};

class MockRetryPolicy : public RetryPolicy {
 public:
  MOCK_METHOD(bool, OnFailure, (Status const&), (override));
  MOCK_METHOD(bool, IsExhausted, (), (const, override));
  MOCK_METHOD(bool, IsPermanentFailure, (Status const&), (const, override));
};

class MockStub {
 public:
  MOCK_METHOD(MockAsyncStreamReturnType, FakeStream, (), ());
};

std::unique_ptr<BackoffPolicy> DefaultBackoffPolicy() {
  using us = std::chrono::microseconds;
  return ExponentialBackoffPolicy(/*initial_delay=*/us(10),
                                  /*maximum_delay=*/us(40), /*scaling=*/2.0)
      .clone();
}

future<void> MockSleeper(std::chrono::duration<double>) {
  return make_ready_future();
}

template <typename T>
void SetOpValue(std::deque<std::shared_ptr<promise<T>>>& operations,
                T response) {
  auto op = std::move(operations.front());
  operations.pop_front();
  op->set_value(response);
}

TEST(AsyncReadWriteStreamingRpcTest, Basic) {
  MockStub mock;
  EXPECT_CALL(mock, FakeStream).WillOnce([]() {
    auto stream = absl::make_unique<MockAsyncReaderWriter>();
    EXPECT_CALL(*stream, Start).WillOnce([]() {
      return make_ready_future(true);
    });
    EXPECT_CALL(*stream, Write(FakeRequest{"key0"}, _)).WillOnce([]() {
      return make_ready_future(true);
    });
    EXPECT_CALL(*stream, Read).WillOnce([]() {
      return make_ready_future(
          absl::make_optional(FakeResponse{"key0", "value0_0"}));
    }).WillOnce([]() {
      return make_ready_future(
          absl::make_optional(FakeResponse{"key0", "value0_1"}));
    });
    EXPECT_CALL(*stream, Finish).WillOnce([]() {
      return make_ready_future(make_ready_future(Status()));
    });
    return stream;
  });

  std::shared_ptr<BackoffPolicy const> backoff_policy =
      std::move(DefaultBackoffPolicy());

  auto stream =
      MakeResumableAsyncStreamingReadWriteRpcImpl<FakeRequest, FakeResponse>(
          []() { return absl::make_unique<MockRetryPolicy>(); }, backoff_policy,
          &MockSleeper, [&mock]() { return mock.FakeStream(); },
          [&mock](MockAsyncStreamReturnType stream) {
            return make_ready_future(
                StatusOr<MockAsyncStreamReturnType>(std::move(stream)));
          });

  auto start = stream->Start();

  auto write = stream->Write(FakeRequest{"key0"},
                             grpc::WriteOptions().set_last_message());
  ASSERT_TRUE(write.get());

  auto response0 = stream->Read().get();
  ASSERT_TRUE(response0.has_value());
  EXPECT_EQ("key0", response0->key);
  EXPECT_EQ("value0_0", response0->value);

  response0 = stream->Read().get();
  ASSERT_TRUE(response0.has_value());
  EXPECT_EQ("key0", response0->key);
  EXPECT_EQ("value0_1", response0->value);

  auto finish = stream->Finish();
  finish.get();

  EXPECT_THAT(start.get(), IsOk());
}

}  // namespace
}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
