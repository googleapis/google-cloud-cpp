// Copyright 2026 Google LLC
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

#include "google/cloud/bigtable/internal/channel_usage.h"
#include "google/cloud/bigtable/testing/mock_bigtable_stub.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/testing_util/fake_clock.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::bigtable::testing::MockBigtableStub;
using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Eq;

TEST(ChannelUsageTest, SetChannel) {
  auto mock = std::make_shared<MockBigtableStub>();
  auto channel = std::make_shared<ChannelUsage<BigtableStub>>();
  EXPECT_THAT(channel->AcquireStub(), Eq(nullptr));
  channel->set_stub(mock);
  EXPECT_THAT(channel->AcquireStub(), Eq(mock));
  auto mock2 = std::make_shared<MockBigtableStub>();
  channel->set_stub(mock2);
  EXPECT_THAT(channel->AcquireStub(), Eq(mock));
}

TEST(ChannelUsageTest, InstantOutstandingRpcs) {
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
}

TEST(ChannelUsageTest, MakeWeak) {
  auto channel = std::make_shared<ChannelUsage<BigtableStub>>();
  auto weak = channel->MakeWeak();
  EXPECT_THAT(weak.lock(), Eq(channel));
  channel.reset();
  EXPECT_THAT(weak.lock(), Eq(nullptr));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
