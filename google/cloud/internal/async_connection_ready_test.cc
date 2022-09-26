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

#include "google/cloud/internal/async_connection_ready.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <grpcpp/generic/async_generic_service.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

TEST(CompletionQueueTest, WaitingForFailingConnection) {
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

TEST(CompletionQueueTest, SuccessfulWaitingForConnection) {
  grpc::ServerBuilder builder;
  grpc::AsyncGenericService generic_service;
  builder.RegisterAsyncGenericService(&generic_service);
  int selected_port;
  builder.AddListeningPort("localhost:0", grpc::InsecureServerCredentials(),
                           &selected_port);
  auto srv_cq = builder.AddCompletionQueue();
  std::thread srv_thread([&] {
    bool ok;
    void* placeholder;
    while (srv_cq->Next(&placeholder, &ok))
      ;
  });
  auto server = builder.BuildAndStart();

  CompletionQueue cli_cq;
  std::thread cli_thread([&cli_cq] { cli_cq.Run(); });

  // This test depends on successfully connecting back to itself. Under load,
  // specially in our CI builds, this can fail for reasons that have nothing to
  // do with the correctness of the code. This warmup call avoids most of these
  // problems. We use a custom channel with different attributes to make sure
  // gRPC uses a different socket.
  auto const endpoint = "localhost:" + std::to_string(selected_port);
  grpc::ChannelArguments arguments;
  arguments.SetInt(GRPC_ARG_USE_LOCAL_SUBCHANNEL_POOL, 1);
  auto channel = grpc::CreateCustomChannel(
      endpoint, grpc::InsecureChannelCredentials(), arguments);
  // Most of the time the connection completes within a second. We use a far
  // more generous deadline to avoid flakes.
  auto const warmup_deadline =
      std::chrono::system_clock::now() + std::chrono::seconds(30);
  if (!channel->WaitForConnected(warmup_deadline)) GTEST_SKIP();

  // The connection doesn't try to connect to the endpoint unless there's a
  // method call or `GetState(true)` is called, so we can safely expect IDLE
  // state here.
  channel = grpc::CreateChannel(endpoint, grpc::InsecureChannelCredentials());
  EXPECT_EQ(GRPC_CHANNEL_IDLE, channel->GetState(false));
  auto const deadline =
      std::chrono::system_clock::now() + std::chrono::seconds(30);
  EXPECT_STATUS_OK(cli_cq.AsyncWaitConnectionReady(channel, deadline).get())
      << "state = " << channel->GetState(false);

  server->Shutdown();
  srv_cq->Shutdown();
  srv_thread.join();

  cli_cq.Shutdown();
  cli_thread.join();
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
