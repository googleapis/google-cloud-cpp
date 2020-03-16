// Copyright 2020 Google LLC.
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

#include "google/cloud/spanner/internal/async_retry_unary_rpc.h"
#include "google/cloud/spanner/testing/mock_completion_queue.h"
#include "google/cloud/spanner/testing/mock_response_reader.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include <google/spanner/v1/spanner.grpc.pb.h>
#include <gmock/gmock.h>
#include <thread>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {
namespace {

namespace spanner_proto = ::google::spanner::v1;
using ::testing::_;
using ::testing::Invoke;

// This allows us to use constants like `10_s`, `40_us`, etc.
// NOLINTNEXTLINE(google-build-using-namespace)
using namespace google::cloud::testing_util::chrono_literals;

class MockClient {
 public:
  MOCK_METHOD3(
      AsyncGetSession,
      std::unique_ptr<
          grpc::ClientAsyncResponseReaderInterface<spanner_proto::Session>>(
          grpc::ClientContext* client_context,
          spanner_proto::GetSessionRequest const& request,
          grpc::CompletionQueue* cq));
};

TEST(AsyncRetryUnaryRpcTest, ImmediatelySucceeds) {
  MockClient client;

  using ReaderType = ::google::cloud::spanner::testing::MockAsyncResponseReader<
      spanner_proto::Session>;
  auto reader = google::cloud::internal::make_unique<ReaderType>();
  EXPECT_CALL(*reader, Finish(_, _, _))
      .WillOnce(Invoke(
          [](spanner_proto::Session* session, grpc::Status* status, void*) {
            // Initialize a value to make sure it is carried all the way back to
            // the caller.
            session->set_name("fake/session/name/response");
            *status = grpc::Status::OK;
          }));

  EXPECT_CALL(client, AsyncGetSession(_, _, _))
      .WillOnce(
          Invoke([&reader](grpc::ClientContext*,
                           spanner_proto::GetSessionRequest const& request,
                           grpc::CompletionQueue*) {
            EXPECT_EQ("fake/session/name/request", request.name());
            return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                // This is safe, see comments in MockAsyncResponseReader.
                spanner_proto::Session>>(reader.get());
          }));

  auto impl = std::make_shared<testing::MockCompletionQueue>();
  CompletionQueue cq(impl);

  // Do some basic initialization of the request to verify the values get
  // carried to the mock.
  spanner_proto::GetSessionRequest request;
  request.set_name("fake/session/name/request");

  auto fut = StartRetryAsyncUnaryRpc(
      __func__, LimitedErrorCountRetryPolicy(3).clone(),
      ExponentialBackoffPolicy(10_us, 40_us, /*scaling=*/2).clone(),
      ConstantIdempotencyPolicy(true),
      [&client](grpc::ClientContext* context,
                spanner_proto::GetSessionRequest const& request,
                grpc::CompletionQueue* cq) {
        return client.AsyncGetSession(context, request, cq);
      },
      request, cq);

  EXPECT_EQ(1, impl->size());
  impl->SimulateCompletion(true);

  EXPECT_TRUE(impl->empty());
  EXPECT_EQ(std::future_status::ready, fut.wait_for(0_us));
  auto result = fut.get();
  ASSERT_STATUS_OK(result);
  EXPECT_EQ("fake/session/name/response", result->name());
}

TEST(AsyncRetryUnaryRpcTest, PermanentFailure) {
  MockClient client;

  using ReaderType = ::google::cloud::spanner::testing::MockAsyncResponseReader<
      spanner_proto::Session>;
  auto reader = google::cloud::internal::make_unique<ReaderType>();
  EXPECT_CALL(*reader, Finish(_, _, _))
      .WillOnce(Invoke([](spanner_proto::Session*, grpc::Status* status,
                          void*) {
        *status = grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh-oh");
      }));

  EXPECT_CALL(client, AsyncGetSession(_, _, _))
      .WillOnce(
          Invoke([&reader](grpc::ClientContext*,
                           spanner_proto::GetSessionRequest const& request,
                           grpc::CompletionQueue*) {
            EXPECT_EQ("fake/session/name/request", request.name());
            return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                // This is safe, see comments in MockAsyncResponseReader.
                spanner_proto::Session>>(reader.get());
          }));

  auto impl = std::make_shared<testing::MockCompletionQueue>();
  CompletionQueue cq(impl);

  // Do some basic initialization of the request to verify the values get
  // carried to the mock.
  spanner_proto::GetSessionRequest request;
  request.set_name("fake/session/name/request");

  auto fut = StartRetryAsyncUnaryRpc(
      __func__, LimitedErrorCountRetryPolicy(3).clone(),
      ExponentialBackoffPolicy(10_us, 40_us, /*scaling=*/2).clone(),
      ConstantIdempotencyPolicy(true),
      [&client](grpc::ClientContext* context,
                spanner_proto::GetSessionRequest const& request,
                grpc::CompletionQueue* cq) {
        return client.AsyncGetSession(context, request, cq);
      },
      request, cq);

  EXPECT_EQ(1, impl->size());
  impl->SimulateCompletion(true);

  EXPECT_TRUE(impl->empty());
  EXPECT_EQ(std::future_status::ready, fut.wait_for(0_us));
  auto result = fut.get();
  EXPECT_FALSE(result);
  EXPECT_EQ(StatusCode::kPermissionDenied, result.status().code());
}

TEST(AsyncRetryUnaryRpcTest, TooManyTransientFailures) {
  MockClient client;

  using ReaderType = ::google::cloud::spanner::testing::MockAsyncResponseReader<
      spanner_proto::Session>;

  auto finish_failure = [](spanner_proto::Session*, grpc::Status* status,
                           void*) {
    *status = grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
  };

  auto r1 = google::cloud::internal::make_unique<ReaderType>();
  EXPECT_CALL(*r1, Finish(_, _, _)).WillOnce(Invoke(finish_failure));
  auto r2 = google::cloud::internal::make_unique<ReaderType>();
  EXPECT_CALL(*r2, Finish(_, _, _)).WillOnce(Invoke(finish_failure));
  auto r3 = google::cloud::internal::make_unique<ReaderType>();
  EXPECT_CALL(*r3, Finish(_, _, _)).WillOnce(Invoke(finish_failure));

  EXPECT_CALL(client, AsyncGetSession(_, _, _))
      .WillOnce(Invoke([&r1](grpc::ClientContext*,
                             spanner_proto::GetSessionRequest const& request,
                             grpc::CompletionQueue*) {
        EXPECT_EQ("fake/session/name/request", request.name());
        return std::unique_ptr<
            grpc::ClientAsyncResponseReaderInterface<spanner_proto::Session>>(
            r1.get());
      }))
      .WillOnce(Invoke([&r2](grpc::ClientContext*,
                             spanner_proto::GetSessionRequest const& request,
                             grpc::CompletionQueue*) {
        EXPECT_EQ("fake/session/name/request", request.name());
        return std::unique_ptr<
            grpc::ClientAsyncResponseReaderInterface<spanner_proto::Session>>(
            r2.get());
      }))
      .WillOnce(Invoke([&r3](grpc::ClientContext*,
                             spanner_proto::GetSessionRequest const& request,
                             grpc::CompletionQueue*) {
        EXPECT_EQ("fake/session/name/request", request.name());
        return std::unique_ptr<
            grpc::ClientAsyncResponseReaderInterface<spanner_proto::Session>>(
            r3.get());
      }));

  auto impl = std::make_shared<testing::MockCompletionQueue>();
  CompletionQueue cq(impl);

  // Do some basic initialization of the request to verify the values get
  // carried to the mock.
  spanner_proto::GetSessionRequest request;
  request.set_name("fake/session/name/request");

  auto fut = StartRetryAsyncUnaryRpc(
      __func__, LimitedErrorCountRetryPolicy(2).clone(),
      ExponentialBackoffPolicy(10_us, 40_us, /*scaling=*/2).clone(),
      ConstantIdempotencyPolicy(true),
      [&client](grpc::ClientContext* context,
                spanner_proto::GetSessionRequest const& request,
                grpc::CompletionQueue* cq) {
        return client.AsyncGetSession(context, request, cq);
      },
      request, cq);

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

  EXPECT_EQ(std::future_status::ready, fut.wait_for(0_us));
  auto result = fut.get();
  EXPECT_FALSE(result);
  EXPECT_EQ(StatusCode::kUnavailable, result.status().code());
}

}  // namespace
}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
