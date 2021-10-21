// Copyright 2018 Google LLC.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/internal/async_retry_unary_rpc.h"
#include "google/cloud/internal/backoff_policy.h"
#include "google/cloud/internal/retry_policy.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/fake_completion_queue_impl.h"
#include "google/cloud/testing_util/mock_async_response_reader.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/bigtable/admin/v2/bigtable_table_admin.grpc.pb.h>
#include <gmock/gmock.h>
#include <thread>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

namespace btadmin = ::google::bigtable::admin::v2;
using ::google::cloud::testing_util::FakeCompletionQueueImpl;
using ::google::cloud::testing_util::MockAsyncResponseReader;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::chrono_literals::operator"" _us;
using ::testing::HasSubstr;
using ::testing::Return;

/**
 * A class to test the retry loop.
 *
 * Defines the async versions of two RPCs, one returning a value and the other
 * returning `void` (or its equivalent in protobuf):
 * - `google.bigtable.admin.v2.GetTable`
 * - `google.bigtagble.admin.v2.DeleteTable`
 */
class MockStub {
 public:
  MOCK_METHOD(
      std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<btadmin::Table>>,
      AsyncGetTable,
      (grpc::ClientContext*, btadmin::GetTableRequest const&,
       grpc::CompletionQueue* cq));

  MOCK_METHOD(
      std::unique_ptr<
          grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>,
      AsyncDeleteTable,
      (grpc::ClientContext*, btadmin::DeleteTableRequest const&,
       grpc::CompletionQueue* cq));
};

// Each library defines its own retry policy class, typically by defining the
// status type (we used to have different status objects on each library), and
// defining which status codes represent a permanent failure. In this test we
// define some types to mock the behavior of our libraries.

/// Define which status codes are permanent failures for this test.
struct IsRetryableTraits {
  static bool IsPermanentFailure(Status const& status) {
    return !status.ok() && status.code() != StatusCode::kUnavailable;
  }
};

using RpcRetryPolicy =
    google::cloud::internal::TraitBasedRetryPolicy<IsRetryableTraits>;
using RpcLimitedErrorCountRetryPolicy =
    google::cloud::internal::LimitedErrorCountRetryPolicy<IsRetryableTraits>;
using RpcBackoffPolicy = google::cloud::internal::BackoffPolicy;
using RpcExponentialBackoffPolicy =
    google::cloud::internal::ExponentialBackoffPolicy;

TEST(AsyncRetryUnaryRpcTest, ImmediatelySucceeds) {
  MockStub mock;

  using ReaderType = MockAsyncResponseReader<btadmin::Table>;
  auto reader = absl::make_unique<ReaderType>();
  EXPECT_CALL(*reader, Finish)
      .WillOnce([](btadmin::Table* table, grpc::Status* status, void*) {
        // Initialize a value to make sure it is carried all the way back to
        // the caller.
        table->set_name("fake/table/name/response");
        *status = grpc::Status::OK;
      });

  EXPECT_CALL(mock, AsyncGetTable)
      .WillOnce([&reader](grpc::ClientContext*,
                          btadmin::GetTableRequest const& request,
                          grpc::CompletionQueue*) {
        EXPECT_EQ("fake/table/name/request", request.name());
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            // This is safe, see comments in MockAsyncResponseReader.
            btadmin::Table>>(reader.get());
      });

  auto impl = std::make_shared<FakeCompletionQueueImpl>();
  CompletionQueue cq(impl);

  // Do some basic initialization of the request to verify the values get
  // carried to the mock.
  btadmin::GetTableRequest request;
  request.set_name("fake/table/name/request");

  auto fut = StartRetryAsyncUnaryRpc(
      cq, __func__, RpcLimitedErrorCountRetryPolicy(3).clone(),
      RpcExponentialBackoffPolicy(10_us, 40_us, 2.0).clone(),
      Idempotency::kIdempotent,
      [&mock](grpc::ClientContext* context,
              btadmin::GetTableRequest const& request,
              grpc::CompletionQueue* cq) {
        return mock.AsyncGetTable(context, request, cq);
      },
      request);

  EXPECT_EQ(1, impl->size());
  impl->SimulateCompletion(true);

  EXPECT_TRUE(impl->empty());
  ASSERT_EQ(std::future_status::ready, fut.wait_for(0_us));
  auto result = fut.get();
  ASSERT_STATUS_OK(result);
  EXPECT_EQ("fake/table/name/response", result->name());
}

