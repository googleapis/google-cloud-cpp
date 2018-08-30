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

// We cannot use googlemock directly in this class. The base class has a
// overloaded `operator new` and `operator delete`, which does not call the
// destructor (that is probably a bug in gRPC). Since the destructor is not
// called googlemock thinks the mock is never deleted and the test fails.
// The solution is to delegate on a `Impl` class that is actually deleted.
class MockAsyncResponseReader : public grpc::ClientAsyncResponseReaderInterface<
                                    btproto::MutateRowResponse> {
 public:
  struct Impl {
    MOCK_METHOD0(StartCall, void());
    MOCK_METHOD1(ReadInitialMetadata, void(void*));
    MOCK_METHOD3(Finish,
                 void(btproto::MutateRowResponse*, grpc::Status*, void*));
  };

  MockAsyncResponseReader() : impl(new Impl) {}

  void StartCall() { impl->StartCall(); }
  void ReadInitialMetadata(void* p) { impl->ReadInitialMetadata(p); }
  void Finish(btproto::MutateRowResponse* response, grpc::Status* status,
              void* tag) {
    impl->Finish(response, status, tag);
    // After Finish, we can release the mock implementation.
    impl.reset();
  }

  std::unique_ptr<Impl> impl;
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
  using internal::CompletionQueueImpl::SimulateCompletion;
};

/// @test Verify that completion queues can create async operations.
TEST(CompletionQueueTest, AyncRpcSimple) {
  MockClient client;

  auto reader = google::cloud::internal::make_unique<MockAsyncResponseReader>();
  EXPECT_CALL(*reader->impl, Finish(_, _, _))
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
  impl->SimulateCompletion(cq, op.get(), AsyncOperation::COMPLETED);
  EXPECT_TRUE(completion_called);
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
