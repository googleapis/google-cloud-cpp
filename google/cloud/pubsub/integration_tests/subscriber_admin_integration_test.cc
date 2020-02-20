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

#include "google/cloud/pubsub/publisher_client.h"
#include "google/cloud/pubsub/subscriber_client.h"
#include "google/cloud/pubsub/subscription.h"
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

using ::testing::Contains;
using ::testing::Not;

std::string RandomTopicId(google::cloud::internal::DefaultPRNG& generator,
                          std::string const& prefix = "cloud-cpp-testing-") {
  constexpr int kMaxRandomTopicSuffixLength = 32;
  return prefix + google::cloud::internal::Sample(generator,
                                                  kMaxRandomTopicSuffixLength,
                                                  "abcdefghijklmnopqrstuvwxyz");
}

std::string RandomSubscriptionId(
    google::cloud::internal::DefaultPRNG& generator,
    std::string const& prefix = "cloud-cpp-testing-") {
  constexpr int kMaxRandomTopicSuffixLength = 32;
  return prefix + google::cloud::internal::Sample(generator,
                                                  kMaxRandomTopicSuffixLength,
                                                  "abcdefghijklmnopqrstuvwxyz");
}

TEST(SubscriberAdminIntegrationTest, SubscriberCRUD) {
  auto project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  ASSERT_FALSE(project_id.empty());

  auto subscription_names = [](SubscriberClient client,
                               std::string const& project_id) {
    std::vector<std::string> names;
    for (auto& subscription : client.ListSubscriptions(project_id)) {
      EXPECT_STATUS_OK(subscription);
      if (!subscription) break;
      names.push_back(subscription->name());
    }
    return names;
  };

  auto generator = google::cloud::internal::MakeDefaultPRNG();
  Topic topic(project_id, RandomTopicId(generator));
  Subscription subscription(project_id, RandomSubscriptionId(generator));

  auto publisher_client = PublisherClient(MakePublisherConnection());
  auto client = SubscriberClient(pubsub::MakeSubscriberConnection());

  EXPECT_THAT(subscription_names(client, project_id),
              Not(Contains(subscription.FullName())));

  auto topic_metadata = publisher_client.CreateTopic(CreateTopicBuilder(topic));
  ASSERT_STATUS_OK(topic_metadata);

  struct Cleanup {
    std::function<void()> action;
    explicit Cleanup(std::function<void()> a) : action(std::move(a)) {}
    ~Cleanup() { action(); }
  };
  Cleanup cleanup_topic{
      [&publisher_client, &topic] { publisher_client.DeleteTopic(topic); }};

  auto create_response =
      client.CreateSubscription(CreateSubscriptionBuilder(subscription, topic));
  ASSERT_STATUS_OK(create_response);

  EXPECT_THAT(subscription_names(client, project_id),
              Contains(subscription.FullName()));

  auto delete_response = client.DeleteSubscription(subscription);
  ASSERT_STATUS_OK(delete_response);

  EXPECT_THAT(subscription_names(client, project_id),
              Not(Contains(subscription.FullName())));
}

TEST(SubscriberAdminIntegrationTest, CreateSubscriptionFailure) {
  // Use an invalid endpoint to force a connection error.
  auto connection_options =
      ConnectionOptions(grpc::InsecureChannelCredentials())
          .set_endpoint("localhost:1");
  auto client = SubscriberClient(pubsub::MakeSubscriberConnection());
  auto create_response = client.CreateSubscription(CreateSubscriptionBuilder(
      Subscription("--invalid-project--", "--invalid-subscription--"),
      Topic("--invalid-project--", "--invalid-topic--")));
  ASSERT_FALSE(create_response.ok());
}

TEST(SubscriberAdminIntegrationTest, ListSubscriptionsFailure) {
  // Use an invalid endpoint to force a connection error.
  auto connection_options =
      ConnectionOptions(grpc::InsecureChannelCredentials())
          .set_endpoint("localhost:1");
  auto client = SubscriberClient(pubsub::MakeSubscriberConnection());
  auto list = client.ListSubscriptions("--invalid-project--");
  auto i = list.begin();
  EXPECT_FALSE(i == list.end());
  EXPECT_FALSE(*i);
}

TEST(SubscriberAdminIntegrationTest, DeleteSubscriptionFailure) {
  // Use an invalid endpoint to force a connection error.
  auto connection_options =
      ConnectionOptions(grpc::InsecureChannelCredentials())
          .set_endpoint("localhost:1");
  auto client = SubscriberClient(pubsub::MakeSubscriberConnection());
  auto delete_response = client.DeleteSubscription(
      Subscription("--invalid-project--", "--invalid-subscription--"));
  ASSERT_FALSE(delete_response.ok());
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
