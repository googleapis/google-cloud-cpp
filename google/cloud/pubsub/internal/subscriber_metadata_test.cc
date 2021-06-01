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

#include "google/cloud/pubsub/internal/subscriber_metadata.h"
#include "google/cloud/pubsub/snapshot.h"
#include "google/cloud/pubsub/subscription.h"
#include "google/cloud/pubsub/testing/mock_subscriber_stub.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::google::cloud::testing_util::IsContextMDValid;

TEST(SubscriberMetadataTest, CreateSubscription) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, CreateSubscription)
      .WillOnce([](grpc::ClientContext& context,
                   google::pubsub::v1::Subscription const&) {
        EXPECT_STATUS_OK(IsContextMDValid(
            context, "google.pubsub.v1.Subscriber.CreateSubscription",
            google::cloud::internal::ApiClientHeader()));
        return make_status_or(google::pubsub::v1::Subscription{});
      });
  SubscriberMetadata stub(mock);
  grpc::ClientContext context;
  google::pubsub::v1::Subscription subscription;
  subscription.set_name(
      pubsub::Subscription("test-project", "test-subscription").FullName());
  auto status = stub.CreateSubscription(context, subscription);
  EXPECT_STATUS_OK(status);
}

TEST(SubscriberMetadataTest, GetSubscription) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, GetSubscription)
      .WillOnce([](grpc::ClientContext& context,
                   google::pubsub::v1::GetSubscriptionRequest const&) {
        EXPECT_STATUS_OK(IsContextMDValid(
            context, "google.pubsub.v1.Subscriber.GetSubscription",
            google::cloud::internal::ApiClientHeader()));
        return make_status_or(google::pubsub::v1::Subscription{});
      });
  SubscriberMetadata stub(mock);
  grpc::ClientContext context;
  google::pubsub::v1::GetSubscriptionRequest request;
  request.set_subscription(
      pubsub::Subscription("test-project", "test-subscription").FullName());
  auto status = stub.GetSubscription(context, request);
  EXPECT_STATUS_OK(status);
}

TEST(SubscriberMetadataTest, UpdateSubscription) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, UpdateSubscription)
      .WillOnce([](grpc::ClientContext& context,
                   google::pubsub::v1::UpdateSubscriptionRequest const&) {
        EXPECT_STATUS_OK(IsContextMDValid(
            context, "google.pubsub.v1.Subscriber.UpdateSubscription",
            google::cloud::internal::ApiClientHeader()));
        return make_status_or(google::pubsub::v1::Subscription{});
      });
  SubscriberMetadata stub(mock);
  grpc::ClientContext context;
  google::pubsub::v1::UpdateSubscriptionRequest request;
  request.mutable_subscription()->set_name(
      pubsub::Subscription("test-project", "test-subscription").FullName());
  auto status = stub.UpdateSubscription(context, request);
  EXPECT_STATUS_OK(status);
}

TEST(SubscriberMetadataTest, ListSubscriptions) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, ListSubscriptions)
      .WillOnce([](grpc::ClientContext& context,
                   google::pubsub::v1::ListSubscriptionsRequest const&) {
        EXPECT_STATUS_OK(IsContextMDValid(
            context, "google.pubsub.v1.Subscriber.ListSubscriptions",
            google::cloud::internal::ApiClientHeader()));
        return make_status_or(google::pubsub::v1::ListSubscriptionsResponse{});
      });
  SubscriberMetadata stub(mock);
  grpc::ClientContext context;
  google::pubsub::v1::ListSubscriptionsRequest request;
  request.set_project("projects/test-project");
  auto status = stub.ListSubscriptions(context, request);
  EXPECT_STATUS_OK(status);
}

TEST(SubscriberMetadataTest, DeleteSubscription) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, DeleteSubscription)
      .WillOnce([](grpc::ClientContext& context,
                   google::pubsub::v1::DeleteSubscriptionRequest const&) {
        EXPECT_STATUS_OK(IsContextMDValid(
            context, "google.pubsub.v1.Subscriber.DeleteSubscription",
            google::cloud::internal::ApiClientHeader()));
        return Status{};
      });
  SubscriberMetadata stub(mock);
  grpc::ClientContext context;
  google::pubsub::v1::DeleteSubscriptionRequest request;
  request.set_subscription(
      pubsub::Subscription("test-project", "test-subscription").FullName());
  auto status = stub.DeleteSubscription(context, request);
  EXPECT_STATUS_OK(status);
}

