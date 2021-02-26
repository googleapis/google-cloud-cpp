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

#include "google/cloud/pubsub/subscription_admin_connection.h"
#include "google/cloud/pubsub/snapshot_builder.h"
#include "google/cloud/pubsub/testing/mock_subscriber_stub.h"
#include "google/cloud/pubsub/testing/test_retry_policies.h"
#include "google/cloud/pubsub/topic.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::google::cloud::pubsub_testing::TestBackoffPolicy;
using ::google::cloud::pubsub_testing::TestRetryPolicy;
using ::google::cloud::testing_util::IsContextMDValid;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::Return;

TEST(SubscriptionAdminConnectionTest, Create) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  Subscription const subscription("test-project", "test-subscription");

  EXPECT_CALL(*mock, CreateSubscription)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce([&](grpc::ClientContext&,
                    google::pubsub::v1::Subscription const& request) {
        EXPECT_EQ(subscription.FullName(), request.name());
        return make_status_or(request);
      });

  auto subscription_admin = pubsub_internal::MakeSubscriptionAdminConnection(
      {}, mock, TestRetryPolicy(), TestBackoffPolicy());
  google::pubsub::v1::Subscription expected;
  expected.set_topic("test-topic-name");
  expected.set_name(subscription.FullName());
  auto response = subscription_admin->CreateSubscription({expected});
  ASSERT_STATUS_OK(response);
  EXPECT_THAT(*response, IsProtoEqual(expected));
}

/// @test Verify the metadata decorator is configured by default
TEST(SubscriptionAdminConnectionTest, CreateWithMetadata) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  Subscription const subscription("test-project", "test-subscription");

  EXPECT_CALL(*mock, CreateSubscription)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce([&](grpc::ClientContext& context,
                    google::pubsub::v1::Subscription const& request) {
        EXPECT_STATUS_OK(IsContextMDValid(
            context, "google.pubsub.v1.Subscriber.CreateSubscription",
            google::cloud::internal::ApiClientHeader()));
        EXPECT_EQ(subscription.FullName(), request.name());
        return make_status_or(request);
      });

  auto subscription_admin = pubsub_internal::MakeSubscriptionAdminConnection(
      {}, mock, TestRetryPolicy(), TestBackoffPolicy());
  google::pubsub::v1::Subscription expected;
  expected.set_topic("test-topic-name");
  expected.set_name(subscription.FullName());
  auto response = subscription_admin->CreateSubscription({expected});
  ASSERT_STATUS_OK(response);
  EXPECT_THAT(*response, IsProtoEqual(expected));
}

TEST(SubscriptionAdminConnectionTest, List) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();

  EXPECT_CALL(*mock, ListSubscriptions)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce(
          [&](grpc::ClientContext&,
              google::pubsub::v1::ListSubscriptionsRequest const& request) {
            EXPECT_EQ("projects/test-project-id", request.project());
            EXPECT_TRUE(request.page_token().empty());
            google::pubsub::v1::ListSubscriptionsResponse response;
            response.add_subscriptions()->set_name("test-subscription-01");
            response.add_subscriptions()->set_name("test-subscription-02");
            return make_status_or(response);
          });

  auto subscription_admin = pubsub_internal::MakeSubscriptionAdminConnection(
      {}, mock, TestRetryPolicy(), TestBackoffPolicy());
  std::vector<std::string> topic_names;
  for (auto& t :
       subscription_admin->ListSubscriptions({"projects/test-project-id"})) {
    ASSERT_STATUS_OK(t);
    topic_names.push_back(t->name());
  }
  EXPECT_THAT(topic_names,
              ElementsAre("test-subscription-01", "test-subscription-02"));
}

TEST(SubscriptionAdminConnectionTest, Get) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  Subscription const subscription("test-project", "test-subscription");
  google::pubsub::v1::Subscription expected;
  expected.set_topic("test-topic-name");
  expected.set_name(subscription.FullName());

  EXPECT_CALL(*mock, GetSubscription)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce([&](grpc::ClientContext&,
                    google::pubsub::v1::GetSubscriptionRequest const& request) {
        EXPECT_EQ(subscription.FullName(), request.subscription());
        return make_status_or(expected);
      });

  auto subscription_admin = pubsub_internal::MakeSubscriptionAdminConnection(
      {}, mock, TestRetryPolicy(), TestBackoffPolicy());
  auto response = subscription_admin->GetSubscription({subscription});
  ASSERT_STATUS_OK(response);
  EXPECT_THAT(*response, IsProtoEqual(expected));
}

