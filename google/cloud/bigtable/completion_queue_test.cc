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
      2_ms, [&promise](CompletionQueue& cq, AsyncTimerResult&, bool ok) {
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

  std::promise<bool> promise;
  auto alarm = cq.MakeRelativeTimer(
      50_ms, [&promise](CompletionQueue& cq, AsyncTimerResult&, bool ok) {
        promise.set_value(ok);
      });

  alarm->Cancel();

  auto f = promise.get_future();
  auto status = f.wait_for(100_ms);
  EXPECT_EQ(std::future_status::ready, status);
  EXPECT_FALSE(f.get());

  cq.Shutdown();
  t.join();
}

class MockAsyncResponseReader : public grpc::ClientAsyncResponseReaderInterface<
                                    btproto::MutateRowResponse> {
 public:
  MOCK_METHOD0(StartCall, void());
  MOCK_METHOD1(ReadInitialMetadata, void(void*));
  MOCK_METHOD3(Finish, void(btproto::MutateRowResponse*, grpc::Status*, void*));
};

class MockClient {
 public:
  MOCK_METHOD3(
      AsyncMutateRow,
      std::unique_ptr<
          grpc::ClientAsyncResponseReaderInterface<btproto::MutateRowResponse>>(
          grpc::ClientContext*, btproto::MutateRowRequest const&,
          grpc::CompletionQueue* cq));
};

class MockCompletionQueue : public internal::CompletionQueueImpl {
 public:
  using internal::CompletionQueueImpl::empty;
  using internal::CompletionQueueImpl::SimulateCompletion;
  using internal::CompletionQueueImpl::size;
};

/// @test Verify that completion queues can create async operations.
TEST(CompletionQueueTest, AyncRpcSimple) {
  MockClient client;

  auto reader = google::cloud::internal::make_unique<MockAsyncResponseReader>();
  // Save the raw pointer as we will need it later.
  auto* saved_reader_pointer = reader.get();
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
            btproto::MutateRowResponse>>(reader.release());
      }));

  auto impl = std::make_shared<MockCompletionQueue>();
  bigtable::CompletionQueue cq(impl);

  // In this unit test we do not need to initialize the request parameter.
  btproto::MutateRowRequest request;
  using AsyncResult = AsyncUnaryRpcResult<btproto::MutateRowResponse>;
  auto context = google::cloud::internal::make_unique<grpc::ClientContext>();

  bool completion_called = false;
  auto op = cq.MakeUnaryRpc(
      client, &MockClient::AsyncMutateRow, request, std::move(context),
      [&completion_called](CompletionQueue& cq, AsyncResult& result,
                           AsyncOperation::Disposition d) {
        EXPECT_EQ(d, AsyncOperation::COMPLETED);
        EXPECT_TRUE(result.status.ok());
        EXPECT_EQ("mocked-status", result.status.error_message());
        completion_called = true;
      });
  EXPECT_EQ(1U, impl->size());
  impl->SimulateCompletion(cq, op.get(), AsyncOperation::COMPLETED);
  EXPECT_TRUE(completion_called);

  EXPECT_TRUE(impl->empty());

  // This is a terrible, horrible, no good, very bad hack: the gRPC library
  // specializes
  // `std::default_delete<grpc::ClientAsyncResponseReaderInterface<R>>`, the
  // implementation in this specialization does nothing:
  //
  //     https://github.com/grpc/grpc/blob/608188c680961b8506847c135b5170b41a9081e8/include/grpcpp/impl/codegen/async_unary_call.h#L305
  //
  // No delete, no destructor, nothing. The gRPC library expects all
  // `grpc::ClientAsyncResponseReader<R>` objects to be allocated from a
  // per-call arena, and deleted in bulk with other objects when the call
  // completes and the full arena is released.  Unfortunately, our mocks are
  // allocated from the global heap, as they do not have an associated call or
  // arena. The override in the gRPC library results in a leak, unless we manage
  // the memory explicitly.
  if (not reader) reader.reset(saved_reader_pointer);
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
