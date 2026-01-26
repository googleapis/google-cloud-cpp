// Copyright 2025 Google LLC
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

#include "google/cloud/bigtable/internal/dynamic_channel_pool.h"
#include "google/cloud/bigtable/testing/mock_bigtable_stub.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/testing_util/fake_clock.h"
#include "google/cloud/testing_util/fake_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::bigtable::testing::MockBigtableStub;
using ::google::cloud::testing_util::FakeCompletionQueueImpl;
using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Eq;

TEST(ChannelUsageTest, SetChannel) {
  auto mock = std::make_shared<MockBigtableStub>();
  auto channel = std::make_shared<ChannelUsage<BigtableStub>>();
  EXPECT_THAT(channel->AcquireStub(), Eq(nullptr));
  channel->set_channel(mock);
  EXPECT_THAT(channel->AcquireStub(), Eq(mock));
  auto mock2 = std::make_shared<MockBigtableStub>();
  channel->set_channel(mock2);
  EXPECT_THAT(channel->AcquireStub(), Eq(mock));
}

TEST(ChannelUsageTest, InstantOutstandingRpcs) {
  //  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  auto mock = std::make_shared<MockBigtableStub>();
  auto channel = std::make_shared<ChannelUsage<BigtableStub>>(mock);

  auto stub = channel->AcquireStub();
  EXPECT_THAT(stub, Eq(mock));
  EXPECT_THAT(channel->instant_outstanding_rpcs(), IsOkAndHolds(1));
  stub = channel->AcquireStub();
  EXPECT_THAT(stub, Eq(mock));
  stub = channel->AcquireStub();
  EXPECT_THAT(stub, Eq(mock));
  EXPECT_THAT(channel->instant_outstanding_rpcs(), IsOkAndHolds(3));
  channel->ReleaseStub();
  EXPECT_THAT(channel->instant_outstanding_rpcs(), IsOkAndHolds(2));
  channel->ReleaseStub();
  channel->ReleaseStub();
  EXPECT_THAT(channel->instant_outstanding_rpcs(), IsOkAndHolds(0));
}

TEST(ChannelUsageTest, SetLastRefreshStatus) {
  auto mock = std::make_shared<MockBigtableStub>();
  auto channel = std::make_shared<ChannelUsage<BigtableStub>>(mock);
  Status expected_status = internal::InternalError("uh oh");
  (void)channel->AcquireStub();
  EXPECT_THAT(channel->instant_outstanding_rpcs(), IsOkAndHolds(1));
  channel->set_last_refresh_status(expected_status);
  EXPECT_THAT(channel->instant_outstanding_rpcs(),
              StatusIs(expected_status.code()));
  EXPECT_THAT(channel->average_outstanding_rpcs(),
              StatusIs(expected_status.code()));
}

TEST(ChannelUsageTest, AverageOutstandingRpcs) {
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  auto mock = std::make_shared<MockBigtableStub>();
  auto channel = std::make_shared<ChannelUsage<BigtableStub>>(mock, clock);
  EXPECT_THAT(channel->instant_outstanding_rpcs(), IsOkAndHolds(0));

  auto start = std::chrono::steady_clock::now();
  clock->SetTime(start);
  // measurements: 1, 2, 3, 4, 5, 6, 7, 8, 9, 10
  for (int i = 0; i < 10; ++i) (void)channel->AcquireStub();

  clock->AdvanceTime(std::chrono::seconds(20));
  // measurements: 11, 12, 13, 14, 15, 16, 17, 18, 19, 20
  for (int i = 0; i < 10; ++i) (void)channel->AcquireStub();

  clock->AdvanceTime(std::chrono::seconds(30));
  // measurements: 21, 22, 23, 24, 25, 26, 27, 28, 29, 30
  for (int i = 0; i < 10; ++i) (void)channel->AcquireStub();

  clock->AdvanceTime(std::chrono::seconds(20));
  // FLOOR(SUM(11...30) / 20) = 20
  EXPECT_THAT(channel->average_outstanding_rpcs(), IsOkAndHolds(20));

  clock->AdvanceTime(std::chrono::seconds(20));
  // FLOOR(SUM(21...30) / 10) = 25
  EXPECT_THAT(channel->average_outstanding_rpcs(), IsOkAndHolds(25));
  // measurements: 29, 28, 27, 26, 25, 24, 23, 22, 21, 20
  for (int i = 0; i < 10; ++i) channel->ReleaseStub();
  // FLOOR((SUM(21...30) + SUM(20...29)) / 20) = 25
  EXPECT_THAT(channel->average_outstanding_rpcs(), IsOkAndHolds(25));

  clock->AdvanceTime(std::chrono::seconds(61));
  // All measurements have aged out.
  EXPECT_THAT(channel->average_outstanding_rpcs(), IsOkAndHolds(0));
}

TEST(ChannelUsageTest, MakeWeak) {
  auto channel = std::make_shared<ChannelUsage<BigtableStub>>();
  auto weak = channel->MakeWeak();
  EXPECT_THAT(weak.lock(), Eq(channel));
}

TEST(DynamicChannelPoolTest, GetChannelRandomTwoLeastUsed) {
  auto fake_cq_impl = std::make_shared<FakeCompletionQueueImpl>();

  auto refresh_state = std::make_shared<ConnectionRefreshState>(
      fake_cq_impl, std::chrono::milliseconds(1),
      std::chrono::milliseconds(10));

  auto stub_factory_fn =
      [](int, bool) -> std::shared_ptr<ChannelUsage<BigtableStub>> {
    auto mock = std::make_shared<MockBigtableStub>();
    return std::make_shared<ChannelUsage<BigtableStub>>(mock);
  };

  bigtable::experimental::DynamicChannelPoolSizingPolicy sizing_policy;

  std::vector<std::shared_ptr<ChannelUsage<BigtableStub>>> channels(10);
  int id = 0;
  std::generate(channels.begin(), channels.end(),
                [&]() { return stub_factory_fn(id++, false); });

  auto pool = DynamicChannelPool<BigtableStub>::Create(
      CompletionQueue(fake_cq_impl), channels, refresh_state, stub_factory_fn,
      sizing_policy);

  auto selected_stub = pool->GetChannelRandomTwoLeastUsed();
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
