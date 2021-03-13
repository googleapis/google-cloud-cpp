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

#include "google/cloud/pubsub/subscription_admin_client.h"
#include "google/cloud/pubsub/mocks/mock_subscription_admin_connection.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::testing::ElementsAre;

TEST(SubscriptionAdminClient, CreateSubscription) {
  auto mock = std::make_shared<pubsub_mocks::MockSubscriptionAdminConnection>();
  Topic const topic("test-project", "test-topic");
  Subscription const subscription("test-project", "test-subscription");
  auto constexpr kDeadlineSeconds = 1234;
  EXPECT_CALL(*mock, CreateSubscription)
      .WillOnce(
          [&](SubscriptionAdminConnection::CreateSubscriptionParams const& p) {
            EXPECT_EQ(topic.FullName(), p.subscription.topic());
            EXPECT_EQ(subscription.FullName(), p.subscription.name());
            EXPECT_EQ(kDeadlineSeconds, p.subscription.ack_deadline_seconds());
            google::pubsub::v1::Subscription response = p.subscription;
            return make_status_or(response);
          });
  SubscriptionAdminClient client(mock);
  auto const response =
      client.CreateSubscription(topic, subscription,
                                SubscriptionBuilder{}.set_ack_deadline(
                                    std::chrono::seconds(kDeadlineSeconds)));
  EXPECT_STATUS_OK(response);
  EXPECT_EQ(kDeadlineSeconds, response->ack_deadline_seconds());
  EXPECT_EQ(topic.FullName(), response->topic());
  EXPECT_EQ(subscription.FullName(), response->name());
}

TEST(SubscriptionAdminClient, GetSubscription) {
  auto mock = std::make_shared<pubsub_mocks::MockSubscriptionAdminConnection>();
  Subscription const subscription("test-project", "test-subscription");
  auto constexpr kDeadlineSeconds = 1234;
  EXPECT_CALL(*mock, GetSubscription)
      .WillOnce(
          [&](SubscriptionAdminConnection::GetSubscriptionParams const& p) {
            EXPECT_EQ(subscription.FullName(), p.subscription.FullName());
            google::pubsub::v1::Subscription response;
            response.set_name(p.subscription.FullName());
            response.set_ack_deadline_seconds(kDeadlineSeconds);
            return make_status_or(response);
          });
  SubscriptionAdminClient client(mock);
  auto const response = client.GetSubscription(subscription);
  EXPECT_STATUS_OK(response);
  EXPECT_EQ(kDeadlineSeconds, response->ack_deadline_seconds());
  EXPECT_EQ(subscription.FullName(), response->name());
}

TEST(SubscriptionAdminClient, UpdateSubscription) {
  auto mock = std::make_shared<pubsub_mocks::MockSubscriptionAdminConnection>();
  Subscription const subscription("test-project", "test-subscription");
  auto constexpr kDeadlineSeconds = 1234;
  EXPECT_CALL(*mock, UpdateSubscription)
      .WillOnce(
          [&](SubscriptionAdminConnection::UpdateSubscriptionParams const& p) {
            EXPECT_EQ(subscription.FullName(), p.request.subscription().name());
            EXPECT_EQ(kDeadlineSeconds,
                      p.request.subscription().ack_deadline_seconds());
            EXPECT_THAT(p.request.update_mask().paths(),
                        ElementsAre("ack_deadline_seconds"));
            google::pubsub::v1::Subscription response =
                p.request.subscription();
            return make_status_or(response);
          });
  SubscriptionAdminClient client(mock);
  auto const response = client.UpdateSubscription(
      subscription, SubscriptionBuilder{}.set_ack_deadline(
                        std::chrono::seconds(kDeadlineSeconds)));
  EXPECT_STATUS_OK(response);
  EXPECT_EQ(kDeadlineSeconds, response->ack_deadline_seconds());
  EXPECT_EQ(subscription.FullName(), response->name());
}

