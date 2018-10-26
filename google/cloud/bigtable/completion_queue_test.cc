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
#include "google/cloud/testing_util/chrono_literals.h"
#include <google/bigtable/v2/bigtable.grpc.pb.h>
#include <gmock/gmock.h>
#include <future>

using namespace google::cloud::testing_util::chrono_literals;
namespace btproto = google::bigtable::v2;

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
  auto status = f.wait_for(50_ms);
  EXPECT_EQ(std::future_status::ready, status);

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
  auto status = f.wait_for(100_ms);
  EXPECT_EQ(std::future_status::ready, status);
  EXPECT_TRUE(f.get().cancelled);

  cq.Shutdown();
  t.join();
}

class MockClient {
 public:
  MOCK_METHOD3(
      AsyncMutateRow,
      std::unique_ptr<
          grpc::ClientAsyncResponseReaderInterface<btproto::MutateRowResponse>>(
          grpc::ClientContext*, btproto::MutateRowRequest const&,
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
/// @test Verify that completion queues can create async operations.
TEST(CompletionQueueTest, AyncRpcSimple) {
  MockClient client;

  auto reader =
      google::cloud::internal::make_unique<testing::MockAsyncApplyReader>();
  EXPECT_CALL(*reader, Finish(_, _, _))
      .WillOnce(
          Invoke([](btproto::MutateRowResponse*, grpc::Status* status, void*) {
            *status = grpc::Status(grpc::StatusCode::OK, "mocked-status");
          }));

  EXPECT_CALL(client, AsyncMutateRow(_, _, _))
      .WillOnce(Invoke([&reader](grpc::ClientContext*,
                                 btproto::MutateRowRequest const&,
                                 grpc::CompletionQueue*) {
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            // This is safe, see comments in MockAsyncResponseReader.
            btproto::MutateRowResponse>>(reader.get());
      }));

  auto impl = std::make_shared<testing::MockCompletionQueue>();
  bigtable::CompletionQueue cq(impl);

  // In this unit test we do not need to initialize the request parameter.
  btproto::MutateRowRequest request;
  auto context = google::cloud::internal::make_unique<grpc::ClientContext>();

  bool completion_called = false;
  auto op = cq.MakeUnaryRpc(
      client, &MockClient::AsyncMutateRow, request, std::move(context),
      [&completion_called](CompletionQueue& cq,
                           btproto::MutateRowResponse& response,
                           grpc::Status& status) {
        EXPECT_TRUE(status.ok());
        EXPECT_EQ("mocked-status", status.error_message());
        completion_called = true;
      });
  EXPECT_EQ(1U, impl->size());
  impl->SimulateCompletion(cq, op.get(), true);
  EXPECT_TRUE(completion_called);

  EXPECT_TRUE(impl->empty());
}

/// @test Verify that completion queues can create async operations with
//        streamed responses.
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
      .WillOnce(Invoke([](grpc::Status* status, void*) {
        *status = grpc::Status(grpc::StatusCode::OK, "mocked-status");
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
        EXPECT_EQ("mocked-status", result.error_message());
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

/// @test Verify that async streams which fail to get created are properly
//        handled.
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

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
