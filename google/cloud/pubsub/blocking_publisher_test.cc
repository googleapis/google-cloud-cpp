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

#include "google/cloud/pubsub/blocking_publisher.h"
#include "google/cloud/pubsub/mocks/mock_blocking_publisher_connection.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::Return;

struct TestOptionA {
  using Type = std::string;
};

struct TestOptionB {
  using Type = std::string;
};

struct TestOptionC {
  using Type = std::string;
};

TEST(BlockingPublisherTest, OptionsNoOverrides) {
  Topic const topic("test-project", "test-topic");
  auto mock = std::make_shared<pubsub_mocks::MockBlockingPublisherConnection>();
  EXPECT_CALL(*mock, options)
      .WillRepeatedly(Return(Options{}
                                 .set<TestOptionA>("test-a")
                                 .set<TestOptionB>("test-b")
                                 .set<TestOptionC>("test-c")));
  EXPECT_CALL(*mock, Publish).WillOnce([](auto const&) {
    auto const& current = google::cloud::internal::CurrentOptions();
    EXPECT_EQ(current.get<TestOptionA>(), "test-a");
    EXPECT_EQ(current.get<TestOptionB>(), "test-b");
    EXPECT_EQ(current.get<TestOptionC>(), "test-c");
    return std::string{"test-ack-id"};
  });

  BlockingPublisher publisher(mock);
  ASSERT_STATUS_OK(
      publisher.Publish(topic, MessageBuilder().SetData("test-only").Build()));
}

TEST(BlockingPublisherTest, OptionsClientOverrides) {
  Topic const topic("test-project", "test-topic");
  auto mock = std::make_shared<pubsub_mocks::MockBlockingPublisherConnection>();
  EXPECT_CALL(*mock, options)
      .WillRepeatedly(Return(Options{}
                                 .set<TestOptionA>("test-a")
                                 .set<TestOptionB>("test-b")
                                 .set<TestOptionC>("test-c")));
  EXPECT_CALL(*mock, Publish).WillOnce([](auto const&) {
    auto const& current = google::cloud::internal::CurrentOptions();
    EXPECT_EQ(current.get<TestOptionA>(), "override-a");
    EXPECT_EQ(current.get<TestOptionB>(), "test-b");
    EXPECT_EQ(current.get<TestOptionC>(), "test-c");
    return std::string{"test-ack-id"};
  });

  BlockingPublisher publisher(mock, Options{}.set<TestOptionA>("override-a"));
  ASSERT_STATUS_OK(
      publisher.Publish(topic, MessageBuilder().SetData("test-only").Build()));
}

TEST(BlockingPublisherTest, OptionsFunctionOverrides) {
  Topic const topic("test-project", "test-topic");
  auto mock = std::make_shared<pubsub_mocks::MockBlockingPublisherConnection>();
  EXPECT_CALL(*mock, options)
      .WillRepeatedly(Return(Options{}
                                 .set<TestOptionA>("test-a")
                                 .set<TestOptionB>("test-b")
                                 .set<TestOptionC>("test-c")));
  EXPECT_CALL(*mock, Publish).WillOnce([](auto const&) {
    auto const& current = google::cloud::internal::CurrentOptions();
    EXPECT_EQ(current.get<TestOptionA>(), "override-a1");
    EXPECT_EQ(current.get<TestOptionB>(), "override-b1");
    EXPECT_EQ(current.get<TestOptionC>(), "test-c");
    return std::string{"test-ack-id"};
  });

  BlockingPublisher publisher(mock, Options{}.set<TestOptionA>("override-a"));
  ASSERT_STATUS_OK(
      publisher.Publish(topic, MessageBuilder().SetData("test-only").Build(),
                        Options{}
                            .set<TestOptionA>("override-a1")
                            .set<TestOptionB>("override-b1")));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
