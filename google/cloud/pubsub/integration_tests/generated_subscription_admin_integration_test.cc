// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/internal/disable_deprecation_warnings.inc"
#include "google/cloud/pubsub/admin/subscription_admin_client.h"
#include "google/cloud/pubsub/admin/subscription_admin_connection.h"
#include "google/cloud/pubsub/admin/topic_admin_client.h"
#include "google/cloud/pubsub/admin/topic_admin_connection.h"
#include "google/cloud/pubsub/snapshot_builder.h"
#include "google/cloud/pubsub/subscription.h"
#include "google/cloud/pubsub/subscription_builder.h"
#include "google/cloud/pubsub/testing/random_names.h"
#include "google/cloud/pubsub/testing/test_retry_policies.h"
#include "google/cloud/pubsub/topic_builder.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/credentials.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/integration_test.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AnyOf;
using ::testing::Contains;
using ::testing::Not;

bool UsingEmulator() {
  return google::cloud::internal::GetEnv("PUBSUB_EMULATOR_HOST").has_value();
}

using GeneratedSubscriptionAdminIntegrationTest =
    ::google::cloud::testing_util::IntegrationTest;

StatusOr<std::vector<std::string>> SubscriptionNames(
    pubsub_admin::SubscriptionAdminClient client,
    std::string const& project_id) {
  std::vector<std::string> names;
  for (auto& subscription :
       client.ListSubscriptions("projects/" + project_id)) {
    if (!subscription) return std::move(subscription).status();
    names.push_back(subscription->name());
  }
  return names;
}

