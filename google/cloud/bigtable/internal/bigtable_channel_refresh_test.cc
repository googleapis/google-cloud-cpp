// Copyright 2022 Google LLC
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

#include "google/cloud/bigtable/internal/bigtable_channel_refresh.h"
#include "google/cloud/bigtable/internal/bigtable_stub_factory.h"
#include "google/cloud/bigtable/options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include <gmock/gmock.h>
#include <chrono>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ms = std::chrono::milliseconds;
using ::google::cloud::testing_util::MockCompletionQueueImpl;
using ::testing::ReturnRef;

auto constexpr kTestChannels = 3;

TEST(BigtableChannelRefresh, Enabled) {
  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  grpc::CompletionQueue grpc_cq;
  EXPECT_CALL(*mock_cq, cq).WillRepeatedly(ReturnRef(grpc_cq));

  // Mock the call to `NotifyOnStateChange::Start()` for each channel
  EXPECT_CALL(*mock_cq, StartOperation)
      .Times(kTestChannels)
      .WillRepeatedly(
          [](std::shared_ptr<internal::AsyncGrpcOperation> const& op,
             absl::FunctionRef<void(void*)>) {
            // False means no state change in the underlying gRPC channel. This
            // will break the refresh loop.
            op->Notify(false);
          });

  CompletionQueue cq(mock_cq);
  auto stub = CreateBigtableStub(
      cq, Options{}
              .set<GrpcNumChannelsOption>(kTestChannels)
              .set<bigtable::MaxConnectionRefreshOption>(ms(1000)));
}

TEST(BigtableChannelRefresh, Disabled) {
  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();

  // The CQ should only do work if we have to set up the channel refresh.
  EXPECT_CALL(*mock_cq, StartOperation).Times(0);

  CompletionQueue cq(mock_cq);
  auto stub = CreateBigtableStub(
      cq, Options{}.set<bigtable::MaxConnectionRefreshOption>(ms(0)));
}

TEST(BigtableChannelRefresh, Continuations) {
  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  grpc::CompletionQueue grpc_cq;
  EXPECT_CALL(*mock_cq, cq).WillRepeatedly(ReturnRef(grpc_cq));

  ::testing::InSequence s;
  // Mock the call to `NotifyOnStateChange::Start()` for the single channel
  EXPECT_CALL(*mock_cq, StartOperation)
      .WillOnce([](std::shared_ptr<internal::AsyncGrpcOperation> const& op,
                   absl::FunctionRef<void(void*)>) {
        // The state has changed. We should schedule another channel refresh.
        op->Notify(true);
      })
      .WillOnce([](std::shared_ptr<internal::AsyncGrpcOperation> const& op,
                   absl::FunctionRef<void(void*)>) {
        // False means no state change in the underlying gRPC channel. This
        // will break the refresh loop.
        op->Notify(false);
      });

  CompletionQueue cq(mock_cq);
  auto stub = CreateBigtableStub(
      cq, Options{}
              .set<UnifiedCredentialsOption>(MakeInsecureCredentials())
              .set<GrpcNumChannelsOption>(1)
              .set<bigtable::MaxConnectionRefreshOption>(ms(1000)));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
