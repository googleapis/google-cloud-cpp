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

#include "google/cloud/pubsub/internal/publisher_metadata.h"
#include "google/cloud/pubsub/subscription.h"
#include "google/cloud/pubsub/testing/mock_publisher_stub.h"
#include "google/cloud/pubsub/topic.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::google::cloud::testing_util::IsContextMDValid;

TEST(PublisherMetadataTest, CreateTopic) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  EXPECT_CALL(*mock, CreateTopic)
      .WillOnce(
          [](grpc::ClientContext& context, google::pubsub::v1::Topic const&) {
            EXPECT_STATUS_OK(IsContextMDValid(
                context, "google.pubsub.v1.Publisher.CreateTopic",
                google::cloud::internal::ApiClientHeader()));
            return make_status_or(google::pubsub::v1::Topic{});
          });

  PublisherMetadata stub(mock);
  grpc::ClientContext context;
  google::pubsub::v1::Topic topic;
  topic.set_name(pubsub::Topic("test-project", "test-topic").FullName());
  auto status = stub.CreateTopic(context, topic);
  EXPECT_STATUS_OK(status);
}

TEST(PublisherMetadataTest, GetTopic) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  EXPECT_CALL(*mock, GetTopic)
      .WillOnce([](grpc::ClientContext& context,
                   google::pubsub::v1::GetTopicRequest const&) {
        EXPECT_STATUS_OK(
            IsContextMDValid(context, "google.pubsub.v1.Publisher.GetTopic",
                             google::cloud::internal::ApiClientHeader()));
        return make_status_or(google::pubsub::v1::Topic{});
      });
  PublisherMetadata stub(mock);
  grpc::ClientContext context;
  google::pubsub::v1::GetTopicRequest request;
  request.set_topic(pubsub::Topic("test-project", "test-topic").FullName());
  auto status = stub.GetTopic(context, request);
  EXPECT_STATUS_OK(status);
}

TEST(PublisherMetadataTest, UpdateTopic) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  EXPECT_CALL(*mock, UpdateTopic)
      .WillOnce([](grpc::ClientContext& context,
                   google::pubsub::v1::UpdateTopicRequest const&) {
        EXPECT_STATUS_OK(
            IsContextMDValid(context, "google.pubsub.v1.Publisher.UpdateTopic",
                             google::cloud::internal::ApiClientHeader()));
        return make_status_or(google::pubsub::v1::Topic{});
      });
  PublisherMetadata stub(mock);
  grpc::ClientContext context;
  google::pubsub::v1::UpdateTopicRequest request;
  request.mutable_topic()->set_name(
      pubsub::Topic("test-project", "test-topic").FullName());
  auto status = stub.UpdateTopic(context, request);
  EXPECT_STATUS_OK(status);
}

TEST(PublisherMetadataTest, ListTopics) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  EXPECT_CALL(*mock, ListTopics)
      .WillOnce([](grpc::ClientContext& context,
                   google::pubsub::v1::ListTopicsRequest const&) {
        EXPECT_STATUS_OK(
            IsContextMDValid(context, "google.pubsub.v1.Publisher.ListTopics",
                             google::cloud::internal::ApiClientHeader()));
        return make_status_or(google::pubsub::v1::ListTopicsResponse{});
      });
  PublisherMetadata stub(mock);
  grpc::ClientContext context;
  google::pubsub::v1::ListTopicsRequest request;
  request.set_project("projects/test-project");
  auto status = stub.ListTopics(context, request);
  EXPECT_STATUS_OK(status);
}

TEST(PublisherMetadataTest, DeleteTopic) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  EXPECT_CALL(*mock, DeleteTopic)
      .WillOnce([](grpc::ClientContext& context,
                   google::pubsub::v1::DeleteTopicRequest const&) {
        EXPECT_STATUS_OK(
            IsContextMDValid(context, "google.pubsub.v1.Publisher.DeleteTopic",
                             google::cloud::internal::ApiClientHeader()));
        return Status{};
      });
  PublisherMetadata stub(mock);
  grpc::ClientContext context;
  google::pubsub::v1::DeleteTopicRequest request;
  request.set_topic(pubsub::Topic("test-project", "test-topic").FullName());
  auto status = stub.DeleteTopic(context, request);
  EXPECT_STATUS_OK(status);
}

