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

#include "google/cloud/pubsub/snapshot_builder.h"
#include "google/cloud/pubsub/subscription.h"
#include "google/cloud/pubsub/subscription_admin_client.h"
#include "google/cloud/pubsub/testing/random_names.h"
#include "google/cloud/pubsub/testing/test_retry_policies.h"
#include "google/cloud/pubsub/topic_admin_client.h"
#include "google/cloud/pubsub/version.h"
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
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::google::cloud::pubsub_testing::TestBackoffPolicy;
using ::google::cloud::pubsub_testing::TestRetryPolicy;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AnyOf;
using ::testing::Contains;
using ::testing::Not;

bool UsingEmulator() {
  return google::cloud::internal::GetEnv("PUBSUB_EMULATOR_HOST").has_value();
}

using SubscriptionAdminIntegrationTest =
    ::google::cloud::testing_util::IntegrationTest;

TEST_F(SubscriptionAdminIntegrationTest, SubscriptionCRUD) {
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

  auto snapshot_names = [](SubscriptionAdminClient client,
                           std::string const& project_id) {
    std::vector<std::string> names;
    for (auto& snapshot : client.ListSnapshots(project_id)) {
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

  auto topic_admin = TopicAdminClient(MakeTopicAdminConnection());
  auto subscription_admin =
      SubscriptionAdminClient(MakeSubscriptionAdminConnection());

  EXPECT_THAT(subscription_names(subscription_admin, project_id),
              Not(Contains(subscription.FullName())));

  auto topic_metadata = topic_admin.CreateTopic(TopicBuilder(topic));
  ASSERT_THAT(topic_metadata,
              AnyOf(IsOk(), StatusIs(StatusCode::kAlreadyExists)));

  struct Cleanup {
    std::function<void()> action;
    explicit Cleanup(std::function<void()> a) : action(std::move(a)) {}
    ~Cleanup() { action(); }
  };
  Cleanup cleanup_topic{
      [&topic_admin, &topic] { topic_admin.DeleteTopic(topic); }};

  auto endpoint = "https://" + project_id + ".appspot.com/push";
  auto create_response = subscription_admin.CreateSubscription(
      topic, subscription,
      SubscriptionBuilder{}.set_push_config(
          PushConfigBuilder{}.set_push_endpoint(endpoint)));
  ASSERT_THAT(create_response,
              AnyOf(IsOk(), StatusIs(StatusCode::kAlreadyExists)));

  auto get_response = subscription_admin.GetSubscription(subscription);
  ASSERT_STATUS_OK(get_response);
  // We cannot compare the full protos because for push configs `Create...()`
  // returns less information than `Get` :shrug:
  EXPECT_EQ(create_response->name(), get_response->name());

  auto constexpr kTestDeadlineSeconds = 20;
  auto update_response = subscription_admin.UpdateSubscription(
      subscription, SubscriptionBuilder{}.set_ack_deadline(
                        std::chrono::seconds(kTestDeadlineSeconds)));
  ASSERT_STATUS_OK(update_response);
  EXPECT_EQ(kTestDeadlineSeconds, update_response->ack_deadline_seconds());

  EXPECT_THAT(subscription_names(subscription_admin, project_id),
              Contains(subscription.FullName()));

  auto modify_push_config_response = subscription_admin.ModifyPushSubscription(
      subscription, PushConfigBuilder{});
  EXPECT_STATUS_OK(modify_push_config_response);

  auto const topic_subscriptions = [&] {
    std::vector<std::string> names;
    for (auto& name : topic_admin.ListTopicSubscriptions(topic)) {
      EXPECT_STATUS_OK(name);
      names.push_back(std::move(*name));
    }
    return names;
  }();
  EXPECT_THAT(topic_subscriptions, Contains(subscription.FullName()));

  // To create snapshots we need at least one subscription, so we test those
  // here too.
  // TODO(#4792) - cannot test server-side assigned names, the emulator lacks
  //    support for them.
  Snapshot snapshot(project_id, pubsub_testing::RandomSnapshotId(generator));
  auto create_snapshot_response =
      subscription_admin.CreateSnapshot(subscription, snapshot);
  ASSERT_STATUS_OK(create_snapshot_response);
  EXPECT_EQ(snapshot.FullName(), create_snapshot_response->name());

  auto const topic_snapshots = [&] {
    std::vector<std::string> names;
    for (auto& name : topic_admin.ListTopicSnapshots(topic)) {
      EXPECT_STATUS_OK(name);
      names.push_back(std::move(*name));
    }
    return names;
  }();
  EXPECT_THAT(topic_snapshots, Contains(snapshot.FullName()));

  auto get_snapshot_response = subscription_admin.GetSnapshot(snapshot);
  ASSERT_STATUS_OK(get_snapshot_response);
  EXPECT_THAT(*get_snapshot_response, IsProtoEqual(*create_snapshot_response));

  // TODO(#4792) - the emulator does not support UpdateSnapshot()
  if (!UsingEmulator()) {
    auto update_snapshot_response = subscription_admin.UpdateSnapshot(
        snapshot, SnapshotBuilder{}.add_label("test-label", "test-value"));
    ASSERT_STATUS_OK(update_snapshot_response);
    EXPECT_FALSE(update_snapshot_response->labels().empty());
  }

  auto seek_response = subscription_admin.Seek(subscription, snapshot);
  EXPECT_STATUS_OK(seek_response);

  EXPECT_THAT(snapshot_names(subscription_admin, project_id),
              Contains(snapshot.FullName()));
  auto delete_snapshot = subscription_admin.DeleteSnapshot(snapshot);
  EXPECT_STATUS_OK(delete_snapshot);
  EXPECT_THAT(snapshot_names(subscription_admin, project_id),
              Not(Contains(snapshot.FullName())));

  // TODO(#4792) - the emulator does not support DetachSubscription()
  if (!UsingEmulator()) {
    auto detach_response = topic_admin.DetachSubscription(subscription);
    ASSERT_STATUS_OK(detach_response);
  }

  auto delete_response = subscription_admin.DeleteSubscription(subscription);
  EXPECT_THAT(delete_response, AnyOf(IsOk(), StatusIs(StatusCode::kNotFound)));

  EXPECT_THAT(subscription_names(subscription_admin, project_id),
              Not(Contains(subscription.FullName())));
}

TEST_F(SubscriptionAdminIntegrationTest, CreateSubscriptionFailure) {
  // Use an invalid endpoint to force a connection error.
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto client = SubscriptionAdminClient(MakeSubscriptionAdminConnection(
      {}, TestRetryPolicy(), TestBackoffPolicy()));
  auto create_response = client.CreateSubscription(
      Topic("--invalid-project--", "--invalid-topic--"),
      Subscription("--invalid-project--", "--invalid-subscription--"));
  ASSERT_FALSE(create_response.ok());
}

TEST_F(SubscriptionAdminIntegrationTest, GetSubscriptionFailure) {
  // Use an invalid endpoint to force a connection error.
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto client = SubscriptionAdminClient(MakeSubscriptionAdminConnection(
      {}, TestRetryPolicy(), TestBackoffPolicy()));
  auto create_response = client.GetSubscription(
      Subscription("--invalid-project--", "--invalid-subscription--"));
  ASSERT_FALSE(create_response.ok());
}

TEST_F(SubscriptionAdminIntegrationTest, UpdateSubscriptionFailure) {
  // Use an invalid endpoint to force a connection error.
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto client = SubscriptionAdminClient(MakeSubscriptionAdminConnection(
      {}, TestRetryPolicy(), TestBackoffPolicy()));
  auto create_response = client.UpdateSubscription(
      Subscription("--invalid-project--", "--invalid-subscription--"),
      SubscriptionBuilder{}.set_ack_deadline(std::chrono::seconds(20)));
  ASSERT_FALSE(create_response.ok());
}

TEST_F(SubscriptionAdminIntegrationTest, ListSubscriptionsFailure) {
  // Use an invalid endpoint to force a connection error.
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto client = SubscriptionAdminClient(MakeSubscriptionAdminConnection(
      {}, TestRetryPolicy(), TestBackoffPolicy()));
  auto list = client.ListSubscriptions("--invalid-project--");
  auto i = list.begin();
  EXPECT_FALSE(i == list.end());
  EXPECT_FALSE(*i);
}

TEST_F(SubscriptionAdminIntegrationTest, DeleteSubscriptionFailure) {
  // Use an invalid endpoint to force a connection error.
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto client = SubscriptionAdminClient(MakeSubscriptionAdminConnection(
      {}, TestRetryPolicy(), TestBackoffPolicy()));
  auto delete_response = client.DeleteSubscription(
      Subscription("--invalid-project--", "--invalid-subscription--"));
  ASSERT_FALSE(delete_response.ok());
}

TEST_F(SubscriptionAdminIntegrationTest, ModifyPushConfigFailure) {
  // Use an invalid endpoint to force a connection error.
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto client = SubscriptionAdminClient(MakeSubscriptionAdminConnection(
      {}, TestRetryPolicy(), TestBackoffPolicy()));
  auto delete_response = client.ModifyPushSubscription(
      Subscription("--invalid-project--", "--invalid-subscription--"),
      PushConfigBuilder{});
  ASSERT_FALSE(delete_response.ok());
}

TEST_F(SubscriptionAdminIntegrationTest, CreateSnapshotFailure) {
  // Use an invalid endpoint to force a connection error.
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto client = SubscriptionAdminClient(MakeSubscriptionAdminConnection(
      {}, TestRetryPolicy(), TestBackoffPolicy()));
  auto response = client.CreateSnapshot(
      Subscription("--invalid-project--", "--invalid-subscription--"));
  ASSERT_FALSE(response.ok());
}

TEST_F(SubscriptionAdminIntegrationTest, GetSnapshotFailure) {
  // Use an invalid endpoint to force a connection error.
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto client = SubscriptionAdminClient(MakeSubscriptionAdminConnection(
      {}, TestRetryPolicy(), TestBackoffPolicy()));
  auto response = client.GetSnapshot(
      Snapshot("--invalid-project--", "--invalid-snapshot--"));
  ASSERT_FALSE(response.ok());
}

TEST_F(SubscriptionAdminIntegrationTest, ListSnapshotsFailure) {
  // Use an invalid endpoint to force a connection error.
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto client = SubscriptionAdminClient(MakeSubscriptionAdminConnection(
      {}, TestRetryPolicy(), TestBackoffPolicy()));
  auto list = client.ListSnapshots("--invalid-project--");
  auto i = list.begin();
  EXPECT_FALSE(i == list.end());
  EXPECT_FALSE(*i);
}

TEST_F(SubscriptionAdminIntegrationTest, UpdateSnapshotFailure) {
  // Use an invalid endpoint to force a connection error.
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto client = SubscriptionAdminClient(MakeSubscriptionAdminConnection(
      {}, TestRetryPolicy(), TestBackoffPolicy()));
  auto response = client.UpdateSnapshot(
      Snapshot("--invalid-project--", "--invalid-snapshot--"),
      SnapshotBuilder{}.clear_labels());
  ASSERT_FALSE(response.ok());
}

TEST_F(SubscriptionAdminIntegrationTest, DeleteSnapshotFailure) {
  // Use an invalid endpoint to force a connection error.
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto client = SubscriptionAdminClient(MakeSubscriptionAdminConnection(
      {}, TestRetryPolicy(), TestBackoffPolicy()));
  auto response = client.DeleteSnapshot(
      Snapshot("--invalid-project--", "--invalid-snapshot--"));
  ASSERT_FALSE(response.ok());
}

TEST_F(SubscriptionAdminIntegrationTest, SeekFailureTimestamp) {
  // Use an invalid endpoint to force a connection error.
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto client = SubscriptionAdminClient(MakeSubscriptionAdminConnection(
      {}, TestRetryPolicy(), TestBackoffPolicy()));
  auto response = client.Seek(
      Subscription("--invalid-project--", "--invalid-subscription--"),
      std::chrono::system_clock::now());
  ASSERT_FALSE(response.ok());
}

TEST_F(SubscriptionAdminIntegrationTest, SeekFailureSnapshot) {
  // Use an invalid endpoint to force a connection error.
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto client = SubscriptionAdminClient(MakeSubscriptionAdminConnection(
      {}, TestRetryPolicy(), TestBackoffPolicy()));
  auto response = client.Seek(
      Subscription("--invalid-project--", "--invalid-subscription--"),
      Snapshot("--invalid-project--", "--invalid-snapshot--"));
  ASSERT_FALSE(response.ok());
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
