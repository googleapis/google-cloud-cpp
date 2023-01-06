// Copyright 2020 Google LLC
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

#include "google/cloud/pubsub/testing/random_names.h"
#include "google/cloud/pubsub/testing/test_retry_policies.h"
#include "google/cloud/pubsub/topic_admin_client.h"
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

using ::google::cloud::pubsub_testing::MakeTestOptions;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AnyOf;
using ::testing::Contains;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::NotNull;

bool UsingEmulator() {
  return google::cloud::internal::GetEnv("PUBSUB_EMULATOR_HOST").has_value();
}

TopicAdminClient MakeTestTopicAdminClient() {
  return TopicAdminClient(MakeTopicAdminConnection(MakeTestOptions()));
}

using TopicAdminIntegrationTest =
    ::google::cloud::testing_util::IntegrationTest;

StatusOr<std::vector<std::string>> TopicNames(TopicAdminClient client,
                                              std::string const& project_id) {
  std::vector<std::string> names;
  for (auto& topic : client.ListTopics(project_id)) {
    if (!topic) return std::move(topic).status();
    names.push_back(std::move(*topic->mutable_name()));
  }
  return names;
}

TEST_F(TopicAdminIntegrationTest, TopicCRUD) {
  auto project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  ASSERT_THAT(project_id, Not(IsEmpty()));

  auto generator = google::cloud::internal::MakeDefaultPRNG();
  Topic topic(project_id, pubsub_testing::RandomTopicId(generator));

  auto publisher = TopicAdminClient(MakeTopicAdminConnection());
  auto names = TopicNames(publisher, project_id);
  ASSERT_STATUS_OK(names);
  EXPECT_THAT(*names, Not(Contains(topic.FullName())));

  auto create_response = publisher.CreateTopic(TopicBuilder(topic));
  ASSERT_THAT(create_response,
              AnyOf(IsOk(), StatusIs(StatusCode::kAlreadyExists)));
  names = TopicNames(publisher, project_id);
  ASSERT_STATUS_OK(names);
  EXPECT_THAT(*names, Contains(topic.FullName()));

  auto get_response = publisher.GetTopic(topic);
  ASSERT_STATUS_OK(get_response);
  EXPECT_THAT(*create_response, IsProtoEqual(*get_response));

  // Skip, as this is not supported by the emulator.
  if (!UsingEmulator()) {
    auto update_response = publisher.UpdateTopic(
        TopicBuilder(topic).add_label("test-key", "test-value"));
    ASSERT_STATUS_OK(update_response);
  }

  // The integration tests for ListTopicSubscriptions(), DetachSubscription()
  // and ListTopicSnapshots() are found in
  // subscription_admin_integration_test.cc. The tests are uninteresting until
  // one creates a subscription and a snapshot, and doing so here would just
  // complicate this test with little benefit.

  auto delete_response = publisher.DeleteTopic(topic);
  EXPECT_THAT(delete_response, AnyOf(IsOk(), StatusIs(StatusCode::kNotFound)));
  names = TopicNames(publisher, project_id);
  ASSERT_STATUS_OK(names);
  EXPECT_THAT(*names, Not(Contains(topic.FullName())));
}

TEST_F(TopicAdminIntegrationTest, UnifiedCredentials) {
  auto project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  ASSERT_THAT(project_id, Not(IsEmpty()));
  auto options =
      Options{}.set<UnifiedCredentialsOption>(MakeGoogleDefaultCredentials());
  if (UsingEmulator()) {
    options = Options{}
                  .set<UnifiedCredentialsOption>(MakeInsecureCredentials())
                  .set<internal::UseInsecureChannelOption>(true);
  }
  auto client = TopicAdminClient(MakeTopicAdminConnection(std::move(options)));
  ASSERT_STATUS_OK(TopicNames(client, project_id));
}

TEST_F(TopicAdminIntegrationTest, CreateTopicFailure) {
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto publisher = MakeTestTopicAdminClient();
  auto create_response = publisher.CreateTopic(
      TopicBuilder(Topic("invalid-project", "invalid-topic")));
  ASSERT_FALSE(create_response);
}

TEST_F(TopicAdminIntegrationTest, GetTopicFailure) {
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto publisher = MakeTestTopicAdminClient();
  auto response = publisher.GetTopic(Topic("invalid-project", "invalid-topic"));
  ASSERT_FALSE(response);
}

TEST_F(TopicAdminIntegrationTest, UpdateTopicFailure) {
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto publisher = MakeTestTopicAdminClient();
  auto response = publisher.UpdateTopic(
      TopicBuilder(Topic("invalid-project", "invalid-topic")));
  ASSERT_FALSE(response);
}

TEST_F(TopicAdminIntegrationTest, ListTopicsFailure) {
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto publisher = MakeTestTopicAdminClient();
  auto list = publisher.ListTopics("--invalid-project--");
  auto i = list.begin();
  EXPECT_FALSE(i == list.end());
  EXPECT_FALSE(*i);
}

TEST_F(TopicAdminIntegrationTest, DeleteTopicFailure) {
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto publisher = MakeTestTopicAdminClient();
  auto delete_response =
      publisher.DeleteTopic(Topic("invalid-project", "invalid-topic"));
  ASSERT_FALSE(delete_response.ok());
}

TEST_F(TopicAdminIntegrationTest, DetachSubscriptionFailure) {
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto publisher = MakeTestTopicAdminClient();
  auto response = publisher.DetachSubscription(
      Subscription("invalid-project", "invalid-subscription"));
  ASSERT_FALSE(response.ok());
}

TEST_F(TopicAdminIntegrationTest, ListTopicSubscriptionsFailure) {
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto publisher = MakeTestTopicAdminClient();
  auto list = publisher.ListTopicSubscriptions(
      Topic("invalid-project", "invalid-topic"));
  auto i = list.begin();
  EXPECT_FALSE(i == list.end());
  EXPECT_FALSE(*i);
}

TEST_F(TopicAdminIntegrationTest, ListTopicSnapshotsFailure) {
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto publisher = MakeTestTopicAdminClient();
  auto list =
      publisher.ListTopicSnapshots(Topic("invalid-project", "invalid-topic"));
  auto i = list.begin();
  EXPECT_FALSE(i == list.end());
  EXPECT_FALSE(*i);
}

/// @test Verify the backwards compatibility `v1` namespace still exists.
TEST_F(TopicAdminIntegrationTest, BackwardsCompatibility) {
  auto connection =
      ::google::cloud::pubsub::v1::MakeTopicAdminConnection(MakeTestOptions());
  EXPECT_THAT(connection, NotNull());
  ASSERT_NO_FATAL_FAILURE(TopicAdminClient(std::move(connection)));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
