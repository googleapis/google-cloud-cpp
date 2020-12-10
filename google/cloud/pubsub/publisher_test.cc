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

#include "google/cloud/pubsub/publisher.h"
#include "google/cloud/pubsub/mocks/mock_publisher_connection.h"
#include "google/cloud/testing_util/assert_ok.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::testing::_;

// Tests {copy,move}x{constructor,assignment} + equality.
TEST(PublisherTest, ValueSemantics) {
  auto mock1 = std::make_shared<pubsub_mocks::MockPublisherConnection>();
  auto mock2 = std::make_shared<pubsub_mocks::MockPublisherConnection>();

  Publisher p1(mock1);
  Publisher p2(mock2);
  EXPECT_NE(p1, p2);

  p2 = p1;
  EXPECT_EQ(p1, p2);

  Publisher p3 = p1;
  EXPECT_EQ(p3, p1);

  Publisher p4 = std::move(p1);
  EXPECT_EQ(p4, p3);

  p1 = std::move(p3);
  EXPECT_EQ(p1, p2);
}

TEST(PublisherTest, PublishSimple) {
  auto mock = std::make_shared<pubsub_mocks::MockPublisherConnection>();
  EXPECT_CALL(*mock, Publish(_))
      .WillOnce([&](PublisherConnection::PublishParams const& p) {
        EXPECT_EQ("test-data-0", p.message.data());
        return make_ready_future(StatusOr<std::string>("test-id-0"));
      });
  EXPECT_CALL(*mock, Flush(_)).Times(1);

  Publisher publisher(mock);
  publisher.Flush();
  auto id =
      publisher.Publish(pubsub::MessageBuilder{}.SetData("test-data-0").Build())
          .get();
  ASSERT_STATUS_OK(id);
  EXPECT_EQ("test-id-0", *id);
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