TEST(SubscriptionAdminClient, ListSubscriptions) {
  auto mock = std::make_shared<pubsub_mocks::MockSubscriptionAdminConnection>();
  auto const s1 = Subscription("test-project", "s1");
  auto const s2 = Subscription("test-project", "s2");
  EXPECT_CALL(*mock, ListSubscriptions)
      .WillOnce([&](SubscriptionAdminConnection::ListSubscriptionsParams const&
                        p) {
        EXPECT_EQ("projects/test-project", p.project_id);
        return internal::MakePaginationRange<pubsub::ListSubscriptionsRange>(
            google::pubsub::v1::ListSubscriptionsRequest{},
            [&](google::pubsub::v1::ListSubscriptionsRequest const&) {
              google::pubsub::v1::ListSubscriptionsResponse response;
              response.add_subscriptions()->set_name(s1.FullName());
              response.add_subscriptions()->set_name(s2.FullName());
              return make_status_or(response);
            },
            [](google::pubsub::v1::ListSubscriptionsResponse const& r) {
              std::vector<google::pubsub::v1::Subscription> items;
              for (auto const& s : r.subscriptions()) items.push_back(s);
              return items;
            });
      });
  SubscriptionAdminClient client(mock);
  std::vector<std::string> names;
  for (auto const& t : client.ListSubscriptions("test-project")) {
    ASSERT_STATUS_OK(t);
    names.push_back(t->name());
  }
  EXPECT_THAT(names, ElementsAre(s1.FullName(), s2.FullName()));
}

TEST(SubscriptionAdminClient, DeleteSubscription) {
  auto mock = std::make_shared<pubsub_mocks::MockSubscriptionAdminConnection>();
  Subscription const subscription("test-project", "test-subscription");
  EXPECT_CALL(*mock, DeleteSubscription)
      .WillOnce(
          [&](SubscriptionAdminConnection::DeleteSubscriptionParams const& p) {
            EXPECT_EQ(subscription.FullName(), p.subscription.FullName());
            return Status{};
          });
  SubscriptionAdminClient client(mock);
  auto const response = client.DeleteSubscription(subscription);
  EXPECT_STATUS_OK(response);
}

TEST(SubscriptionAdminClient, ModifyPushConfig) {
  auto mock = std::make_shared<pubsub_mocks::MockSubscriptionAdminConnection>();
  Subscription const subscription("test-project", "test-subscription");
  EXPECT_CALL(*mock, ModifyPushConfig)
      .WillOnce(
          [&](SubscriptionAdminConnection::ModifyPushConfigParams const& p) {
            EXPECT_EQ(subscription.FullName(), p.request.subscription());
            EXPECT_EQ("https://test-endpoint.example.com",
                      p.request.push_config().push_endpoint());
            return Status{};
          });
  SubscriptionAdminClient client(mock);
  auto const response = client.ModifyPushSubscription(
      subscription, PushConfigBuilder{}.set_push_endpoint(
                        "https://test-endpoint.example.com"));
  EXPECT_STATUS_OK(response);
}

TEST(SubscriptionAdminClient, CreateSnapshot) {
  auto mock = std::make_shared<pubsub_mocks::MockSubscriptionAdminConnection>();
  Subscription const subscription("test-project", "test-subscription");
  Snapshot const snapshot("test-project", "test-snapshot");
  EXPECT_CALL(*mock, CreateSnapshot)
      .WillOnce(
          [&](SubscriptionAdminConnection::CreateSnapshotParams const& p) {
            EXPECT_EQ(subscription.FullName(), p.request.subscription());
            EXPECT_EQ(snapshot.FullName(), p.request.name());
            google::pubsub::v1::Snapshot response;
            response.set_name(p.request.name());
            return make_status_or(response);
          });
  SubscriptionAdminClient client(mock);
  auto const response = client.CreateSnapshot(
      subscription, snapshot, SnapshotBuilder{}.add_label("k0", "l0"));
  EXPECT_STATUS_OK(response);
  EXPECT_EQ(snapshot.FullName(), response->name());
}

TEST(SubscriptionAdminClient, GetSnapshot) {
  auto mock = std::make_shared<pubsub_mocks::MockSubscriptionAdminConnection>();
  Snapshot const snapshot("test-project", "test-snapshot");
  EXPECT_CALL(*mock, GetSnapshot)
      .WillOnce([&](SubscriptionAdminConnection::GetSnapshotParams const& p) {
        EXPECT_EQ(snapshot.FullName(), p.snapshot.FullName());
        google::pubsub::v1::Snapshot response;
        response.set_name(p.snapshot.FullName());
        return make_status_or(response);
      });
  SubscriptionAdminClient client(mock);
  auto const response = client.GetSnapshot(snapshot);
  EXPECT_STATUS_OK(response);
  EXPECT_EQ(snapshot.FullName(), response->name());
}