TEST_F(GeneratedSubscriptionAdminIntegrationTest, SubscriptionCRUD) {
  auto project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  ASSERT_FALSE(project_id.empty());

  auto snapshot_names = [](pubsub_admin::SubscriptionAdminClient client,
                           std::string const& project_id) {
    std::vector<std::string> names;
    for (auto& snapshot : client.ListSnapshots("projects/" + project_id)) {
      EXPECT_STATUS_OK(snapshot);
      if (!snapshot) break;
      names.push_back(snapshot->name());
    }
    return names;
  };

  auto generator = google::cloud::internal::MakeDefaultPRNG();
  Topic topic(project_id, pubsub_testing::RandomTopicId(generator));
  Subscription subscription(project_id,
                            pubsub_testing::RandomSubscriptionId(generator));

  auto topic_admin = pubsub_admin::TopicAdminClient(
      pubsub_admin::MakeTopicAdminConnection("australia-southeast1"));
  auto subscription_admin = pubsub_admin::SubscriptionAdminClient(
      pubsub_admin::MakeSubscriptionAdminConnection("australia-southeast1"));

  auto names = SubscriptionNames(subscription_admin, project_id);
  ASSERT_STATUS_OK(names);
  EXPECT_THAT(*names, Not(Contains(subscription.FullName())));

  auto topic_metadata =
      topic_admin.CreateTopic(TopicBuilder(topic).BuildCreateRequest());
  ASSERT_THAT(topic_metadata,
              AnyOf(IsOk(), StatusIs(StatusCode::kAlreadyExists)));

  struct Cleanup {
    std::function<void()> action;
    explicit Cleanup(std::function<void()> a) : action(std::move(a)) {}
    ~Cleanup() { action(); }
  };
  Cleanup cleanup_topic{
      [&topic_admin, &topic] { topic_admin.DeleteTopic(topic.FullName()); }};

  auto endpoint = "https://" + project_id + ".appspot.com/push";
  auto create_response = subscription_admin.CreateSubscription(
      SubscriptionBuilder{}
          .set_push_config(PushConfigBuilder{}.set_push_endpoint(endpoint))
          .BuildCreateRequest(topic, subscription));
  ASSERT_THAT(create_response,
              AnyOf(IsOk(), StatusIs(StatusCode::kAlreadyExists)));

  auto get_response =
      subscription_admin.GetSubscription(subscription.FullName());
  ASSERT_STATUS_OK(get_response);
  // We cannot compare the full protos because for push configs `Create...()`
  // returns less information than `Get` :shrug:
  EXPECT_EQ(create_response->name(), get_response->name());

  auto constexpr kTestDeadlineSeconds = 20;
  auto update_response = subscription_admin.UpdateSubscription(
      SubscriptionBuilder{}
          .set_ack_deadline(std::chrono::seconds(kTestDeadlineSeconds))
          .BuildUpdateRequest(subscription));
  ASSERT_STATUS_OK(update_response);
  EXPECT_EQ(kTestDeadlineSeconds, update_response->ack_deadline_seconds());
  names = SubscriptionNames(subscription_admin, project_id);
  ASSERT_STATUS_OK(names);
  EXPECT_THAT(*names, Contains(subscription.FullName()));

  auto modify_push_config_response = subscription_admin.ModifyPushConfig(
      PushConfigBuilder{}.BuildModifyPushConfig(subscription));
  EXPECT_STATUS_OK(modify_push_config_response);

  auto const topic_subscriptions = [&] {
    std::vector<std::string> names;
    for (auto& name : topic_admin.ListTopicSubscriptions(topic.FullName())) {
      EXPECT_STATUS_OK(name);
      names.push_back(std::move(*name));
    }
    return names;
  }();
  EXPECT_THAT(topic_subscriptions, Contains(subscription.FullName()));

  // To create snapshots we need at least one subscription, so we test those
  // here too.
  Snapshot snapshot(project_id, pubsub_testing::RandomSnapshotId(generator));
  auto create_snapshot_response = subscription_admin.CreateSnapshot(
      snapshot.FullName(), subscription.FullName());
  ASSERT_STATUS_OK(create_snapshot_response);
  EXPECT_EQ(snapshot.FullName(), create_snapshot_response->name());

  auto const topic_snapshots = [&] {
    std::vector<std::string> names;
    for (auto& name : topic_admin.ListTopicSnapshots(topic.FullName())) {
      EXPECT_STATUS_OK(name);
      names.push_back(std::move(*name));
    }
    return names;
  }();
  EXPECT_THAT(topic_snapshots, Contains(snapshot.FullName()));

  auto get_snapshot_response =
      subscription_admin.GetSnapshot(snapshot.FullName());
  ASSERT_STATUS_OK(get_snapshot_response);
  EXPECT_THAT(*get_snapshot_response, IsProtoEqual(*create_snapshot_response));

  // Skip, as this is not supported by the emulator.
  if (!UsingEmulator()) {
    auto update_snapshot_response = subscription_admin.UpdateSnapshot(
        SnapshotBuilder{}
            .add_label("test-label", "test-value")
            .BuildUpdateRequest(snapshot));
    ASSERT_STATUS_OK(update_snapshot_response);
    EXPECT_FALSE(update_snapshot_response->labels().empty());
  }

  google::pubsub::v1::SeekRequest seek_request;
  seek_request.set_subscription(subscription.FullName());
  seek_request.set_snapshot(snapshot.FullName());
  auto seek_response = subscription_admin.Seek(seek_request);
  EXPECT_STATUS_OK(seek_response);

  EXPECT_THAT(snapshot_names(subscription_admin, project_id),
              Contains(snapshot.FullName()));
  auto delete_snapshot = subscription_admin.DeleteSnapshot(snapshot.FullName());
  EXPECT_STATUS_OK(delete_snapshot);
  EXPECT_THAT(snapshot_names(subscription_admin, project_id),
              Not(Contains(snapshot.FullName())));

  // Skip, as this is not supported by the emulator.
  if (!UsingEmulator()) {
    google::pubsub::v1::DetachSubscriptionRequest detach_request;
    detach_request.set_subscription(subscription.FullName());
    auto detach_response = topic_admin.DetachSubscription(detach_request);
    ASSERT_STATUS_OK(detach_response);
  }

  auto delete_response =
      subscription_admin.DeleteSubscription(subscription.FullName());
  EXPECT_THAT(delete_response, AnyOf(IsOk(), StatusIs(StatusCode::kNotFound)));

  names = SubscriptionNames(subscription_admin, project_id);
  ASSERT_STATUS_OK(names);
  EXPECT_THAT(*names, Not(Contains(subscription.FullName())));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