TEST(SubscriptionAdminConnectionTest, Update) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  Subscription const subscription("test-project", "test-subscription");

  EXPECT_CALL(*mock, UpdateSubscription)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce(
          [&](grpc::ClientContext&,
              google::pubsub::v1::UpdateSubscriptionRequest const& request) {
            EXPECT_EQ(subscription.FullName(), request.subscription().name());
            EXPECT_THAT(request.update_mask().paths(),
                        Contains("ack_deadline_seconds"));
            return make_status_or(request.subscription());
          });

  auto subscription_admin = pubsub_internal::MakeSubscriptionAdminConnection(
      {}, mock, TestRetryPolicy(), TestBackoffPolicy());
  google::pubsub::v1::Subscription expected;
  expected.set_name(subscription.FullName());
  expected.set_ack_deadline_seconds(1);

  google::pubsub::v1::UpdateSubscriptionRequest request;
  *request.mutable_subscription() = expected;
  request.mutable_update_mask()->add_paths("ack_deadline_seconds");
  auto response = subscription_admin->UpdateSubscription({request});
  ASSERT_STATUS_OK(response);
  EXPECT_THAT(*response, IsProtoEqual(expected));
}

/**
 * @test Verify DeleteTopic() and logging works.
 *
 * We use this test for both DeleteTopic and logging. DeleteTopic has a simple
 * return type, so it is a good candidate to do the logging test too.
 */
TEST(SubscriptionAdminConnectionTest, DeleteWithLogging) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  Subscription const subscription("test-project", "test-subscription");
  testing_util::ScopedLog log;

  EXPECT_CALL(*mock, DeleteSubscription)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce(
          [&](grpc::ClientContext&,
              google::pubsub::v1::DeleteSubscriptionRequest const& request) {
            EXPECT_EQ(subscription.FullName(), request.subscription());
            return Status{};
          });

  auto subscription_admin = pubsub_internal::MakeSubscriptionAdminConnection(
      ConnectionOptions{}.enable_tracing("rpc"), mock, TestRetryPolicy(),
      TestBackoffPolicy());
  auto response = subscription_admin->DeleteSubscription({subscription});
  ASSERT_STATUS_OK(response);

  EXPECT_THAT(log.ExtractLines(), Contains(HasSubstr("DeleteSubscription")));
}

TEST(SubscriptionAdminConnectionTest, ModifyPushConfig) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  Subscription const subscription("test-project", "test-subscription");
  testing_util::ScopedLog log;

  EXPECT_CALL(*mock, ModifyPushConfig)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce(
          [&](grpc::ClientContext&,
              google::pubsub::v1::ModifyPushConfigRequest const& request) {
            EXPECT_EQ(subscription.FullName(), request.subscription());
            return Status{};
          });

  auto subscription_admin = pubsub_internal::MakeSubscriptionAdminConnection(
      ConnectionOptions{}.enable_tracing("rpc"), mock, TestRetryPolicy(),
      TestBackoffPolicy());
  google::pubsub::v1::ModifyPushConfigRequest request;
  request.set_subscription(subscription.FullName());
  auto response = subscription_admin->ModifyPushConfig({request});
  ASSERT_STATUS_OK(response);

  EXPECT_THAT(log.ExtractLines(), Contains(HasSubstr("ModifyPushConfig")));
}

TEST(SubscriptionAdminConnectionTest, CreateSnapshot) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  Topic const topic("test-project", "test-topic");
  Subscription const subscription("test-project", "test-subscription");
  Snapshot const snapshot("test-project", "test-snapshot-0001");
  google::pubsub::v1::Snapshot expected;
  expected.set_topic(topic.FullName());
  expected.set_name(snapshot.FullName());

  EXPECT_CALL(*mock, CreateSnapshot)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce([&](grpc::ClientContext&,
                    google::pubsub::v1::CreateSnapshotRequest const& request) {
        EXPECT_EQ(subscription.FullName(), request.subscription());
        EXPECT_FALSE(request.name().empty());
        return make_status_or(expected);
      });

  auto subscription_admin = pubsub_internal::MakeSubscriptionAdminConnection(
      {}, mock, TestRetryPolicy(), TestBackoffPolicy());
  auto response = subscription_admin->CreateSnapshot(
      {SnapshotBuilder{}.BuildCreateRequest(subscription, snapshot)});
  ASSERT_STATUS_OK(response);
  EXPECT_THAT(*response, IsProtoEqual(expected));
}

TEST(SubscriptionAdminConnectionTest, CreateSnapshotNotIdempotent) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  Topic const topic("test-project", "test-topic");
  Subscription const subscription("test-project", "test-subscription");
  Snapshot const snapshot("test-project", "test-snapshot-0001");
  google::pubsub::v1::Snapshot expected;
  expected.set_topic(topic.FullName());
  expected.set_name(snapshot.FullName());

  EXPECT_CALL(*mock, CreateSnapshot)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")));

  auto subscription_admin = pubsub_internal::MakeSubscriptionAdminConnection(
      {}, mock, TestRetryPolicy(), TestBackoffPolicy());
  auto response = subscription_admin->CreateSnapshot(
      {SnapshotBuilder{}.BuildCreateRequest(subscription)});
  EXPECT_THAT(response,
              StatusIs(StatusCode::kUnavailable, HasSubstr("try-again")));
}

