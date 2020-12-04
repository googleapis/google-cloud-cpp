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

#include "google/cloud/pubsub/topic_admin_client.h"
#include "google/cloud/pubsub/mocks/mock_topic_admin_connection.h"
#include "google/cloud/testing_util/assert_ok.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::testing::ElementsAre;

TEST(TopicAdminClient, CreateTopic) {
  auto mock = std::make_shared<pubsub_mocks::MockTopicAdminConnection>();
  Topic const topic("test-project", "test-topic");
  EXPECT_CALL(*mock, CreateTopic)
      .WillOnce([&](TopicAdminConnection::CreateTopicParams const& p) {
        EXPECT_EQ(topic.FullName(), p.topic.name());
        EXPECT_EQ("test-kms-key-name", p.topic.kms_key_name());
        google::pubsub::v1::Topic response = p.topic;
        return make_status_or(response);
      });
  TopicAdminClient client(mock);
  auto const response = client.CreateTopic(
      TopicBuilder(topic).set_kms_key_name("test-kms-key-name"));
  EXPECT_STATUS_OK(response);
  EXPECT_EQ("test-kms-key-name", response->kms_key_name());
  EXPECT_EQ(topic.FullName(), response->name());
}

TEST(TopicAdminClient, GetTopic) {
  auto mock = std::make_shared<pubsub_mocks::MockTopicAdminConnection>();
  Topic const topic("test-project", "test-topic");
  EXPECT_CALL(*mock, GetTopic)
      .WillOnce([&](TopicAdminConnection::GetTopicParams const& p) {
        EXPECT_EQ(topic.FullName(), p.topic.FullName());
        google::pubsub::v1::Topic response;
        response.set_name(p.topic.FullName());
        response.set_kms_key_name("test-kms-key-name");
        return make_status_or(response);
      });
  TopicAdminClient client(mock);
  auto const response = client.GetTopic(topic);
  EXPECT_STATUS_OK(response);
  EXPECT_EQ("test-kms-key-name", response->kms_key_name());
  EXPECT_EQ(topic.FullName(), response->name());
}

TEST(TopicAdminClient, UpdateTopic) {
  auto mock = std::make_shared<pubsub_mocks::MockTopicAdminConnection>();
  Topic const topic("test-project", "test-topic");
  EXPECT_CALL(*mock, UpdateTopic)
      .WillOnce([&](TopicAdminConnection::UpdateTopicParams const& p) {
        EXPECT_EQ(topic.FullName(), p.request.topic().name());
        EXPECT_EQ("test-kms-key-name", p.request.topic().kms_key_name());
        EXPECT_THAT(p.request.update_mask().paths(),
                    ElementsAre("kms_key_name"));
        google::pubsub::v1::Topic response = p.request.topic();
        return make_status_or(response);
      });
  TopicAdminClient client(mock);
  auto const response = client.UpdateTopic(
      TopicBuilder(topic).set_kms_key_name("test-kms-key-name"));
  EXPECT_STATUS_OK(response);
  EXPECT_EQ("test-kms-key-name", response->kms_key_name());
  EXPECT_EQ(topic.FullName(), response->name());
}

TEST(TopicAdminClient, ListTopics) {
  auto mock = std::make_shared<pubsub_mocks::MockTopicAdminConnection>();
  auto const t1 = Topic("test-project", "t1");
  auto const t2 = Topic("test-project", "t2");
  EXPECT_CALL(*mock, ListTopics)
      .WillOnce([&](TopicAdminConnection::ListTopicsParams const& p) {
        EXPECT_EQ("projects/test-project", p.project_id);
        return internal::MakePaginationRange<pubsub::ListTopicsRange>(
            google::pubsub::v1::ListTopicsRequest{},
            [&](google::pubsub::v1::ListTopicsRequest const&) {
              google::pubsub::v1::ListTopicsResponse response;
              response.add_topics()->set_name(t1.FullName());
              response.add_topics()->set_name(t2.FullName());
              return make_status_or(response);
            },
            [](google::pubsub::v1::ListTopicsResponse const& r) {
              std::vector<google::pubsub::v1::Topic> items;
              for (auto const& t : r.topics()) items.push_back(t);
              return items;
            });
      });
  TopicAdminClient client(mock);
  std::vector<std::string> names;
  for (auto const& t : client.ListTopics("test-project")) {
    ASSERT_STATUS_OK(t);
    names.push_back(t->name());
  }
  EXPECT_THAT(names, ElementsAre(t1.FullName(), t2.FullName()));
}