TEST(SubscriptionAdminClient, UpdateSnapshot) {
  auto mock = std::make_shared<pubsub_mocks::MockSubscriptionAdminConnection>();
  Snapshot const snapshot("test-project", "test-snapshot");
  EXPECT_CALL(*mock, UpdateSnapshot)
      .WillOnce(
          [&](SubscriptionAdminConnection::UpdateSnapshotParams const& p) {
            EXPECT_EQ(snapshot.FullName(), p.request.snapshot().name());
            EXPECT_THAT(p.request.update_mask().paths(), ElementsAre("labels"));
            google::pubsub::v1::Snapshot response = p.request.snapshot();
            return make_status_or(response);
          });
  SubscriptionAdminClient client(mock);
  auto const response =
      client.UpdateSnapshot(snapshot, SnapshotBuilder{}.add_label("k1", "l1"));
  EXPECT_STATUS_OK(response);
  EXPECT_EQ(snapshot.FullName(), response->name());
}

TEST(SubscriptionAdminClient, ListSnapshots) {
  auto mock = std::make_shared<pubsub_mocks::MockSubscriptionAdminConnection>();
  auto const s1 = Snapshot("test-project", "s1");
  auto const s2 = Snapshot("test-project", "s2");
  EXPECT_CALL(*mock, ListSnapshots)
      .WillOnce([&](SubscriptionAdminConnection::ListSnapshotsParams const& p) {
        EXPECT_EQ("projects/test-project", p.project_id);
        return internal::MakePaginationRange<pubsub::ListSnapshotsRange>(
            google::pubsub::v1::ListSnapshotsRequest{},
            [&](google::pubsub::v1::ListSnapshotsRequest const&) {
              google::pubsub::v1::ListSnapshotsResponse response;
              response.add_snapshots()->set_name(s1.FullName());
              response.add_snapshots()->set_name(s2.FullName());
              return make_status_or(response);
            },
            [](google::pubsub::v1::ListSnapshotsResponse const& r) {
              std::vector<google::pubsub::v1::Snapshot> items;
              for (auto const& s : r.snapshots()) items.push_back(s);
              return items;
            });
      });
  SubscriptionAdminClient client(mock);
  std::vector<std::string> names;
  for (auto const& t : client.ListSnapshots("test-project")) {
    ASSERT_STATUS_OK(t);
    names.push_back(t->name());
  }
  EXPECT_THAT(names, ElementsAre(s1.FullName(), s2.FullName()));
}

TEST(SubscriptionAdminClient, DeleteSnapshot) {
  auto mock = std::make_shared<pubsub_mocks::MockSubscriptionAdminConnection>();
  Snapshot const snapshot("test-project", "test-snapshot");
  EXPECT_CALL(*mock, DeleteSnapshot)
      .WillOnce(
          [&](SubscriptionAdminConnection::DeleteSnapshotParams const& p) {
            EXPECT_EQ(snapshot.FullName(), p.snapshot.FullName());
            return Status{};
          });
  SubscriptionAdminClient client(mock);
  auto const response = client.DeleteSnapshot(snapshot);
  EXPECT_STATUS_OK(response);
}

TEST(SubscriptionAdminClient, Seek) {
  auto mock = std::make_shared<pubsub_mocks::MockSubscriptionAdminConnection>();
  Subscription const subscription("test-project", "test-subscription");
  Snapshot const snapshot("test-project", "test-snapshot");
  EXPECT_CALL(*mock, Seek)
      .WillOnce([&](SubscriptionAdminConnection::SeekParams const& p) {
        EXPECT_EQ(subscription.FullName(), p.request.subscription());
        EXPECT_EQ(snapshot.FullName(), p.request.snapshot());
        google::pubsub::v1::SeekResponse response;
        return make_status_or(response);
      });
  SubscriptionAdminClient client(mock);
  auto const response = client.Seek(subscription, snapshot);
  EXPECT_STATUS_OK(response);
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