TEST(SubscriberMetadataTest, ModifyPushConfig) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, ModifyPushConfig)
      .WillOnce([](grpc::ClientContext& context,
                   google::pubsub::v1::ModifyPushConfigRequest const&) {
        EXPECT_STATUS_OK(IsContextMDValid(
            context, "google.pubsub.v1.Subscriber.ModifyPushConfig",
            google::cloud::internal::ApiClientHeader()));
        return Status{};
      });
  SubscriberMetadata stub(mock);
  grpc::ClientContext context;
  google::pubsub::v1::ModifyPushConfigRequest request;
  request.set_subscription(
      pubsub::Subscription("test-project", "test-subscription").FullName());
  auto status = stub.ModifyPushConfig(context, request);
  EXPECT_STATUS_OK(status);
}

TEST(SubscriberMetadataTest, AsyncStreamingPull) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, AsyncStreamingPull)
      .WillOnce([](google::cloud::CompletionQueue&,
                   std::unique_ptr<grpc::ClientContext> context,
                   google::pubsub::v1::StreamingPullRequest const&) {
        EXPECT_STATUS_OK(IsContextMDValid(
            *context, "google.pubsub.v1.Subscriber.StreamingPull",
            google::cloud::internal::ApiClientHeader()));
        return absl::make_unique<pubsub_testing::MockAsyncPullStream>();
      });
  SubscriberMetadata stub(mock);
  google::cloud::CompletionQueue cq;
  google::pubsub::v1::StreamingPullRequest request;
  request.set_subscription(
      pubsub::Subscription("test-project", "test-subscription").FullName());
  auto stream = stub.AsyncStreamingPull(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_TRUE(stream);
}

TEST(SubscriberMetadataTest, AsyncAcknowledge) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, AsyncAcknowledge)
      .WillOnce([](google::cloud::CompletionQueue&,
                   std::unique_ptr<grpc::ClientContext> context,
                   google::pubsub::v1::AcknowledgeRequest const&) {
        EXPECT_STATUS_OK(IsContextMDValid(
            *context, "google.pubsub.v1.Subscriber.Acknowledge",
            google::cloud::internal::ApiClientHeader()));
        return make_ready_future(Status{});
      });
  SubscriberMetadata stub(mock);
  google::cloud::CompletionQueue cq;
  google::pubsub::v1::AcknowledgeRequest request;
  request.set_subscription(
      pubsub::Subscription("test-project", "test-subscription").FullName());
  auto response = stub.AsyncAcknowledge(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_STATUS_OK(response.get());
}

TEST(SubscriberMetadataTest, AsyncModifyAckDeadline) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, AsyncModifyAckDeadline)
      .WillOnce([](google::cloud::CompletionQueue&,
                   std::unique_ptr<grpc::ClientContext> context,
                   google::pubsub::v1::ModifyAckDeadlineRequest const&) {
        EXPECT_STATUS_OK(IsContextMDValid(
            *context, "google.pubsub.v1.Subscriber.ModifyAckDeadline",
            google::cloud::internal::ApiClientHeader()));
        return make_ready_future(Status{});
      });
  SubscriberMetadata stub(mock);
  google::cloud::CompletionQueue cq;
  google::pubsub::v1::ModifyAckDeadlineRequest request;
  request.set_subscription(
      pubsub::Subscription("test-project", "test-subscription").FullName());
  auto response = stub.AsyncModifyAckDeadline(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_STATUS_OK(response.get());
}

TEST(SubscriberMetadataTest, CreateSnapshot) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, CreateSnapshot)
      .WillOnce([](grpc::ClientContext& context,
                   google::pubsub::v1::CreateSnapshotRequest const&) {
        EXPECT_STATUS_OK(IsContextMDValid(
            context, "google.pubsub.v1.Subscriber.CreateSnapshot",
            google::cloud::internal::ApiClientHeader()));
        return make_status_or(google::pubsub::v1::Snapshot{});
      });
  SubscriberMetadata stub(mock);
  grpc::ClientContext context;
  google::pubsub::v1::CreateSnapshotRequest request;
  request.set_name(
      pubsub::Snapshot("test-project", "test-snapshot").FullName());
  auto status = stub.CreateSnapshot(context, request);
  EXPECT_STATUS_OK(status);
}