TEST(SubscriptionAdminConnectionTest, GetSnapshot) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  Topic const topic("test-project", "test-topic");
  Snapshot const snapshot("test-project", "test-snapshot-0001");
  google::pubsub::v1::Snapshot expected;
  expected.set_topic(topic.FullName());
  expected.set_name(snapshot.FullName());

  EXPECT_CALL(*mock, GetSnapshot)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce([&](grpc::ClientContext&,
                    google::pubsub::v1::GetSnapshotRequest const& request) {
        EXPECT_EQ(snapshot.FullName(), request.snapshot());

        return make_status_or(expected);
      });

  auto subscription_admin = pubsub_internal::MakeSubscriptionAdminConnection(
      {}, mock, TestRetryPolicy(), TestBackoffPolicy());
  auto response = subscription_admin->GetSnapshot({snapshot});
  ASSERT_STATUS_OK(response);
  EXPECT_THAT(*response, IsProtoEqual(expected));
}

TEST(SubscriptionAdminConnectionTest, ListSnapshots) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();

  EXPECT_CALL(*mock, ListSnapshots)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce([&](grpc::ClientContext&,
                    google::pubsub::v1::ListSnapshotsRequest const& request) {
        EXPECT_EQ("projects/test-project-id", request.project());
        EXPECT_TRUE(request.page_token().empty());
        google::pubsub::v1::ListSnapshotsResponse response;
        response.add_snapshots()->set_name("test-snapshot-01");
        response.add_snapshots()->set_name("test-snapshot-02");
        return make_status_or(response);
      });

  auto snapshot_admin = pubsub_internal::MakeSubscriptionAdminConnection(
      {}, mock, TestRetryPolicy(), TestBackoffPolicy());
  std::vector<std::string> names;
  for (auto& t : snapshot_admin->ListSnapshots({"projects/test-project-id"})) {
    ASSERT_STATUS_OK(t);
    names.push_back(t->name());
  }
  EXPECT_THAT(names, ElementsAre("test-snapshot-01", "test-snapshot-02"));
}

TEST(SubscriptionAdminConnectionTest, UpdateSnapshot) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  Topic const topic("test-project", "test-topic");
  Subscription const subscription("test-project", "test-subscription");
  Snapshot const snapshot("test-project", "test-snapshot-0001");
  google::pubsub::v1::Snapshot expected;
  expected.set_topic(topic.FullName());
  expected.set_name(snapshot.FullName());

  EXPECT_CALL(*mock, UpdateSnapshot)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce([&](grpc::ClientContext&,
                    google::pubsub::v1::UpdateSnapshotRequest const& request) {
        EXPECT_EQ(snapshot.FullName(), request.snapshot().name());
        return make_status_or(expected);
      });

  auto subscription_admin = pubsub_internal::MakeSubscriptionAdminConnection(
      {}, mock, TestRetryPolicy(), TestBackoffPolicy());
  auto response = subscription_admin->UpdateSnapshot(
      {SnapshotBuilder{}.BuildUpdateRequest(snapshot)});
  ASSERT_STATUS_OK(response);
  EXPECT_THAT(*response, IsProtoEqual(expected));
}

TEST(SubscriptionAdminConnectionTest, DeleteSnapshot) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  Snapshot const snapshot("test-project", "test-snapshot");

  EXPECT_CALL(*mock, DeleteSnapshot)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce([&](grpc::ClientContext&,
                    google::pubsub::v1::DeleteSnapshotRequest const& request) {
        EXPECT_EQ(snapshot.FullName(), request.snapshot());
        return Status{};
      });

  auto snapshot_admin = pubsub_internal::MakeSubscriptionAdminConnection(
      ConnectionOptions{}.enable_tracing("rpc"), mock, TestRetryPolicy(),
      TestBackoffPolicy());
  auto response = snapshot_admin->DeleteSnapshot({snapshot});
  ASSERT_STATUS_OK(response);
}

TEST(SubscriptionAdminConnectionTest, Seek) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  Subscription const subscription("test-project", "test-subscription");
  Snapshot const snapshot("test-project", "test-snapshot");

  EXPECT_CALL(*mock, Seek)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce([&](grpc::ClientContext&,
                    google::pubsub::v1::SeekRequest const& request) {
        EXPECT_EQ(subscription.FullName(), request.subscription());
        EXPECT_EQ(snapshot.FullName(), request.snapshot());
        return make_status_or(google::pubsub::v1::SeekResponse{});
      });

  auto snapshot_admin = pubsub_internal::MakeSubscriptionAdminConnection(
      ConnectionOptions{}.enable_tracing("rpc"), mock, TestRetryPolicy(),
      TestBackoffPolicy());
  google::pubsub::v1::SeekRequest request;
  request.set_subscription(subscription.FullName());
  request.set_snapshot(snapshot.FullName());
  auto response = snapshot_admin->Seek({request});
  ASSERT_STATUS_OK(response);
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