TEST(AsyncRetryUnaryRpcTest, VoidImmediatelySucceeds) {
  MockStub mock;

  using ReaderType = MockAsyncResponseReader<google::protobuf::Empty>;
  auto reader = absl::make_unique<ReaderType>();
  EXPECT_CALL(*reader, Finish)
      .WillOnce([](google::protobuf::Empty*, grpc::Status* status, void*) {
        *status = grpc::Status::OK;
      });

  EXPECT_CALL(mock, AsyncDeleteTable)
      .WillOnce([&reader](grpc::ClientContext*,
                          btadmin::DeleteTableRequest const& request,
                          grpc::CompletionQueue*) {
        EXPECT_EQ("fake/table/name/request", request.name());
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            // This is safe, see comments in MockAsyncResponseReader.
            google::protobuf::Empty>>(reader.get());
      });

  auto impl = std::make_shared<FakeCompletionQueueImpl>();
  CompletionQueue cq(impl);

  // Do some basic initialization of the request to verify the values get
  // carried to the mock.
  btadmin::DeleteTableRequest request;
  request.set_name("fake/table/name/request");

  auto fut = StartRetryAsyncUnaryRpc(
      cq, __func__, RpcLimitedErrorCountRetryPolicy(3).clone(),
      RpcExponentialBackoffPolicy(10_us, 40_us, 2.0).clone(),
      Idempotency::kNonIdempotent,
      [&mock](grpc::ClientContext* context,
              btadmin::DeleteTableRequest const& request,
              grpc::CompletionQueue* cq) {
        return mock.AsyncDeleteTable(context, request, cq);
      },
      request);

  EXPECT_EQ(1, impl->size());
  impl->SimulateCompletion(true);

  EXPECT_TRUE(impl->empty());
  ASSERT_EQ(std::future_status::ready, fut.wait_for(0_us));
  auto result = fut.get();
  ASSERT_STATUS_OK(result);
}

TEST(AsyncRetryUnaryRpcTest, PermanentFailure) {
  MockStub mock;

  using ReaderType = MockAsyncResponseReader<btadmin::Table>;
  auto reader = absl::make_unique<ReaderType>();
  EXPECT_CALL(*reader, Finish)
      .WillOnce([](btadmin::Table*, grpc::Status* status, void*) {
        *status = grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh-oh");
      });

  EXPECT_CALL(mock, AsyncGetTable)
      .WillOnce([&reader](grpc::ClientContext*,
                          btadmin::GetTableRequest const& request,
                          grpc::CompletionQueue*) {
        EXPECT_EQ("fake/table/name/request", request.name());
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            // This is safe, see comments in MockAsyncResponseReader.
            btadmin::Table>>(reader.get());
      });

  auto impl = std::make_shared<FakeCompletionQueueImpl>();
  CompletionQueue cq(impl);

  // Do some basic initialization of the request to verify the values get
  // carried to the mock.
  btadmin::GetTableRequest request;
  request.set_name("fake/table/name/request");

  auto fut = StartRetryAsyncUnaryRpc(
      cq, __func__, RpcLimitedErrorCountRetryPolicy(3).clone(),
      RpcExponentialBackoffPolicy(10_us, 40_us, 2.0).clone(),
      Idempotency::kIdempotent,
      [&mock](grpc::ClientContext* context,
              btadmin::GetTableRequest const& request,
              grpc::CompletionQueue* cq) {
        return mock.AsyncGetTable(context, request, cq);
      },
      request);

  EXPECT_EQ(1, impl->size());
  impl->SimulateCompletion(true);

  EXPECT_TRUE(impl->empty());
  ASSERT_EQ(std::future_status::ready, fut.wait_for(0_us));
  auto result = fut.get();
  EXPECT_FALSE(result);
  EXPECT_EQ(StatusCode::kPermissionDenied, result.status().code());
  EXPECT_THAT(result.status().message(), HasSubstr("permanent failure"));
  EXPECT_THAT(result.status().message(), HasSubstr("uh-oh"));
}

