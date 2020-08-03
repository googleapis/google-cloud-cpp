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

#include "google/cloud/pubsub/subscription.h"
#include "google/cloud/pubsub/subscription_admin_client.h"
#include "google/cloud/pubsub/testing/random_names.h"
#include "google/cloud/pubsub/topic_admin_client.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::testing::Contains;
using ::testing::Not;

TEST(SubscriptionAdminIntegrationTest, SubscriptionCRUD) {
  auto project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  ASSERT_FALSE(project_id.empty());

  auto subscription_names = [](SubscriptionAdminClient client,
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
  Topic topic(project_id, pubsub_testing::RandomTopicId(generator));
  Subscription subscription(project_id,
                            pubsub_testing::RandomSubscriptionId(generator));

  auto publisher_client = TopicAdminClient(MakeTopicAdminConnection());
  auto client =
      SubscriptionAdminClient(pubsub::MakeSubscriptionAdminConnection());

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

  auto get_response = client.GetSubscription(subscription);
  ASSERT_STATUS_OK(get_response);
  EXPECT_THAT(*create_response, IsProtoEqual(*get_response));

  EXPECT_THAT(subscription_names(client, project_id),
              Contains(subscription.FullName()));

  auto delete_response = client.DeleteSubscription(subscription);
  ASSERT_STATUS_OK(delete_response);

  EXPECT_THAT(subscription_names(client, project_id),
              Not(Contains(subscription.FullName())));
}

TEST(SubscriptionAdminIntegrationTest, CreateSubscriptionFailure) {
  // Use an invalid endpoint to force a connection error.
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto client = SubscriptionAdminClient(MakeSubscriptionAdminConnection());
  auto create_response = client.CreateSubscription(CreateSubscriptionBuilder(
      Subscription("--invalid-project--", "--invalid-subscription--"),
      Topic("--invalid-project--", "--invalid-topic--")));
  ASSERT_FALSE(create_response.ok());
}

TEST(SubscriptionAdminIntegrationTest, ListSubscriptionsFailure) {
  // Use an invalid endpoint to force a connection error.
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto client = SubscriptionAdminClient(MakeSubscriptionAdminConnection());
  auto list = client.ListSubscriptions("--invalid-project--");
  auto i = list.begin();
  EXPECT_FALSE(i == list.end());
  EXPECT_FALSE(*i);
}

TEST(SubscriptionAdminIntegrationTest, DeleteSubscriptionFailure) {
  // Use an invalid endpoint to force a connection error.
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto client = SubscriptionAdminClient(MakeSubscriptionAdminConnection());
  auto delete_response = client.DeleteSubscription(
      Subscription("--invalid-project--", "--invalid-subscription--"));
  ASSERT_FALSE(delete_response.ok());
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