TEST(TopicAdminClient, DeleteTopic) {
  auto mock = std::make_shared<pubsub_mocks::MockTopicAdminConnection>();
  Topic const topic("test-project", "test-topic");
  EXPECT_CALL(*mock, DeleteTopic)
      .WillOnce([&](TopicAdminConnection::DeleteTopicParams const& p) {
        EXPECT_EQ(topic.FullName(), p.topic.FullName());
        return Status{};
      });
  TopicAdminClient client(mock);
  auto const response = client.DeleteTopic(topic);
  EXPECT_STATUS_OK(response);
}

TEST(TopicAdminClient, DetachSubscription) {
  auto mock = std::make_shared<pubsub_mocks::MockTopicAdminConnection>();
  Subscription const subscription("test-project", "test-subscription");
  EXPECT_CALL(*mock, DetachSubscription)
      .WillOnce([&](TopicAdminConnection::DetachSubscriptionParams const& p) {
        EXPECT_EQ(subscription.FullName(), p.subscription.FullName());
        return make_status_or(google::pubsub::v1::DetachSubscriptionResponse{});
      });
  TopicAdminClient client(mock);
  auto const response = client.DetachSubscription(subscription);
  EXPECT_STATUS_OK(response);
}

TEST(TopicAdminClient, ListTopicSubscriptions) {
  auto mock = std::make_shared<pubsub_mocks::MockTopicAdminConnection>();
  auto const topic = Topic("test-project", "test-topic");
  auto const s1 = Subscription("test-project", "s1");
  auto const s2 = Subscription("test-project", "s2");
  EXPECT_CALL(*mock, ListTopicSubscriptions)
      .WillOnce([&](TopicAdminConnection::ListTopicSubscriptionsParams const&
                        p) {
        EXPECT_EQ(topic.FullName(), p.topic_full_name);
        return internal::MakePaginationRange<
            pubsub::ListTopicSubscriptionsRange>(
            google::pubsub::v1::ListTopicSubscriptionsRequest{},
            [&](google::pubsub::v1::ListTopicSubscriptionsRequest const&) {
              google::pubsub::v1::ListTopicSubscriptionsResponse response;
              response.add_subscriptions(s1.FullName());
              response.add_subscriptions(s2.FullName());
              return make_status_or(response);
            },
            [](google::pubsub::v1::ListTopicSubscriptionsResponse const& r) {
              std::vector<std::string> items;
              for (auto const& s : r.subscriptions()) items.push_back(s);
              return items;
            });
      });
  TopicAdminClient client(mock);
  std::vector<std::string> names;
  for (auto const& s : client.ListTopicSubscriptions(topic)) {
    ASSERT_STATUS_OK(s);
    names.push_back(*s);
  }
  EXPECT_THAT(names, ElementsAre(s1.FullName(), s2.FullName()));
}

TEST(TopicAdminClient, ListTopicSnapshots) {
  auto mock = std::make_shared<pubsub_mocks::MockTopicAdminConnection>();
  auto const topic = Topic("test-project", "test-topic");
  auto const s1 = Snapshot("test-project", "s1");
  auto const s2 = Snapshot("test-project", "s2");
  EXPECT_CALL(*mock, ListTopicSnapshots)
      .WillOnce([&](TopicAdminConnection::ListTopicSnapshotsParams const& p) {
        EXPECT_EQ(topic.FullName(), p.topic_full_name);
        return internal::MakePaginationRange<pubsub::ListTopicSnapshotsRange>(
            google::pubsub::v1::ListTopicSnapshotsRequest{},
            [&](google::pubsub::v1::ListTopicSnapshotsRequest const&) {
              google::pubsub::v1::ListTopicSnapshotsResponse response;
              response.add_snapshots(s1.FullName());
              response.add_snapshots(s2.FullName());
              return make_status_or(response);
            },
            [](google::pubsub::v1::ListTopicSnapshotsResponse const& r) {
              std::vector<std::string> items;
              for (auto const& s : r.snapshots()) items.push_back(s);
              return items;
            });
      });
  TopicAdminClient client(mock);
  std::vector<std::string> names;
  for (auto const& s : client.ListTopicSnapshots(topic)) {
    ASSERT_STATUS_OK(s);
    names.push_back(*s);
  }
  EXPECT_THAT(names, ElementsAre(s1.FullName(), s2.FullName()));
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
