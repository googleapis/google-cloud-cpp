// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/pubsub/internal/subscriber_stub.h"
#include "google/cloud/pubsub/publisher_connection.h"
#include "google/cloud/pubsub/subscription.h"
#include "google/cloud/pubsub/subscription_admin_client.h"
#include "google/cloud/pubsub/testing/random_names.h"
#include "google/cloud/pubsub/topic_admin_client.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/assert_ok.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

TEST(MessageIntegrationTest, PublishPullAck) {
  auto project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  ASSERT_FALSE(project_id.empty());

  auto generator = google::cloud::internal::MakeDefaultPRNG();
  Topic topic(project_id, pubsub_testing::RandomTopicId(generator));
  Subscription subscription(project_id,
                            pubsub_testing::RandomSubscriptionId(generator));

  auto topic_admin = TopicAdminClient(MakeTopicAdminConnection());
  auto subscription_admin =
      SubscriptionAdminClient(MakeSubscriptionAdminConnection());

  auto topic_metadata = topic_admin.CreateTopic(CreateTopicBuilder(topic));
  ASSERT_STATUS_OK(topic_metadata);

  struct Cleanup {
    std::function<void()> action;
    explicit Cleanup(std::function<void()> a) : action(std::move(a)) {}
    ~Cleanup() { action(); }
  };
  Cleanup cleanup_topic(
      [topic_admin, &topic]() mutable { topic_admin.DeleteTopic(topic); });

  auto subscription_metadata = subscription_admin.CreateSubscription(
      CreateSubscriptionBuilder(subscription, topic));
  ASSERT_STATUS_OK(subscription_metadata);

  auto publisher = MakePublisherConnection();
  auto subscriber =
      pubsub_internal::CreateDefaultSubscriberStub(ConnectionOptions{}, 0);

  auto pull =
      [&]() -> StatusOr<std::vector<google::pubsub::v1::ReceivedMessage>> {
    grpc::ClientContext context;
    google::pubsub::v1::PullRequest request;
    request.set_subscription(subscription.FullName());
    request.set_max_messages(5);
    auto response = subscriber->Pull(context, request);
    if (!response) return std::move(response).status();
    std::vector<google::pubsub::v1::ReceivedMessage> messages;
    messages.reserve(response->received_messages_size());
    for (auto& m : *response->mutable_received_messages()) {
      messages.push_back(std::move(m));
    }
    return messages;
  };

  auto ack = [&](std::string const& id) -> Status {
    grpc::ClientContext context;
    google::pubsub::v1::AcknowledgeRequest request;
    request.set_subscription(subscription.FullName());
    request.add_ack_ids(id);
    return subscriber->Acknowledge(context, request);
  };

  std::vector<std::string> ids;
  for (auto const* data : {"message-0", "message-1", "message-2"}) {
    auto response = publisher
                        ->Publish({topic.FullName(),
                                   MessageBuilder{}.SetData(data).Build()})
                        .get();
    EXPECT_STATUS_OK(response);
    if (response) {
      ids.push_back(*std::move(response));
    }
  }
  EXPECT_FALSE(ids.empty());

  while (!ids.empty()) {
    auto messages = pull();
    ASSERT_STATUS_OK(messages);

    for (auto const& m : *messages) {
      SCOPED_TRACE("Search for message " + m.DebugString());
      auto i = std::find(ids.begin(), ids.end(), m.message().message_id());
      EXPECT_STATUS_OK(ack(m.ack_id()));
      if (i != ids.end()) {
        ids.erase(i);
      } else {
        FAIL() << "Cannot find message id=" << m.message().message_id()
               << " to erase";
      }
    }
  }

  auto delete_response = subscription_admin.DeleteSubscription(subscription);
  ASSERT_STATUS_OK(delete_response);
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