TEST(AsyncRetryUnaryRpcTest, TooManyTransientFailures) {
  MockStub mock;

  using ReaderType = MockAsyncResponseReader<btadmin::Table>;
  auto finish_failure = [](btadmin::Table*, grpc::Status* status, void*) {
    *status = grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
  };

  auto r1 = absl::make_unique<ReaderType>();
  EXPECT_CALL(*r1, Finish).WillOnce(finish_failure);
  auto r2 = absl::make_unique<ReaderType>();
  EXPECT_CALL(*r2, Finish).WillOnce(finish_failure);
  auto r3 = absl::make_unique<ReaderType>();
  EXPECT_CALL(*r3, Finish).WillOnce(finish_failure);

  EXPECT_CALL(mock, AsyncGetTable)
      .WillOnce([&r1](grpc::ClientContext*,
                      btadmin::GetTableRequest const& request,
                      grpc::CompletionQueue*) {
        EXPECT_EQ("fake/table/name/request", request.name());
        return std::unique_ptr<
            grpc::ClientAsyncResponseReaderInterface<btadmin::Table>>(r1.get());
      })
      .WillOnce([&r2](grpc::ClientContext*,
                      btadmin::GetTableRequest const& request,
                      grpc::CompletionQueue*) {
        EXPECT_EQ("fake/table/name/request", request.name());
        return std::unique_ptr<
            grpc::ClientAsyncResponseReaderInterface<btadmin::Table>>(r2.get());
      })
      .WillOnce([&r3](grpc::ClientContext*,
                      btadmin::GetTableRequest const& request,
                      grpc::CompletionQueue*) {
        EXPECT_EQ("fake/table/name/request", request.name());
        return std::unique_ptr<
            grpc::ClientAsyncResponseReaderInterface<btadmin::Table>>(r3.get());
      });

  auto impl = std::make_shared<FakeCompletionQueueImpl>();
  CompletionQueue cq(impl);

  // Do some basic initialization of the request to verify the values get
  // carried to the mock.
  btadmin::GetTableRequest request;
  request.set_name("fake/table/name/request");

  auto fut = StartRetryAsyncUnaryRpc(
      cq, __func__, RpcLimitedErrorCountRetryPolicy(2).clone(),
      RpcExponentialBackoffPolicy(10_us, 40_us, 2.0).clone(),
      Idempotency::kIdempotent,
      [&mock](grpc::ClientContext* context,
              btadmin::GetTableRequest const& request,
              grpc::CompletionQueue* cq) {
        return mock.AsyncGetTable(context, request, cq);
      },
      request);

  // Because the maximum number of failures is 2 we expect 3 calls (the 3rd
  // failure is the "too many" case). In between the calls there are timers
  // executed, but there is no timer after the 3rd failure.
  EXPECT_EQ(1, impl->size());  // simulate the call completing
  impl->SimulateCompletion(true);
  EXPECT_EQ(1, impl->size());  // simulate the timer completing
  impl->SimulateCompletion(true);
  EXPECT_EQ(1, impl->size());  // simulate the call completing
  impl->SimulateCompletion(true);
  EXPECT_EQ(1, impl->size());  // simulate the timer completing
  impl->SimulateCompletion(true);
  EXPECT_EQ(1, impl->size());  // simulate the call completing
  impl->SimulateCompletion(true);
  EXPECT_TRUE(impl->empty());

  ASSERT_EQ(std::future_status::ready, fut.wait_for(0_us));
  auto result = fut.get();
  EXPECT_FALSE(result);
  EXPECT_EQ(StatusCode::kUnavailable, result.status().code());
  EXPECT_THAT(result.status().message(), HasSubstr("retry policy exhausted"));
  EXPECT_THAT(result.status().message(), HasSubstr("try-again"));
}

