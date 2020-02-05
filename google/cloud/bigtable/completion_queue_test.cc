// Copyright 2018 Google LLC
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

#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/bigtable/testing/mock_completion_queue.h"
#include "google/cloud/bigtable/testing/mock_mutate_rows_reader.h"
#include "google/cloud/internal/make_unique.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include <google/bigtable/admin/v2/bigtable_table_admin.grpc.pb.h>
#include <google/bigtable/v2/bigtable.grpc.pb.h>
#include <gmock/gmock.h>
#include <future>

using namespace google::cloud::testing_util::chrono_literals;
namespace btproto = google::bigtable::v2;
namespace btadmin = google::bigtable::admin::v2;

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

/// @test Verify the basic lifecycle of a completion queue.
TEST(CompletionQueueTest, LifeCycleFuture) {
  CompletionQueue cq;

  std::thread t([&cq]() { cq.Run(); });

  promise<bool> promise;
  cq.MakeRelativeTimer(2_ms).then(
      [&promise](future<StatusOr<std::chrono::system_clock::time_point>>) {
        promise.set_value(true);
      });

  auto f = promise.get_future();
  auto status = f.wait_for(500_ms);
  EXPECT_EQ(std::future_status::ready, status);
  EXPECT_TRUE(f.get());

  cq.Shutdown();
  t.join();
}

class MockClient {
 public:
  // Use an operation with simple request / response parameters, so it is easy
  // to test them.
  MOCK_METHOD3(
      AsyncGetTable,
      std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<btadmin::Table>>(
          grpc::ClientContext*, btadmin::GetTableRequest const&,
          grpc::CompletionQueue* cq));

  MOCK_METHOD4(
      AsyncMutateRows,
      std::unique_ptr<
          grpc::ClientAsyncReaderInterface<btproto::MutateRowsResponse>>(
          grpc::ClientContext* context,
          const ::google::bigtable::v2::MutateRowsRequest& request,
          grpc::CompletionQueue* cq, void* tag));
};

/// @test Verify that completion queues can create async operations returning
/// futures.
TEST(CompletionQueueTest, AsyncRpcSimpleFuture) {
  MockClient client;

  using ReaderType =
      ::google::cloud::bigtable::testing::MockAsyncResponseReader<
          btadmin::Table>;
  auto reader = google::cloud::internal::make_unique<ReaderType>();
  EXPECT_CALL(*reader, Finish(_, _, _))
      .WillOnce(Invoke([](btadmin::Table* table, grpc::Status* status, void*) {
        // Initialize a value to make sure it is carried all the way back to
        // the caller.
        table->set_name("fake/table/name/response");
        *status = grpc::Status::OK;
      }));

  EXPECT_CALL(client, AsyncGetTable(_, _, _))
      .WillOnce(Invoke([&reader](grpc::ClientContext*,
                                 btadmin::GetTableRequest const& request,
                                 grpc::CompletionQueue*) {
        EXPECT_EQ("fake/table/name/request", request.name());
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            // This is safe, see comments in MockAsyncResponseReader.
            btadmin::Table>>(reader.get());
      }));

  auto impl = std::make_shared<testing::MockCompletionQueue>();
  bigtable::CompletionQueue cq(impl);

  // Do some basic initialization of the request to verify the values get
  // carried to the mock.
  btadmin::GetTableRequest request;
  request.set_name("fake/table/name/request");
  auto context = google::cloud::internal::make_unique<grpc::ClientContext>();

  auto future = cq.MakeUnaryRpc(
      [&client](grpc::ClientContext* context,
                btadmin::GetTableRequest const& request,
                grpc::CompletionQueue* cq) {
        return client.AsyncGetTable(context, request, cq);
      },
      request, std::move(context));

  EXPECT_EQ(1, impl->size());
  impl->SimulateCompletion(true);
  EXPECT_TRUE(impl->empty());

  ASSERT_EQ(std::future_status::ready, future.wait_for(0_ms));
  auto response = future.get();
  ASSERT_STATUS_OK(response);
  EXPECT_EQ("fake/table/name/response", response->name());
}

/// @test Verify that completion queues can create async operations returning
/// futures.
TEST(CompletionQueueTest, AsyncRpcSimpleFutureFailure) {
  MockClient client;

  using ReaderType =
      ::google::cloud::bigtable::testing::MockAsyncResponseReader<
          btadmin::Table>;
  auto reader = google::cloud::internal::make_unique<ReaderType>();
  EXPECT_CALL(*reader, Finish(_, _, _))
      .WillOnce(Invoke([](btadmin::Table*, grpc::Status* status, void*) {
        *status = grpc::Status(grpc::StatusCode::NOT_FOUND, "not found");
      }));

  EXPECT_CALL(client, AsyncGetTable(_, _, _))
      .WillOnce(Invoke([&reader](grpc::ClientContext*,
                                 btadmin::GetTableRequest const&,
                                 grpc::CompletionQueue*) {
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            // This is safe, see comments in MockAsyncResponseReader.
            btadmin::Table>>(reader.get());
      }));

  auto impl = std::make_shared<testing::MockCompletionQueue>();
  bigtable::CompletionQueue cq(impl);

  // In this unit test we do not need to initialize the request parameter.
  btadmin::GetTableRequest request;
  auto context = google::cloud::internal::make_unique<grpc::ClientContext>();

  auto future = cq.MakeUnaryRpc(
      [&client](grpc::ClientContext* context,
                btadmin::GetTableRequest const& request,
                grpc::CompletionQueue* cq) {
        return client.AsyncGetTable(context, request, cq);
      },
      request, std::move(context));

  EXPECT_EQ(1, impl->size());
  impl->SimulateCompletion(true);
  EXPECT_TRUE(impl->empty());

  ASSERT_EQ(std::future_status::ready, future.wait_for(0_ms));
  StatusOr<btadmin::Table> response = future.get();
  EXPECT_FALSE(response.ok());
  EXPECT_EQ(StatusCode::kNotFound, response.status().code());
  EXPECT_EQ("not found", response.status().message());
}

/// @test Verify that completion queues can invoke a custom function in the
/// event loop.
TEST(CompletionQueueTest, Noop) {
  bigtable::CompletionQueue cq;

  std::thread t([&cq]() { cq.Run(); });

  std::promise<void> promise;
  cq.RunAsync([&promise](CompletionQueue&) { promise.set_value(); });

  promise.get_future().get();

  cq.Shutdown();
  t.join();
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
