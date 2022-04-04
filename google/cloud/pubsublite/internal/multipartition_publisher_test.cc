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

#include "google/cloud/pubsublite/internal/multipartition_publisher.h"
#include "google/cloud/pubsublite/testing/mock_alarm_registry.h"
#include "google/cloud/pubsublite/testing/mock_partition_publisher.h"
#include "google/cloud/pubsublite/testing/mock_routing_policy.h"
#include "google/cloud/pubsublite/mocks/mock_admin_connection.h"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {

using Partition = RoutingPolicy::Partition;
using google::cloud::pubsublite::MessageMetadata;
using google::cloud::pubsublite_mocks::MockAdminServiceConnection;

using ::testing::_;
using ::testing::ByMove;
using ::testing::InSequence;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::WithArg;

using google::cloud::pubsublite::Topic;

using ::google::cloud::pubsublite_testing::MockAlarmRegistry;
using ::google::cloud::pubsublite_testing::MockAlarmRegistryCancelToken;
using ::google::cloud::pubsublite_testing::MockPartitionPublisher;
using ::google::cloud::pubsublite_testing::MockRoutingPolicy;

auto const kAlarmDuration = std::chrono::milliseconds{std::chrono::seconds{60}};

class SinglePublisherTest : public ::testing::Test {
 protected:
  SinglePublisherTest()
      : partition_publisher_ref_{*(new StrictMock<MockPartitionPublisher>())},
        admin_connection_{
            std::make_shared<StrictMock<MockAdminServiceConnection>>()},
        alarm_token_ref_{*(new StrictMock<MockAlarmRegistryCancelToken>())},
        routing_policy_ref_{*(new StrictMock<MockRoutingPolicy>())},
        topic_{"project", "location", "name"} {
    EXPECT_CALL(alarm_registry_, RegisterAlarm(kAlarmDuration, _))
        .WillOnce(WithArg<1>([&](std::function<void()> on_alarm) {
          // as this is a unit test, we mock the AlarmRegistry behavior
          // this enables the test suite to control when the alarm is
          // rung/messages are flushed
          on_alarm_ = std::move(on_alarm);
          return absl::WrapUnique(&alarm_token_ref_);
        }));
  }

  StrictMock<MockFunction<std::unique_ptr<PartitionPublisher>(Partition)>>
      partition_publisher_factory_;
  StrictMock<MockPartitionPublisher>& partition_publisher_ref_;
  std::shared_ptr<StrictMock<MockAdminServiceConnection>> admin_connection_;
  StrictMock<MockAlarmRegistry> alarm_registry_;
  StrictMock<MockAlarmRegistryCancelToken>& alarm_token_ref_;
  StrictMock<MockRoutingPolicy>& routing_policy_ref_;
  Topic const topic_;
  std::function<void()> on_alarm_;
  std::unique_ptr<Publisher<MessageMetadata>> multipartition_publisher_;
};

}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