TEST(AsyncRetryUnaryRpcTest, TransientOnNonIdempotent) {
  MockStub mock;

  using ReaderType = MockAsyncResponseReader<google::protobuf::Empty>;
  auto reader = absl::make_unique<ReaderType>();
  EXPECT_CALL(*reader, Finish)
      .WillOnce([](google::protobuf::Empty*, grpc::Status* status, void*) {
        *status =
            grpc::Status(grpc::StatusCode::UNAVAILABLE, "maybe-try-again");
      });

  EXPECT_CALL(mock, AsyncDeleteTable)
      .WillOnce([&reader](grpc::ClientContext*,
                          btadmin::DeleteTableRequest const& request,
                          grpc::CompletionQueue*) {
        EXPECT_EQ("fake/table/name/request", request.name());
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            // This is safe, see comments in MockAsyncResponseReader.
            google::protobuf::Empty>>(reader.get());
      });

  auto impl = std::make_shared<FakeCompletionQueueImpl>();
  CompletionQueue cq(impl);

  // Do some basic initialization of the request to verify the values get
  // carried to the mock.
  btadmin::DeleteTableRequest request;
  request.set_name("fake/table/name/request");

  auto fut = StartRetryAsyncUnaryRpc(
      cq, __func__, RpcLimitedErrorCountRetryPolicy(3).clone(),
      RpcExponentialBackoffPolicy(10_us, 40_us, 2.0).clone(),
      Idempotency::kNonIdempotent,
      [&mock](grpc::ClientContext* context,
              btadmin::DeleteTableRequest const& request,
              grpc::CompletionQueue* cq) {
        return mock.AsyncDeleteTable(context, request, cq);
      },
      request);

  EXPECT_EQ(1, impl->size());
  impl->SimulateCompletion(true);

  EXPECT_TRUE(impl->empty());
  ASSERT_EQ(std::future_status::ready, fut.wait_for(0_us));
  auto result = fut.get();
  EXPECT_EQ(StatusCode::kUnavailable, result.status().code());
  EXPECT_THAT(result.status().message(), HasSubstr("non-idempotent"));
  EXPECT_THAT(result.status().message(), HasSubstr("maybe-try-again"));
}

class RetryPolicyWithSetup {
 public:
  virtual ~RetryPolicyWithSetup() = default;
  virtual bool OnFailure(Status const&) = 0;
  virtual void Setup(grpc::ClientContext&) const = 0;
  virtual bool IsExhausted() const = 0;
  virtual bool IsPermanentFailure(Status const&) const = 0;
};

class MockRetryPolicy : public RetryPolicyWithSetup {
 public:
  MOCK_METHOD(bool, OnFailure, (Status const&), (override));
  MOCK_METHOD(void, Setup, (grpc::ClientContext&), (const, override));
  MOCK_METHOD(bool, IsExhausted, (), (const, override));
  MOCK_METHOD(bool, IsPermanentFailure, (Status const&), (const, override));
};

TEST(AsyncRetryUnaryRpcTest, SetsTimeout) {
  auto mock = absl::make_unique<MockRetryPolicy>();
  EXPECT_CALL(*mock, OnFailure)
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  EXPECT_CALL(*mock, IsPermanentFailure).WillRepeatedly(Return(false));
  EXPECT_CALL(*mock, Setup).Times(3);

  using ReaderType = MockAsyncResponseReader<btadmin::Table>;
  std::vector<std::unique_ptr<ReaderType>> cleanup_readers;

  auto impl = std::make_shared<FakeCompletionQueueImpl>();
  CompletionQueue cq(impl);
  auto fut = StartRetryAsyncUnaryRpc(
      cq, __func__, std::move(mock),
      RpcExponentialBackoffPolicy(10_us, 40_us, 2.0).clone(),
      Idempotency::kIdempotent,
      [&](grpc::ClientContext*, btadmin::GetTableRequest const&,
          grpc::CompletionQueue*) {
        auto finish_failure = [](btadmin::Table*, grpc::Status* status, void*) {
          *status = grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
        };
        auto r = absl::make_unique<ReaderType>();
        EXPECT_CALL(*r, Finish).WillOnce(finish_failure);
        // gRPC defines a trivial deleter for these, and google mock complains
        // if they are not deleted.
        auto* tmp = r.get();
        cleanup_readers.push_back(std::move(r));
        return std::unique_ptr<
            grpc::ClientAsyncResponseReaderInterface<btadmin::Table>>(tmp);
      },
      btadmin::GetTableRequest{});

  while (!impl->empty()) impl->SimulateCompletion(true);

  auto actual = fut.get();
  ASSERT_THAT(actual.status(), StatusIs(StatusCode::kUnavailable));
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