TEST(PublisherMetadataTest, DetachSubscription) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  EXPECT_CALL(*mock, DetachSubscription)
      .WillOnce([](grpc::ClientContext& context,
                   google::pubsub::v1::DetachSubscriptionRequest const&) {
        EXPECT_STATUS_OK(IsContextMDValid(
            context, "google.pubsub.v1.Publisher.DetachSubscription",
            google::cloud::internal::ApiClientHeader()));
        return make_status_or(google::pubsub::v1::DetachSubscriptionResponse{});
      });
  PublisherMetadata stub(mock);
  grpc::ClientContext context;
  google::pubsub::v1::DetachSubscriptionRequest request;
  request.set_subscription(
      pubsub::Subscription("test-project", "test-subscription").FullName());
  auto status = stub.DetachSubscription(context, request);
  EXPECT_STATUS_OK(status);
}

TEST(PublisherMetadataTest, ListTopicSubscriptions) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  EXPECT_CALL(*mock, ListTopicSubscriptions)
      .WillOnce([](grpc::ClientContext& context,
                   google::pubsub::v1::ListTopicSubscriptionsRequest const&) {
        EXPECT_STATUS_OK(IsContextMDValid(
            context, "google.pubsub.v1.Publisher.ListTopicSubscriptions",
            google::cloud::internal::ApiClientHeader()));
        return make_status_or(
            google::pubsub::v1::ListTopicSubscriptionsResponse{});
      });
  PublisherMetadata stub(mock);
  grpc::ClientContext context;
  google::pubsub::v1::ListTopicSubscriptionsRequest request;
  request.set_topic(pubsub::Topic("test-project", "test-topic").FullName());
  auto status = stub.ListTopicSubscriptions(context, request);
  EXPECT_STATUS_OK(status);
}

TEST(PublisherMetadataTest, ListTopicSnapshots) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  EXPECT_CALL(*mock, ListTopicSnapshots)
      .WillOnce([](grpc::ClientContext& context,
                   google::pubsub::v1::ListTopicSnapshotsRequest const&) {
        EXPECT_STATUS_OK(IsContextMDValid(
            context, "google.pubsub.v1.Publisher.ListTopicSnapshots",
            google::cloud::internal::ApiClientHeader()));
        return make_status_or(google::pubsub::v1::ListTopicSnapshotsResponse{});
      });
  PublisherMetadata stub(mock);
  grpc::ClientContext context;
  google::pubsub::v1::ListTopicSnapshotsRequest request;
  request.set_topic(pubsub::Topic("test-project", "test-topic").FullName());
  auto status = stub.ListTopicSnapshots(context, request);
  EXPECT_STATUS_OK(status);
}

TEST(PublisherMetadataTest, AsyncPublish) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce([](google::cloud::CompletionQueue&,
                   std::unique_ptr<grpc::ClientContext> context,
                   google::pubsub::v1::PublishRequest const&) {
        EXPECT_STATUS_OK(
            IsContextMDValid(*context, "google.pubsub.v1.Publisher.Publish",
                             google::cloud::internal::ApiClientHeader()));
        return make_ready_future(
            make_status_or(google::pubsub::v1::PublishResponse{}));
      });
  PublisherMetadata stub(mock);
  google::cloud::CompletionQueue cq;
  google::pubsub::v1::PublishRequest request;
  request.set_topic(pubsub::Topic("test-project", "test-topic").FullName());
  auto status =
      stub.AsyncPublish(cq, absl::make_unique<grpc::ClientContext>(), request)
          .get();
  EXPECT_STATUS_OK(status);
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
