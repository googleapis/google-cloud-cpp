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

#include "google/cloud/pubsub/internal/publisher_round_robin.h"
#include "google/cloud/pubsub/testing/mock_publisher_stub.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::testing::Return;

TEST(PublisherRoundRobinTest, CreateTopic) {
  std::vector<std::shared_ptr<PublisherStub>> mocks;
  for (int i = 0; i != 3; ++i) {
    auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
    EXPECT_CALL(*mock, CreateTopic)
        .WillOnce(Return(make_status_or(google::pubsub::v1::Topic{})));
    mocks.push_back(std::move(mock));
  }
  PublisherRoundRobin stub(mocks);
  for (int i = 0; i != 3; ++i) {
    grpc::ClientContext context;
    google::pubsub::v1::Topic topic;
    topic.set_name("test-topic-name");
    auto status = stub.CreateTopic(context, topic);
    EXPECT_STATUS_OK(status);
  }
}

TEST(PublisherRoundRobinTest, GetTopic) {
  std::vector<std::shared_ptr<PublisherStub>> mocks;
  for (int i = 0; i != 3; ++i) {
    auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
    EXPECT_CALL(*mock, GetTopic)
        .WillOnce(Return(make_status_or(google::pubsub::v1::Topic{})));
    mocks.push_back(std::move(mock));
  }
  PublisherRoundRobin stub(mocks);
  for (int i = 0; i != 3; ++i) {
    grpc::ClientContext context;
    google::pubsub::v1::GetTopicRequest request;
    request.set_topic("test-topic-name");
    auto status = stub.GetTopic(context, request);
    EXPECT_STATUS_OK(status);
  }
}

TEST(PublisherRoundRobinTest, UpdateTopic) {
  std::vector<std::shared_ptr<PublisherStub>> mocks;
  for (int i = 0; i != 3; ++i) {
    auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
    EXPECT_CALL(*mock, UpdateTopic)
        .WillOnce(Return(make_status_or(google::pubsub::v1::Topic{})));
    mocks.push_back(std::move(mock));
  }
  PublisherRoundRobin stub(mocks);
  for (int i = 0; i != 3; ++i) {
    grpc::ClientContext context;
    google::pubsub::v1::UpdateTopicRequest request;
    request.mutable_topic()->set_name("test-topic-name");
    auto status = stub.UpdateTopic(context, request);
    EXPECT_STATUS_OK(status);
  }
}

TEST(PublisherRoundRobinTest, ListTopics) {
  std::vector<std::shared_ptr<PublisherStub>> mocks;
  for (int i = 0; i != 3; ++i) {
    auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
    EXPECT_CALL(*mock, ListTopics)
        .WillOnce(
            Return(make_status_or(google::pubsub::v1::ListTopicsResponse{})));
    mocks.push_back(std::move(mock));
  }
  PublisherRoundRobin stub(mocks);
  for (int i = 0; i != 3; ++i) {
    grpc::ClientContext context;
    google::pubsub::v1::ListTopicsRequest request;
    request.set_project("test-project-name");
    auto status = stub.ListTopics(context, request);
    EXPECT_STATUS_OK(status);
  }
}

TEST(PublisherRoundRobinTest, DeleteTopic) {
  std::vector<std::shared_ptr<PublisherStub>> mocks;
  for (int i = 0; i != 3; ++i) {
    auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
    EXPECT_CALL(*mock, DeleteTopic).WillOnce(Return(Status{}));
    mocks.push_back(std::move(mock));
  }
  PublisherRoundRobin stub(mocks);
  for (int i = 0; i != 3; ++i) {
    grpc::ClientContext context;
    google::pubsub::v1::DeleteTopicRequest request;
    request.set_topic("test-topic-name");
    auto status = stub.DeleteTopic(context, request);
    EXPECT_STATUS_OK(status);
  }
}

TEST(PublisherRoundRobinTest, DetachSubscription) {
  std::vector<std::shared_ptr<PublisherStub>> mocks;
  for (int i = 0; i != 3; ++i) {
    auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
    EXPECT_CALL(*mock, DetachSubscription)
        .WillOnce(Return(
            make_status_or(google::pubsub::v1::DetachSubscriptionResponse{})));
    mocks.push_back(std::move(mock));
  }
  PublisherRoundRobin stub(mocks);
  for (int i = 0; i != 3; ++i) {
    grpc::ClientContext context;
    google::pubsub::v1::DetachSubscriptionRequest request;
    request.set_subscription("test-subscription-name");
    auto status = stub.DetachSubscription(context, request);
    EXPECT_STATUS_OK(status);
  }
}

TEST(PublisherRoundRobinTest, ListTopicSubscriptions) {
  std::vector<std::shared_ptr<PublisherStub>> mocks;
  for (int i = 0; i != 3; ++i) {
    auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
    EXPECT_CALL(*mock, ListTopicSubscriptions)
        .WillOnce(Return(make_status_or(
            google::pubsub::v1::ListTopicSubscriptionsResponse{})));
    mocks.push_back(std::move(mock));
  }
  PublisherRoundRobin stub(mocks);
  for (int i = 0; i != 3; ++i) {
    grpc::ClientContext context;
    google::pubsub::v1::ListTopicSubscriptionsRequest request;
    request.set_topic("test-topic-name");
    auto status = stub.ListTopicSubscriptions(context, request);
    EXPECT_STATUS_OK(status);
  }
}

TEST(PublisherRoundRobinTest, ListTopicSnapshots) {
  std::vector<std::shared_ptr<PublisherStub>> mocks;
  for (int i = 0; i != 3; ++i) {
    auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
    EXPECT_CALL(*mock, ListTopicSnapshots)
        .WillOnce(Return(
            make_status_or(google::pubsub::v1::ListTopicSnapshotsResponse{})));
    mocks.push_back(std::move(mock));
  }
  PublisherRoundRobin stub(mocks);
  for (int i = 0; i != 3; ++i) {
    grpc::ClientContext context;
    google::pubsub::v1::ListTopicSnapshotsRequest request;
    request.set_topic("test-topic-name");
    auto status = stub.ListTopicSnapshots(context, request);
    EXPECT_STATUS_OK(status);
  }
}

TEST(PublisherRoundRobinTest, AsyncPublish) {
  std::vector<std::shared_ptr<PublisherStub>> mocks;
  for (int i = 0; i != 3; ++i) {
    auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
    EXPECT_CALL(*mock, AsyncPublish)
        .WillOnce([](google::cloud::CompletionQueue&,
                     std::unique_ptr<grpc::ClientContext>,
                     google::pubsub::v1::PublishRequest const&) {
          return make_ready_future(
              make_status_or(google::pubsub::v1::PublishResponse{}));
        });
    mocks.push_back(std::move(mock));
  }
  PublisherRoundRobin stub(mocks);
  for (int i = 0; i != 3; ++i) {
    google::cloud::CompletionQueue cq;
    google::pubsub::v1::PublishRequest request;
    request.set_topic("test-topic-name");
    auto status =
        stub.AsyncPublish(cq, absl::make_unique<grpc::ClientContext>(), request)
            .get();
    EXPECT_STATUS_OK(status);
  }
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
