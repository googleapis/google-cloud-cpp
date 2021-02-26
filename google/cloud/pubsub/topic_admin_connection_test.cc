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

#include "google/cloud/pubsub/topic_admin_connection.h"
#include "google/cloud/pubsub/testing/mock_publisher_stub.h"
#include "google/cloud/pubsub/testing/test_retry_policies.h"
#include "google/cloud/pubsub/topic_builder.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::google::cloud::pubsub_testing::TestBackoffPolicy;
using ::google::cloud::pubsub_testing::TestRetryPolicy;
using ::google::cloud::testing_util::IsProtoEqual;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::Return;

TEST(TopicAdminConnectionTest, Create) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  Topic const topic("test-project", "test-topic");

  EXPECT_CALL(*mock, CreateTopic)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce(
          [&](grpc::ClientContext&, google::pubsub::v1::Topic const& request) {
            EXPECT_EQ(topic.FullName(), request.name());
            return make_status_or(request);
          });

  auto topic_admin = pubsub_internal::MakeTopicAdminConnection(
      {}, mock, TestRetryPolicy(), TestBackoffPolicy());
  auto const expected = TopicBuilder(topic).BuildCreateRequest();
  auto response = topic_admin->CreateTopic({expected});
  ASSERT_STATUS_OK(response);
  EXPECT_THAT(*response, IsProtoEqual(expected));
}

/// @test verify the metadata decorator is automatically configured.
TEST(TopicAdminConnectionTest, Metadata) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  Topic const topic("test-project", "test-topic");

  EXPECT_CALL(*mock, CreateTopic)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce([&](grpc::ClientContext& context,
                    google::pubsub::v1::Topic const& request) {
        EXPECT_STATUS_OK(google::cloud::testing_util::IsContextMDValid(
            context, "google.pubsub.v1.Publisher.CreateTopic",
            google::cloud::internal::ApiClientHeader()));
        return make_status_or(request);
      });

  auto topic_admin = pubsub_internal::MakeTopicAdminConnection(
      {}, mock, TestRetryPolicy(), TestBackoffPolicy());
  auto const expected = TopicBuilder(topic).BuildCreateRequest();
  auto response = topic_admin->CreateTopic({expected});
  ASSERT_STATUS_OK(response);
  EXPECT_THAT(*response, IsProtoEqual(expected));
}

TEST(TopicAdminConnectionTest, Get) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  Topic const topic("test-project", "test-topic");
  google::pubsub::v1::Topic expected;
  expected.set_name(topic.FullName());

  EXPECT_CALL(*mock, GetTopic)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce([&](grpc::ClientContext&,
                    google::pubsub::v1::GetTopicRequest const& request) {
        EXPECT_EQ(topic.FullName(), request.topic());
        return make_status_or(expected);
      });

  auto topic_admin = pubsub_internal::MakeTopicAdminConnection(
      {}, mock, TestRetryPolicy(), TestBackoffPolicy());
  auto response = topic_admin->GetTopic({topic});
  ASSERT_STATUS_OK(response);
  EXPECT_THAT(*response, IsProtoEqual(expected));
}

TEST(TopicAdminConnectionTest, Update) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  Topic const topic("test-project", "test-topic");
  google::pubsub::v1::Topic expected;
  expected.set_name(topic.FullName());

  EXPECT_CALL(*mock, UpdateTopic)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce([&](grpc::ClientContext&,
                    google::pubsub::v1::UpdateTopicRequest const& request) {
        EXPECT_EQ(topic.FullName(), request.topic().name());
        EXPECT_THAT(request.update_mask().paths(), ElementsAre("labels"));
        return make_status_or(expected);
      });

  auto topic_admin = pubsub_internal::MakeTopicAdminConnection(
      {}, mock, TestRetryPolicy(), TestBackoffPolicy());
  auto response =
      topic_admin->UpdateTopic({TopicBuilder(topic)
                                    .add_label("test-key", "test-value")
                                    .BuildUpdateRequest()});
  ASSERT_STATUS_OK(response);
  EXPECT_THAT(*response, IsProtoEqual(expected));
}

TEST(TopicAdminConnectionTest, List) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();

  EXPECT_CALL(*mock, ListTopics)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce([&](grpc::ClientContext&,
                    google::pubsub::v1::ListTopicsRequest const& request) {
        EXPECT_EQ("projects/test-project-id", request.project());
        EXPECT_TRUE(request.page_token().empty());
        google::pubsub::v1::ListTopicsResponse response;
        response.add_topics()->set_name("test-topic-01");
        response.add_topics()->set_name("test-topic-02");
        return make_status_or(response);
      });

  auto topic_admin = pubsub_internal::MakeTopicAdminConnection(
      {}, mock, TestRetryPolicy(), TestBackoffPolicy());
  std::vector<std::string> topic_names;
  for (auto& t : topic_admin->ListTopics({"projects/test-project-id"})) {
    ASSERT_STATUS_OK(t);
    topic_names.push_back(t->name());
  }
  EXPECT_THAT(topic_names, ElementsAre("test-topic-01", "test-topic-02"));
}

