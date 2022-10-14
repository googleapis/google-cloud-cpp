// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/pubsub/internal/publisher_auth.h"
#include "google/cloud/pubsub/testing/mock_publisher_stub.h"
#include "google/cloud/testing_util/mock_grpc_authentication_strategy.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::pubsub_testing::MockPublisherStub;
using ::google::cloud::testing_util::MakeTypicalAsyncMockAuth;
using ::google::cloud::testing_util::MakeTypicalMockAuth;
using ::google::cloud::testing_util::StatusIs;
using ::testing::IsNull;
using ::testing::Not;
using ::testing::Return;

TEST(PublisherAuthTest, CreateTopic) {
  auto mock = std::make_shared<MockPublisherStub>();
  EXPECT_CALL(*mock, CreateTopic)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = PublisherAuth(MakeTypicalMockAuth(), mock);
  google::pubsub::v1::Topic request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.CreateTopic(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.CreateTopic(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(PublisherAuthTest, GetTopic) {
  auto mock = std::make_shared<MockPublisherStub>();
  EXPECT_CALL(*mock, GetTopic)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = PublisherAuth(MakeTypicalMockAuth(), mock);
  google::pubsub::v1::GetTopicRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.GetTopic(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.GetTopic(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(PublisherAuthTest, UpdateTopic) {
  auto mock = std::make_shared<MockPublisherStub>();
  EXPECT_CALL(*mock, UpdateTopic)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = PublisherAuth(MakeTypicalMockAuth(), mock);
  google::pubsub::v1::UpdateTopicRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.UpdateTopic(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.UpdateTopic(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(PublisherAuthTest, ListTopics) {
  auto mock = std::make_shared<MockPublisherStub>();
  EXPECT_CALL(*mock, ListTopics)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = PublisherAuth(MakeTypicalMockAuth(), mock);
  google::pubsub::v1::ListTopicsRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.ListTopics(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.ListTopics(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(PublisherAuthTest, DeleteTopic) {
  auto mock = std::make_shared<MockPublisherStub>();
  EXPECT_CALL(*mock, DeleteTopic)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = PublisherAuth(MakeTypicalMockAuth(), mock);
  google::pubsub::v1::DeleteTopicRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.DeleteTopic(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.DeleteTopic(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(PublisherAuthTest, DetachSubscription) {
  auto mock = std::make_shared<MockPublisherStub>();
  EXPECT_CALL(*mock, DetachSubscription)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = PublisherAuth(MakeTypicalMockAuth(), mock);
  google::pubsub::v1::DetachSubscriptionRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.DetachSubscription(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.DetachSubscription(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(PublisherAuthTest, ListTopicSubscriptions) {
  auto mock = std::make_shared<MockPublisherStub>();
  EXPECT_CALL(*mock, ListTopicSubscriptions)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = PublisherAuth(MakeTypicalMockAuth(), mock);
  google::pubsub::v1::ListTopicSubscriptionsRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.ListTopicSubscriptions(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.ListTopicSubscriptions(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(PublisherAuthTest, ListTopicSnapshots) {
  auto mock = std::make_shared<MockPublisherStub>();
  EXPECT_CALL(*mock, ListTopicSnapshots)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = PublisherAuth(MakeTypicalMockAuth(), mock);
  google::pubsub::v1::ListTopicSnapshotsRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.ListTopicSnapshots(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.ListTopicSnapshots(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(PublisherAuthTest, AsyncPublish) {
  auto mock = std::make_shared<MockPublisherStub>();
  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce([](::testing::Unused, ::testing::Unused, ::testing::Unused) {
        return make_ready_future(StatusOr<google::pubsub::v1::PublishResponse>(
            Status(StatusCode::kPermissionDenied, "uh-oh")));
      });

  auto under_test = PublisherAuth(MakeTypicalAsyncMockAuth(), mock);
  google::pubsub::v1::PublishRequest request;
  CompletionQueue cq;
  auto auth_failure = under_test.AsyncPublish(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_THAT(auth_failure.get(), StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.AsyncPublish(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_THAT(auth_success.get(), StatusIs(StatusCode::kPermissionDenied));
}

TEST(PublisherAuthTest, Publish) {
  auto mock = std::make_shared<MockPublisherStub>();
  EXPECT_CALL(*mock, Publish)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = PublisherAuth(MakeTypicalMockAuth(), mock);
  google::pubsub::v1::PublishRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.Publish(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.Publish(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
