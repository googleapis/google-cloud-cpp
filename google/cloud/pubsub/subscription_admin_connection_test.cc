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
#include "google/cloud/pubsub/snapshot_mutation_builder.h"
#include "google/cloud/pubsub/testing/mock_subscriber_stub.h"
#include "google/cloud/pubsub/topic.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/capture_log_lines_backend.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::google::cloud::testing_util::IsProtoEqual;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::HasSubstr;

TEST(SubscriptionAdminConnectionTest, Create) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  Subscription const subscription("test-project", "test-subscription");

  EXPECT_CALL(*mock, CreateSubscription)
      .WillOnce([&](grpc::ClientContext&,
                    google::pubsub::v1::Subscription const& request) {
        EXPECT_EQ(subscription.FullName(), request.name());
        return make_status_or(request);
      });

  auto subscription_admin =
      pubsub_internal::MakeSubscriptionAdminConnection({}, mock);
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

  auto subscription_admin =
      pubsub_internal::MakeSubscriptionAdminConnection({}, mock);
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
      .WillOnce([&](grpc::ClientContext&,
                    google::pubsub::v1::GetSubscriptionRequest const& request) {
        EXPECT_EQ(subscription.FullName(), request.subscription());
        return make_status_or(expected);
      });

  auto subscription_admin =
      pubsub_internal::MakeSubscriptionAdminConnection({}, mock);
  auto response = subscription_admin->GetSubscription({subscription});
  ASSERT_STATUS_OK(response);
  EXPECT_THAT(*response, IsProtoEqual(expected));
}

TEST(SubscriptionAdminConnectionTest, Update) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  Subscription const subscription("test-project", "test-subscription");

  EXPECT_CALL(*mock, UpdateSubscription)
      .WillOnce(
          [&](grpc::ClientContext&,
              google::pubsub::v1::UpdateSubscriptionRequest const& request) {
            EXPECT_EQ(subscription.FullName(), request.subscription().name());
            EXPECT_THAT(request.update_mask().paths(),
                        Contains("ack_deadline_seconds"));
            return make_status_or(request.subscription());
          });

  auto subscription_admin =
      pubsub_internal::MakeSubscriptionAdminConnection({}, mock);
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
  auto backend =
      std::make_shared<google::cloud::testing_util::CaptureLogLinesBackend>();
  auto id = google::cloud::LogSink::Instance().AddBackend(backend);

  EXPECT_CALL(*mock, DeleteSubscription)
      .WillOnce(
          [&](grpc::ClientContext&,
              google::pubsub::v1::DeleteSubscriptionRequest const& request) {
            EXPECT_EQ(subscription.FullName(), request.subscription());
            return Status{};
          });

  auto subscription_admin = pubsub_internal::MakeSubscriptionAdminConnection(
      ConnectionOptions{}.enable_tracing("rpc"), mock);
  auto response = subscription_admin->DeleteSubscription({subscription});
  ASSERT_STATUS_OK(response);

  EXPECT_THAT(backend->log_lines, Contains(HasSubstr("DeleteSubscription")));
  google::cloud::LogSink::Instance().RemoveBackend(id);
}

TEST(SubscriptionAdminConnectionTest, CreateSnapshot) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  Topic const topic("test-project", "test-topic");
  Subscription const subscription("test-project", "test-subscription");
  google::pubsub::v1::Snapshot expected;
  expected.set_topic(topic.FullName());
  expected.set_name("projects/test-project/snapshots/test-snapshot-0001");

  EXPECT_CALL(*mock, CreateSnapshot)
      .WillOnce([&](grpc::ClientContext&,
                    google::pubsub::v1::CreateSnapshotRequest const& request) {
        EXPECT_EQ(subscription.FullName(), request.subscription());
        EXPECT_TRUE(request.name().empty());
        return make_status_or(expected);
      });

  auto subscription_admin =
      pubsub_internal::MakeSubscriptionAdminConnection({}, mock);
  auto response = subscription_admin->CreateSnapshot(
      {SnapshotMutationBuilder{}.BuildCreateMutation(subscription)});
  ASSERT_STATUS_OK(response);
  EXPECT_THAT(*response, IsProtoEqual(expected));
}

TEST(SubscriptionAdminConnectionTest, GetSnapshot) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  Topic const topic("test-project", "test-topic");
  Snapshot const snapshot("test-project", "test-snapshot-0001");
  google::pubsub::v1::Snapshot expected;
  expected.set_topic(topic.FullName());
  expected.set_name(snapshot.FullName());

  EXPECT_CALL(*mock, GetSnapshot)
      .WillOnce([&](grpc::ClientContext&,
                    google::pubsub::v1::GetSnapshotRequest const& request) {
        EXPECT_EQ(snapshot.FullName(), request.snapshot());

        return make_status_or(expected);
      });

  auto subscription_admin =
      pubsub_internal::MakeSubscriptionAdminConnection({}, mock);
  auto response = subscription_admin->GetSnapshot({snapshot});
  ASSERT_STATUS_OK(response);
  EXPECT_THAT(*response, IsProtoEqual(expected));
}

TEST(SubscriptionAdminConnectionTest, ListSnapshots) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();

  EXPECT_CALL(*mock, ListSnapshots)
      .WillOnce([&](grpc::ClientContext&,
                    google::pubsub::v1::ListSnapshotsRequest const& request) {
        EXPECT_EQ("projects/test-project-id", request.project());
        EXPECT_TRUE(request.page_token().empty());
        google::pubsub::v1::ListSnapshotsResponse response;
        response.add_snapshots()->set_name("test-snapshot-01");
        response.add_snapshots()->set_name("test-snapshot-02");
        return make_status_or(response);
      });

  auto snapshot_admin =
      pubsub_internal::MakeSubscriptionAdminConnection({}, mock);
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
      .WillOnce([&](grpc::ClientContext&,
                    google::pubsub::v1::UpdateSnapshotRequest const& request) {
        EXPECT_EQ(snapshot.FullName(), request.snapshot().name());
        return make_status_or(expected);
      });

  auto subscription_admin =
      pubsub_internal::MakeSubscriptionAdminConnection({}, mock);
  auto response = subscription_admin->UpdateSnapshot(
      {SnapshotMutationBuilder{}.BuildUpdateMutation(snapshot)});
  ASSERT_STATUS_OK(response);
  EXPECT_THAT(*response, IsProtoEqual(expected));
}

TEST(SubscriptionAdminConnectionTest, DeleteSnapshot) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  Snapshot const snapshot("test-project", "test-snapshot");

  EXPECT_CALL(*mock, DeleteSnapshot)
      .WillOnce([&](grpc::ClientContext&,
                    google::pubsub::v1::DeleteSnapshotRequest const& request) {
        EXPECT_EQ(snapshot.FullName(), request.snapshot());
        return Status{};
      });

  auto snapshot_admin = pubsub_internal::MakeSubscriptionAdminConnection(
      ConnectionOptions{}.enable_tracing("rpc"), mock);
  auto response = snapshot_admin->DeleteSnapshot({snapshot});
  ASSERT_STATUS_OK(response);
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
