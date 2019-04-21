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
TEST(CompletionQueueTest, LifeCycle) {
  CompletionQueue cq;

  std::thread t([&cq]() { cq.Run(); });

  std::promise<bool> promise;
  auto alarm = cq.MakeRelativeTimer(
      2_ms, [&promise](CompletionQueue& cq, AsyncTimerResult&) {
        promise.set_value(true);
      });

  auto f = promise.get_future();
  auto status = f.wait_for(500_ms);
  EXPECT_EQ(std::future_status::ready, status);

  cq.Shutdown();
  t.join();
}

TEST(CompletionQueueTest, LifeCycleFuture) {
  CompletionQueue cq;

  std::thread t([&cq]() { cq.Run(); });

  promise<bool> promise;
  cq.MakeRelativeTimer(2_ms).then(
      [&promise](future<std::chrono::system_clock::time_point>) {
        promise.set_value(true);
      });

  auto f = promise.get_future();
  auto status = f.wait_for(500_ms);
  EXPECT_EQ(std::future_status::ready, status);
  EXPECT_TRUE(f.get());

  cq.Shutdown();
  t.join();
}

/// @test Verify that we can cancel alarms.
TEST(CompletionQueueTest, CancelAlarm) {
  bigtable::CompletionQueue cq;

  std::thread t([&cq]() { cq.Run(); });

  std::promise<AsyncTimerResult> promise;
  auto alarm = cq.MakeRelativeTimer(
      50_ms, [&promise](CompletionQueue& cq, AsyncTimerResult& result) {
        promise.set_value(result);
      });

  alarm->Cancel();

  auto f = promise.get_future();
  auto status = f.wait_for(500_ms);
  EXPECT_EQ(std::future_status::ready, status);
  EXPECT_TRUE(f.get().cancelled);

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

template <typename Response>
class MockClientAsyncReaderInterface
    : public grpc::ClientAsyncReaderInterface<Response> {
 public:
  MOCK_METHOD1(StartCall, void(void*));
  MOCK_METHOD1(ReadInitialMetadata, void(void*));
  MOCK_METHOD2(Finish, void(grpc::Status*, void*));
  MOCK_METHOD2_T(Read, void(Response*, void*));
};

/// @test Verify that completion queues can create async operations with
/// callbacks.
TEST(CompletionQueueTest, AsyncRpcSimple) {
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

  bool completion_called = false;
  auto op = cq.MakeUnaryRpc(
      client, &MockClient::AsyncGetTable, request, std::move(context),
      [&completion_called](CompletionQueue& cq, btadmin::Table& response,
                           grpc::Status& status) {
        EXPECT_TRUE(status.ok());
        EXPECT_EQ("fake/table/name/response", response.name());
        completion_called = true;
      });
  EXPECT_EQ(1U, impl->size());
  impl->SimulateCompletion(cq, op.get(), true);
  EXPECT_TRUE(completion_called);

  EXPECT_TRUE(impl->empty());
}

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

  EXPECT_EQ(1U, impl->size());
  impl->SimulateCompletion(cq, true);
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

  EXPECT_EQ(1U, impl->size());
  impl->SimulateCompletion(cq, true);
  EXPECT_TRUE(impl->empty());

  ASSERT_EQ(std::future_status::ready, future.wait_for(0_ms));
  StatusOr<btadmin::Table> response = future.get();
  EXPECT_FALSE(response.ok());
  EXPECT_EQ(StatusCode::kNotFound, response.status().code());
  EXPECT_EQ("not found", response.status().message());
}

