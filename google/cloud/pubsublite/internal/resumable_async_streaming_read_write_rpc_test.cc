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
#include "google/cloud/internal/async_read_write_stream_impl.h"
#include "google/cloud/async_operation.h"
#include "google/cloud/backoff_policy.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/future.h"
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

using ::google::cloud::internal::AsyncGrpcOperation;
using ::google::cloud::internal::MakeStreamingReadWriteRpc;

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

using MockReturnType = std::unique_ptr<
    grpc::ClientAsyncReaderWriterInterface<FakeRequest, FakeResponse>>;

using MockAsyncReturnType =
    std::unique_ptr<AsyncStreamingReadWriteRpc<FakeRequest, FakeResponse>>;

class MockReaderWriter
    : public grpc::ClientAsyncReaderWriterInterface<FakeRequest, FakeResponse> {
 public:
  MOCK_METHOD(void, WritesDone, (void*), (override));
  MOCK_METHOD(void, Read, (FakeResponse*, void*), (override));
  MOCK_METHOD(void, Write, (FakeRequest const&, void*), (override));
  MOCK_METHOD(void, Write, (FakeRequest const&, grpc::WriteOptions, void*),
              (override));
  MOCK_METHOD(void, Finish, (grpc::Status*, void*), (override));
  MOCK_METHOD(void, StartCall, (void*), (override));
  MOCK_METHOD(void, ReadInitialMetadata, (void*), (override));
};

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

class MockStub {
 public:
  MOCK_METHOD(MockAsyncReturnType, FakeStream, (), ());
  MOCK_METHOD(MockReturnType, FakeRpc,
              (grpc::ClientContext*, grpc::CompletionQueue*), ());
};

struct TestRetryablePolicy {
  static bool IsPermanentFailure(google::cloud::Status const& s) {
    return !s.ok() &&
           (s.code() == google::cloud::StatusCode::kPermissionDenied);
  }
};

using RetryPolicyForTest =
    ::google::cloud::internal::TraitBasedRetryPolicy<TestRetryablePolicy>;

using LimitedErrorCountRetryPolicyForTest =
    ::google::cloud::internal::LimitedErrorCountRetryPolicy<
        TestRetryablePolicy>;

std::unique_ptr<RetryPolicyForTest> DefaultRetryPolicy() {
  // With maximum_failures==2 it tolerates up to 2 failures, so the *third*
  // failure is an error.
  return LimitedErrorCountRetryPolicyForTest(/*maximum_failures=*/2).clone();
}

std::unique_ptr<BackoffPolicy> DefaultBackoffPolicy() {
  using us = std::chrono::microseconds;
  return ExponentialBackoffPolicy(/*initial_delay=*/us(10),
                                  /*maximum_delay=*/us(40), /*scaling=*/2.0)
      .clone();
}

future<void> MockSleeper(std::chrono::duration<double>) {
  return make_ready_future();
}

TEST(AsyncReadWriteStreamingRpcTest, Basic) {
  MockStub mock;
  std::deque<std::shared_ptr<AsyncGrpcOperation>> operations;
  auto notify_next_op = [&](bool ok = true) {
    auto op = std::move(operations.front());
    operations.pop_front();
    op->Notify(ok);
  };
  EXPECT_CALL(mock, FakeStream).WillOnce([&operations, &mock]() {
    EXPECT_CALL(mock, FakeRpc)
        .WillOnce([](grpc::ClientContext*, grpc::CompletionQueue*) {
          auto stream = absl::make_unique<MockReaderWriter>();
          EXPECT_CALL(*stream, StartCall).Times(1);
          EXPECT_CALL(*stream, Read)
              .WillOnce([](FakeResponse* response, void*) {
                response->key = "key0";
                response->value = "value0_0";
              });
          EXPECT_CALL(*stream, Finish)
              .WillOnce([](grpc::Status* status, void*) {
                *status = grpc::Status::OK;
              });
          return stream;
        });

    auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
    grpc::CompletionQueue grpc_cq;
    EXPECT_CALL(*mock_cq, cq).WillRepeatedly(ReturnRef(grpc_cq));

    EXPECT_CALL(*mock_cq, StartOperation)
        .WillRepeatedly([&operations](std::shared_ptr<AsyncGrpcOperation> op,
                                      absl::FunctionRef<void(void*)> call) {
          void* tag = op.get();
          operations.push_back(std::move(op));
          call(tag);
        });

    google::cloud::CompletionQueue cq(mock_cq);
    return MakeStreamingReadWriteRpc<FakeRequest, FakeResponse>(
        cq, absl::make_unique<grpc::ClientContext>(),
        [&mock](grpc::ClientContext* context, grpc::CompletionQueue* cq) {
          return mock.FakeRpc(context, cq);
        });
  });

  std::shared_ptr<BackoffPolicy const> backoff_policy =
      std::move(DefaultBackoffPolicy());

  ResumableAsyncStreamingReadWriteRpcImpl<FakeRequest, FakeResponse,
                                          RetryPolicyForTest>
      stream(
          backoff_policy, &MockSleeper, [&mock]() { return mock.FakeStream(); },
          [&mock](MockAsyncReturnType stream) {
            return make_ready_future(
                StatusOr<std::unique_ptr<
                    AsyncStreamingReadWriteRpc<FakeRequest, FakeResponse>>>(
                    std::move(stream)));
          },
          std::move(DefaultRetryPolicy()));

  auto start = stream.Start();
  ASSERT_EQ(1, operations.size());
  notify_next_op();

  auto read0 = stream.Read();
  ASSERT_EQ(1, operations.size());
  notify_next_op();
  auto response0 = read0.get();
  ASSERT_TRUE(response0.has_value());
  EXPECT_EQ("key0", response0->key);
  EXPECT_EQ("value0_0", response0->value);

  auto finish = stream.Finish();
  ASSERT_EQ(1, operations.size());
  notify_next_op();
  finish.get();

  EXPECT_THAT(start.get(), IsOk());
}

}  // namespace
}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