TEST(SubscriberMetadataTest, GetSnapshot) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, GetSnapshot)
      .WillOnce([](grpc::ClientContext& context,
                   google::pubsub::v1::GetSnapshotRequest const&) {
        EXPECT_STATUS_OK(
            IsContextMDValid(context, "google.pubsub.v1.Subscriber.GetSnapshot",
                             google::cloud::internal::ApiClientHeader()));
        return make_status_or(google::pubsub::v1::Snapshot{});
      });
  SubscriberMetadata stub(mock);
  grpc::ClientContext context;
  google::pubsub::v1::GetSnapshotRequest request;
  request.set_snapshot(
      pubsub::Snapshot("test-project", "test-snapshot").FullName());
  auto status = stub.GetSnapshot(context, request);
  EXPECT_STATUS_OK(status);
}

TEST(SubscriberMetadataTest, ListSnapshots) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, ListSnapshots)
      .WillOnce([](grpc::ClientContext& context,
                   google::pubsub::v1::ListSnapshotsRequest const&) {
        EXPECT_STATUS_OK(IsContextMDValid(
            context, "google.pubsub.v1.Subscriber.ListSnapshots",
            google::cloud::internal::ApiClientHeader()));
        return make_status_or(google::pubsub::v1::ListSnapshotsResponse{});
      });
  SubscriberMetadata stub(mock);
  grpc::ClientContext context;
  google::pubsub::v1::ListSnapshotsRequest request;
  request.set_project("projects/test-project");
  auto status = stub.ListSnapshots(context, request);
  EXPECT_STATUS_OK(status);
}

TEST(SubscriberMetadataTest, UpdateSnapshot) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, UpdateSnapshot)
      .WillOnce([](grpc::ClientContext& context,
                   google::pubsub::v1::UpdateSnapshotRequest const&) {
        EXPECT_STATUS_OK(IsContextMDValid(
            context, "google.pubsub.v1.Subscriber.UpdateSnapshot",
            google::cloud::internal::ApiClientHeader()));
        return make_status_or(google::pubsub::v1::Snapshot{});
      });
  SubscriberMetadata stub(mock);
  grpc::ClientContext context;
  google::pubsub::v1::UpdateSnapshotRequest request;
  request.mutable_snapshot()->set_name(
      pubsub::Snapshot("test-project", "test-snapshot").FullName());
  auto status = stub.UpdateSnapshot(context, request);
  EXPECT_STATUS_OK(status);
}

TEST(SubscriberMetadataTest, DeleteSnapshot) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, DeleteSnapshot)
      .WillOnce([](grpc::ClientContext& context,
                   google::pubsub::v1::DeleteSnapshotRequest const&) {
        EXPECT_STATUS_OK(IsContextMDValid(
            context, "google.pubsub.v1.Subscriber.DeleteSnapshot",
            google::cloud::internal::ApiClientHeader()));
        return Status{};
      });
  SubscriberMetadata stub(mock);
  grpc::ClientContext context;
  google::pubsub::v1::DeleteSnapshotRequest request;
  request.set_snapshot(
      pubsub::Snapshot("test-project", "test-snapshot").FullName());
  auto status = stub.DeleteSnapshot(context, request);
  EXPECT_STATUS_OK(status);
}

TEST(SubscriberMetadataTest, Seek) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, Seek)
      .WillOnce([](grpc::ClientContext& context,
                   google::pubsub::v1::SeekRequest const&) {
        EXPECT_STATUS_OK(
            IsContextMDValid(context, "google.pubsub.v1.Subscriber.Seek",
                             google::cloud::internal::ApiClientHeader()));
        return make_status_or(google::pubsub::v1::SeekResponse{});
      });
  SubscriberMetadata stub(mock);
  grpc::ClientContext context;
  google::pubsub::v1::SeekRequest request;
  request.set_subscription(
      pubsub::Subscription("test-project", "test-subscription").FullName());
  auto status = stub.Seek(context, request);
  EXPECT_STATUS_OK(status);
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
