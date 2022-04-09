// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/pubsublite/publisher_connection_impl.h"
#include "google/cloud/pubsublite/testing/mock_publisher.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsublite {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::pubsub::Message;
using ::google::cloud::pubsublite::MessageMetadata;
using ::google::cloud::pubsublite_testing::MockPublisher;
using PublishParams =
    ::google::cloud::pubsub::PublisherConnection::PublishParams;
using ::google::cloud::pubsub_internal::FromProto;

using google::cloud::pubsublite::v1::PubSubMessage;
using ::google::cloud::pubsublite::v1::PubSubMessage;
using ::google::pubsub::v1::PubsubMessage;

using ::testing::StrictMock;

TEST(PublisherConnectionImplTest, BadMessage) {
  auto publisher =
      absl::make_unique<StrictMock<MockPublisher<MessageMetadata>>>();
  auto& publisher_ref = *publisher;
  auto opts = Options{}.set<PublishMessageTransformer>([](Message const&) {
    return StatusOr<PubSubMessage>{Status{StatusCode::kAborted, "uh ohhh"}};
  });
  PublisherConnectionImpl conn{std::move(publisher), opts};
  auto received = conn.Publish(PublishParams{FromProto(PubsubMessage{})});
  auto status = received.get().status();
  EXPECT_EQ(status, (Status{StatusCode::kAborted, "uh ohhh"}));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite
}  // namespace cloud
}  // namespace google
