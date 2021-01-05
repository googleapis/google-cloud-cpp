// Copyright 2020 Google LLC
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

#include "google/cloud/internal/async_connection_ready.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/fake_completion_queue_impl.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

class CompletionQueueTest : public ::testing::Test {
 protected:
  static void CallNotify(AsyncConnectionReadyFuture& op, bool ok) {
    op.Notify(ok);
  }
};

namespace {

TEST_F(CompletionQueueTest, WaitingForFailingConnection) {
  auto channel = grpc::CreateChannel("some_nonexistent.address",
                                     grpc::InsecureChannelCredentials());
  CompletionQueue cq;
  std::thread t([&cq] { cq.Run(); });

  // The connection doesn't try to connect to the endpoint unless there's a
  // method call or `GetState(true)` is called, so we can safely expect IDLE
  // state here.
  EXPECT_EQ(GRPC_CHANNEL_IDLE, channel->GetState(false));

  EXPECT_EQ(
      StatusCode::kDeadlineExceeded,
      cq.AsyncWaitConnectionReady(
            channel, std::chrono::system_clock::now() + std::chrono::seconds(1))
          .get()
          .code());

  cq.Shutdown();
  t.join();
}

TEST_F(CompletionQueueTest, SuccessfulWaitingForConnection) {
  grpc::ServerBuilder builder;
  grpc::AsyncGenericService generic_service;
  builder.RegisterAsyncGenericService(&generic_service);
  int selected_port;
  builder.AddListeningPort("localhost:0", grpc::InsecureServerCredentials(),
                           &selected_port);
  auto srv_cq = builder.AddCompletionQueue();
  std::thread srv_thread([&srv_cq] {
    bool ok;
    void* dummy;
    while (srv_cq->Next(&dummy, &ok))
      ;
  });
  auto server = builder.BuildAndStart();

  CompletionQueue cli_cq;
  std::thread cli_thread([&cli_cq] { cli_cq.Run(); });

  auto channel =
      grpc::CreateChannel("localhost:" + std::to_string(selected_port),
                          grpc::InsecureChannelCredentials());

  // The connection doesn't try to connect to the endpoint unless there's a
  // method call or `GetState(true)` is called, so we can safely expect IDLE
  // state here.
  EXPECT_EQ(GRPC_CHANNEL_IDLE, channel->GetState(false));
  EXPECT_STATUS_OK(
      cli_cq
          .AsyncWaitConnectionReady(channel, std::chrono::system_clock::now() +
                                                 std::chrono::seconds(1))
          .get());

  srv_cq->Shutdown();
  srv_thread.join();

  cli_cq.Shutdown();
  cli_thread.join();
}

TEST_F(CompletionQueueTest, DisappearingCompletionQueue) {
  auto channel = grpc::CreateChannel("some_nonexistent.address",
                                     grpc::InsecureChannelCredentials());
  auto cq_impl = std::make_shared<
      ::google::cloud::testing_util::FakeCompletionQueueImpl>();
  AsyncConnectionReadyFuture op(
      channel, std::chrono::system_clock::now() + std::chrono::hours(10),
      cq_impl);
  cq_impl.reset();
  // The connection doesn't try to connect to the endpoint unless there's a
  // method call or `GetState(true)` is called, so we can safely expect IDLE
  // state here.
  EXPECT_EQ(GRPC_CHANNEL_IDLE, channel->GetState(false));
  CallNotify(op, true);
  auto status = op.GetFuture().get();
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(StatusCode::kCancelled, status.code());
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