/**
 * @test Verify DeleteTopic() and logging works.
 *
 * We use this test for both DeleteTopic and logging. DeleteTopic has a simple
 * return type, so it is a good candidate to do the logging test too.
 */
TEST(TopicAdminConnectionTest, DeleteWithLogging) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  Topic const topic("test-project", "test-topic");
  testing_util::ScopedLog log;

  EXPECT_CALL(*mock, DeleteTopic)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce([&](grpc::ClientContext&,
                    google::pubsub::v1::DeleteTopicRequest const& request) {
        EXPECT_EQ(topic.FullName(), request.topic());
        return Status{};
      });

  auto topic_admin = pubsub_internal::MakeTopicAdminConnection(
      ConnectionOptions{}.enable_tracing("rpc"), mock, TestRetryPolicy(),
      TestBackoffPolicy());
  auto response = topic_admin->DeleteTopic({topic});
  ASSERT_STATUS_OK(response);

  EXPECT_THAT(log.ExtractLines(), Contains(HasSubstr("DeleteTopic")));
}

TEST(TopicAdminConnectionTest, DetachSubscription) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  Subscription const subscription("test-project", "test-subscription");

  EXPECT_CALL(*mock, DetachSubscription)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce(
          [&](grpc::ClientContext&,
              google::pubsub::v1::DetachSubscriptionRequest const& request) {
            EXPECT_EQ(subscription.FullName(), request.subscription());
            return make_status_or(
                google::pubsub::v1::DetachSubscriptionResponse{});
          });

  auto topic_admin = pubsub_internal::MakeTopicAdminConnection(
      ConnectionOptions{}.enable_tracing("rpc"), mock, TestRetryPolicy(),
      TestBackoffPolicy());
  auto response = topic_admin->DetachSubscription({subscription});
  ASSERT_STATUS_OK(response);
}

TEST(TopicAdminConnectionTest, ListSubscriptions) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  auto const topic_name = Topic("test-project-id", "test-topic-id").FullName();
  EXPECT_CALL(*mock, ListTopicSubscriptions)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce([&](grpc::ClientContext&,
                    google::pubsub::v1::ListTopicSubscriptionsRequest const&
                        request) {
        EXPECT_EQ(topic_name, request.topic());
        EXPECT_TRUE(request.page_token().empty());
        google::pubsub::v1::ListTopicSubscriptionsResponse response;
        response.add_subscriptions("projects/test-project-id/subscriptions/s1");
        response.add_subscriptions("projects/test-project-id/subscriptions/s2");
        return make_status_or(response);
      });

  auto topic_admin = pubsub_internal::MakeTopicAdminConnection(
      {}, mock, TestRetryPolicy(), TestBackoffPolicy());
  std::vector<std::string> names;
  for (auto& t : topic_admin->ListTopicSubscriptions({topic_name})) {
    ASSERT_STATUS_OK(t);
    names.push_back(std::move(*t));
  }
  EXPECT_THAT(names, ElementsAre("projects/test-project-id/subscriptions/s1",
                                 "projects/test-project-id/subscriptions/s2"));
}

TEST(TopicAdminConnectionTest, ListSnapshots) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  auto const topic_name = Topic("test-project-id", "test-topic-id").FullName();
  EXPECT_CALL(*mock, ListTopicSnapshots)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce(
          [&](grpc::ClientContext&,
              google::pubsub::v1::ListTopicSnapshotsRequest const& request) {
            EXPECT_EQ(topic_name, request.topic());
            EXPECT_TRUE(request.page_token().empty());
            google::pubsub::v1::ListTopicSnapshotsResponse response;
            response.add_snapshots("projects/test-project-id/snapshots/s1");
            response.add_snapshots("projects/test-project-id/snapshots/s2");
            return make_status_or(response);
          });

  auto topic_admin = pubsub_internal::MakeTopicAdminConnection(
      {}, mock, TestRetryPolicy(), TestBackoffPolicy());
  std::vector<std::string> names;
  for (auto& t : topic_admin->ListTopicSnapshots({topic_name})) {
    ASSERT_STATUS_OK(t);
    names.push_back(std::move(*t));
  }
  EXPECT_THAT(names, ElementsAre("projects/test-project-id/snapshots/s1",
                                 "projects/test-project-id/snapshots/s2"));
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
