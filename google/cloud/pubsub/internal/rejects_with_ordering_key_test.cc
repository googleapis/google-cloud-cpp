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

#include "google/cloud/pubsub/internal/rejects_with_ordering_key.h"
#include "google/cloud/pubsub/mocks/mock_publisher_connection.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::google::cloud::testing_util::StatusIs;

TEST(RejectsWithOrderingKeyTest, MessageRejected) {
  auto mock = std::make_shared<pubsub_mocks::MockPublisherConnection>();
  pubsub::Topic const topic("test-project", "test-topic");
  EXPECT_CALL(*mock, Publish).Times(0);
  EXPECT_CALL(*mock, ResumePublish).Times(1);

  auto publisher = RejectsWithOrderingKey::Create(mock);
  auto response = publisher
                      ->Publish({pubsub::MessageBuilder{}
                                     .SetData("test-data-0")
                                     .SetOrderingKey("test-ordering-key-0")
                                     .Build()})
                      .get();
  EXPECT_THAT(response, StatusIs(StatusCode::kInvalidArgument));
  publisher->ResumePublish({"test-ordering-key-0"});
}

TEST(RejectsWithOrderingKeyTest, MessageAccepted) {
  auto mock = std::make_shared<pubsub_mocks::MockPublisherConnection>();
  EXPECT_CALL(*mock, Publish)
      .WillOnce([](pubsub::PublisherConnection::PublishParams const&) {
        return make_ready_future(StatusOr<std::string>("test-id"));
      });

  auto publisher = RejectsWithOrderingKey::Create(mock);
  auto response =
      publisher
          ->Publish({pubsub::MessageBuilder{}.SetData("test-data-0").Build()})
          .get();
  ASSERT_STATUS_OK(response);
  EXPECT_EQ("test-id", *response);
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