/// @test Verify that completion queues can create async operations for
/// streaming read RPCs.
TEST(CompletionQueueTest, AsyncRpcSimpleStream) {
  MockClient client;

  // Normally, reader is created by the client and returned as unique_ptr, but I
  // want to mock it, so I create it here. Eventually, I'll return it as a
  // unique_ptr from the client, but before that, I hold it in reader_deleter in
  // case something goes wrong.
  MockClientAsyncReaderInterface<btproto::MutateRowsResponse>* reader =
      new MockClientAsyncReaderInterface<btproto::MutateRowsResponse>;
  std::unique_ptr<MockClientAsyncReaderInterface<btproto::MutateRowsResponse>>
      reader_deleter(reader);

  EXPECT_CALL(*reader, Read(_, _))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r, void*) {
        r->add_entries()->set_index(0);
      }))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r, void*) {
        r->add_entries()->set_index(1);
      }))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r, void*) {
        r->add_entries()->set_index(2);
      }));
  EXPECT_CALL(*reader, Finish(_, _))
      .WillOnce(Invoke(
          [](grpc::Status* status, void*) { *status = grpc::Status::OK; }));

  EXPECT_CALL(client, AsyncMutateRows(_, _, _, _))
      .WillOnce(Invoke([&reader_deleter](grpc::ClientContext*,
                                         btproto::MutateRowsRequest const&,
                                         grpc::CompletionQueue*, void*) {
        auto tmp = google::cloud::internal::make_unique<
            MockClientAsyncReaderInterface<btproto::MutateRowsResponse>>();
        return std::move(reader_deleter);
      }));

  auto impl = std::make_shared<testing::MockCompletionQueue>();
  bigtable::CompletionQueue cq(impl);

  // In this unit test we do not need to initialize the request parameter.
  btproto::MutateRowsRequest request;
  auto context = google::cloud::internal::make_unique<grpc::ClientContext>();

  bool completion_called = false;
  int cur_idx = 0;
  auto op = cq.MakeUnaryStreamRpc(
      client, &MockClient::AsyncMutateRows, request, std::move(context),
      [&cur_idx](CompletionQueue&, const grpc::ClientContext&,
                 btproto::MutateRowsResponse& resp) {
        EXPECT_EQ(1, resp.entries().size());
        EXPECT_EQ(cur_idx++, resp.entries(0).index());
      },
      [&completion_called](CompletionQueue&, grpc::ClientContext&,
                           grpc::Status& result) {
        EXPECT_TRUE(result.ok());
        completion_called = true;
      });
  // Initially stream is in CREATING state
  EXPECT_EQ(1U, impl->size());
  impl->SimulateCompletion(cq, op.get(), true);
  // Now the stream should now be in PROCESSING state
  EXPECT_EQ(0, cur_idx);
  EXPECT_EQ(1U, impl->size());
  impl->SimulateCompletion(cq, op.get(), true);
  EXPECT_EQ(1, cur_idx);
  EXPECT_EQ(1U, impl->size());
  impl->SimulateCompletion(cq, op.get(), true);
  EXPECT_EQ(2, cur_idx);
  EXPECT_EQ(1U, impl->size());
  impl->SimulateCompletion(cq, op.get(), false);
  // Stream should now be in FINISHING state, the next completion should be
  // translated into calling the "finished" callback.
  EXPECT_EQ(2, cur_idx);
  EXPECT_EQ(1U, impl->size());
  EXPECT_FALSE(completion_called);
  impl->SimulateCompletion(cq, op.get(), false);
  EXPECT_EQ(2, cur_idx);
  EXPECT_TRUE(impl->empty());
  EXPECT_TRUE(completion_called);
}

/// @test Verify that completion queues properly handle errors when creating
/// streaming read RPCS.
TEST(CompletionQueueTest, AsyncRpcStreamNotCreated) {
  MockClient client;

  // Normally, reader is created by the client and returned as unique_ptr, but I
  // want to mock it, so I create it here. Eventually, I'll return it as a
  // unique_ptr from the client, but before that, I hold it in reader_deleter in
  // case something goes wrong.
  MockClientAsyncReaderInterface<btproto::MutateRowsResponse>* reader =
      new MockClientAsyncReaderInterface<btproto::MutateRowsResponse>;
  std::unique_ptr<MockClientAsyncReaderInterface<btproto::MutateRowsResponse>>
      reader_deleter(reader);

  EXPECT_CALL(*reader, Finish(_, _))
      .WillOnce(Invoke([](grpc::Status* status, void*) {
        *status = grpc::Status(grpc::StatusCode::UNAVAILABLE, "mocked-status");
      }));

  EXPECT_CALL(client, AsyncMutateRows(_, _, _, _))
      .WillOnce(Invoke([&reader_deleter](grpc::ClientContext*,
                                         btproto::MutateRowsRequest const&,
                                         grpc::CompletionQueue*, void*) {
        auto tmp = google::cloud::internal::make_unique<
            MockClientAsyncReaderInterface<btproto::MutateRowsResponse>>();
        return std::move(reader_deleter);
      }));

  auto impl = std::make_shared<testing::MockCompletionQueue>();
  bigtable::CompletionQueue cq(impl);

  // In this unit test we do not need to initialize the request parameter.
  btproto::MutateRowsRequest request;
  auto context = google::cloud::internal::make_unique<grpc::ClientContext>();

  bool completion_called = false;
  auto op = cq.MakeUnaryStreamRpc(
      client, &MockClient::AsyncMutateRows, request, std::move(context),
      [](CompletionQueue&, const grpc::ClientContext&,
         btproto::MutateRowsResponse& resp) { EXPECT_TRUE(false); },
      [&completion_called](CompletionQueue&, grpc::ClientContext&,
                           grpc::Status& result) {
        EXPECT_FALSE(result.ok());
        EXPECT_EQ("mocked-status", result.error_message());
        completion_called = true;
      });
  // Initially stream is in CREATING state
  EXPECT_EQ(1U, impl->size());
  impl->SimulateCompletion(cq, op.get(), false);
  // Now the stream should now be in FINISHING state
  EXPECT_EQ(1U, impl->size());
  EXPECT_FALSE(completion_called);
  impl->SimulateCompletion(cq, op.get(), false);
  EXPECT_TRUE(impl->empty());
  EXPECT_TRUE(completion_called);
}

/// @test Verify that completion queues can invoke a custom function in the
/// event loop.
TEST(CompletionQueueTest, Noop) {
  bigtable::CompletionQueue cq;

  std::thread t([&cq]() { cq.Run(); });

  std::promise<void> promise;
  auto alarm =
      cq.RunAsync([&promise](CompletionQueue& cq) { promise.set_value(); });

  promise.get_future().get();

  cq.Shutdown();
  t.join();
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
