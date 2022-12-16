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

using ::google::cloud::testing_util::MockCompletionQueueImpl;

auto constexpr kTestChannels = 3;

TEST(BigtableChannelRefresh, Enabled) {
  using ms = std::chrono::milliseconds;
  using ns = std::chrono::nanoseconds;

  auto mock = std::make_shared<MockCompletionQueueImpl>();

  // We call `MakeRelativeTimer` once per channel when registering the refresh
  // timers and `RunAsync` once per channel when deregistering them.
  EXPECT_CALL(*mock, RunAsync).Times(kTestChannels);
  EXPECT_CALL(*mock, MakeRelativeTimer)
      .Times(kTestChannels)
      .WillRepeatedly([](std::chrono::nanoseconds duration) {
        EXPECT_LE(ns(ms(500)).count(), duration.count());
        EXPECT_LE(duration.count(), ns(ms(1000)).count());
        return make_ready_future<
            StatusOr<std::chrono::system_clock::time_point>>(
            Status(StatusCode::kCancelled, "cancelled"));
      });

  CompletionQueue cq(mock);
  auto stub = CreateBigtableStub(
      cq, Options{}
              .set<GrpcNumChannelsOption>(kTestChannels)
              .set<bigtable::MinConnectionRefreshOption>(ms(500))
              .set<bigtable::MaxConnectionRefreshOption>(ms(1000)));
}

TEST(BigtableChannelRefresh, Disabled) {
  using ms = std::chrono::milliseconds;

  auto mock = std::make_shared<MockCompletionQueueImpl>();

  // The CQ should only do work if we have to set up the channel refresh.
  EXPECT_CALL(*mock, RunAsync).Times(0);
  EXPECT_CALL(*mock, MakeRelativeTimer).Times(0);

  CompletionQueue cq(mock);
  auto stub = CreateBigtableStub(
      cq, Options{}.set<bigtable::MaxConnectionRefreshOption>(ms(0)));
}

TEST(BigtableChannelRefresh, Continuations) {
  using ms = std::chrono::milliseconds;
  using ns = std::chrono::nanoseconds;
  using ::testing::Return;

  promise<StatusOr<std::chrono::system_clock::time_point>> p;

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  grpc::CompletionQueue grpc_cq;
  EXPECT_CALL(*mock_cq, cq).WillRepeatedly(Return(&grpc_cq));

  ::testing::InSequence s;
  EXPECT_CALL(*mock_cq, MakeRelativeTimer)
      .WillOnce([&p](std::chrono::nanoseconds duration) {
        EXPECT_LE(ns(ms(500)).count(), duration.count());
        EXPECT_LE(duration.count(), ns(ms(1000)).count());
        // This is a regression test for
        // https://github.com/googleapis/google-cloud-cpp/issues/9712
        //
        // We delay returning the future until the Stub is fully constructed.
        return p.get_future();
      });
  // Mock the call to `CQ::AsyncWaitConnectionReady()`
  EXPECT_CALL(*mock_cq, StartOperation)
      .WillOnce([](std::shared_ptr<internal::AsyncGrpcOperation> const& op,
                   absl::FunctionRef<void(void*)>) {
        // False means no state change in the underlying gRPC channel
        op->Notify(false);
      });
  // We should schedule another channel refresh.
  EXPECT_CALL(*mock_cq, MakeRelativeTimer)
      .WillOnce([](std::chrono::nanoseconds duration) {
        EXPECT_LE(ns(ms(500)).count(), duration.count());
        EXPECT_LE(duration.count(), ns(ms(1000)).count());
        return make_ready_future<
            StatusOr<std::chrono::system_clock::time_point>>(
            Status(StatusCode::kCancelled, "cancelled"));
      });
  // Deregister the two timers we created.
  EXPECT_CALL(*mock_cq, RunAsync).Times(2);

  CompletionQueue cq(mock_cq);
  auto stub = CreateBigtableStub(
      cq, Options{}
              .set<UnifiedCredentialsOption>(MakeInsecureCredentials())
              .set<GrpcNumChannelsOption>(1)
              .set<bigtable::MinConnectionRefreshOption>(ms(500))
              .set<bigtable::MaxConnectionRefreshOption>(ms(1000)));

  // Simulate the timer firing, which should trigger another round of refreshes.
  p.set_value(make_status_or(std::chrono::system_clock::now()));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
