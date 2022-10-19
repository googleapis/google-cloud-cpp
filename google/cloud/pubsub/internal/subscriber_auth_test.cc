// Copyright 2021 Google LLC
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

#include "google/cloud/pubsub/internal/subscriber_auth_decorator.h"
#include "google/cloud/pubsub/testing/mock_subscriber_stub.h"
#include "google/cloud/internal/async_read_write_stream_impl.h"
#include "google/cloud/testing_util/mock_grpc_authentication_strategy.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::MakeTypicalAsyncMockAuth;
using ::google::cloud::testing_util::MakeTypicalMockAuth;
using ::google::cloud::testing_util::StatusIs;
using ::testing::IsNull;
using ::testing::Not;
using ::testing::Return;
using ::testing::StrictMock;

TEST(SubscriberAuthTest, CreateSubscription) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, CreateSubscription)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto under_test = SubscriberAuth(MakeTypicalMockAuth(), mock);
  grpc::ClientContext ctx;
  google::pubsub::v1::Subscription request;
  auto auth_failure = under_test.CreateSubscription(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.CreateSubscription(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(SubscriberAuthTest, GetSubscription) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, GetSubscription)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto under_test = SubscriberAuth(MakeTypicalMockAuth(), mock);
  grpc::ClientContext ctx;
  google::pubsub::v1::GetSubscriptionRequest request;
  auto auth_failure = under_test.GetSubscription(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.GetSubscription(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(SubscriberAuthTest, UpdateSubscription) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, UpdateSubscription)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto under_test = SubscriberAuth(MakeTypicalMockAuth(), mock);
  grpc::ClientContext ctx;
  google::pubsub::v1::UpdateSubscriptionRequest request;
  auto auth_failure = under_test.UpdateSubscription(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.UpdateSubscription(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(SubscriberAuthTest, ListSubscriptions) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, ListSubscriptions)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto under_test = SubscriberAuth(MakeTypicalMockAuth(), mock);
  grpc::ClientContext ctx;
  google::pubsub::v1::ListSubscriptionsRequest request;
  auto auth_failure = under_test.ListSubscriptions(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.ListSubscriptions(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(SubscriberAuthTest, DeleteSubscription) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, DeleteSubscription)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto under_test = SubscriberAuth(MakeTypicalMockAuth(), mock);
  grpc::ClientContext ctx;
  google::pubsub::v1::DeleteSubscriptionRequest request;
  auto auth_failure = under_test.DeleteSubscription(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.DeleteSubscription(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(SubscriberAuthTest, ModifyPushConfig) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, ModifyPushConfig)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto under_test = SubscriberAuth(MakeTypicalMockAuth(), mock);
  grpc::ClientContext ctx;
  google::pubsub::v1::ModifyPushConfigRequest request;
  auto auth_failure = under_test.ModifyPushConfig(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.ModifyPushConfig(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(SubscriberAuthTest, AsyncStreamingPullFailedAuth) {
  auto mock =
      std::make_shared<StrictMock<pubsub_testing::MockSubscriberStub>>();
  auto auth = std::make_shared<testing_util::MockAuthenticationStrategy>();
  EXPECT_CALL(*auth, AsyncConfigureContext)
      .WillOnce([](std::unique_ptr<grpc::ClientContext>)
                    -> future<StatusOr<std::unique_ptr<grpc::ClientContext>>> {
        return make_ready_future(StatusOr<std::unique_ptr<grpc::ClientContext>>(
            Status(StatusCode::kInvalidArgument, "cannot-set-credentials")));
      });
  auto under_test = SubscriberAuth(auth, mock);
  google::cloud::CompletionQueue cq;
  auto auth_failure = under_test.AsyncStreamingPull(
      cq, absl::make_unique<grpc::ClientContext>());
  ASSERT_FALSE(auth_failure->Start().get());
  EXPECT_THAT(auth_failure->Finish().get(),
              StatusIs(StatusCode::kInvalidArgument));
}

TEST(SubscriberAuthTest, AsyncStreamingPullAuthSuccess) {
  using ErrorStream =
      ::google::cloud::internal::AsyncStreamingReadWriteRpcError<
          google::pubsub::v1::StreamingPullRequest,
          google::pubsub::v1::StreamingPullResponse>;

  auto mock =
      std::make_shared<StrictMock<pubsub_testing::MockSubscriberStub>>();
  EXPECT_CALL(*mock, AsyncStreamingPull)
      .WillOnce([](::testing::Unused, ::testing::Unused) {
        return absl::make_unique<ErrorStream>(
            Status(StatusCode::kPermissionDenied, "uh-oh"));
      });
  auto auth = std::make_shared<testing_util::MockAuthenticationStrategy>();
  EXPECT_CALL(*auth, AsyncConfigureContext)
      .WillOnce([](std::unique_ptr<grpc::ClientContext> context) {
        context->set_credentials(
            grpc::AccessTokenCredentials("test-only-invalid"));
        return make_ready_future(make_status_or(std::move(context)));
      });
  auto under_test = SubscriberAuth(auth, mock);
  google::cloud::CompletionQueue cq;
  auto auth_failure = under_test.AsyncStreamingPull(
      cq, absl::make_unique<grpc::ClientContext>());
  ASSERT_FALSE(auth_failure->Start().get());
  EXPECT_THAT(auth_failure->Finish().get(),
              StatusIs(StatusCode::kPermissionDenied));
}

TEST(SubscriberAuthTest, AsyncAcknowledge) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, AsyncAcknowledge)
      .WillOnce([](::testing::Unused, ::testing::Unused, ::testing::Unused) {
        return make_ready_future(
            Status(StatusCode::kPermissionDenied, "uh-oh"));
      });
  auto under_test = SubscriberAuth(MakeTypicalAsyncMockAuth(), mock);
  google::cloud::CompletionQueue cq;
  google::pubsub::v1::AcknowledgeRequest request;
  auto auth_failure = under_test.AsyncAcknowledge(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_THAT(auth_failure.get(), StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.AsyncAcknowledge(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_THAT(auth_success.get(), StatusIs(StatusCode::kPermissionDenied));
}

TEST(SubscriberAuthTest, AsyncModifyAckDeadline) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, AsyncModifyAckDeadline)
      .WillOnce([](::testing::Unused, ::testing::Unused, ::testing::Unused) {
        return make_ready_future(
            Status(StatusCode::kPermissionDenied, "uh-oh"));
      });
  auto under_test = SubscriberAuth(MakeTypicalAsyncMockAuth(), mock);
  google::cloud::CompletionQueue cq;
  google::pubsub::v1::ModifyAckDeadlineRequest request;
  auto auth_failure = under_test.AsyncModifyAckDeadline(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_THAT(auth_failure.get(), StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.AsyncModifyAckDeadline(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_THAT(auth_success.get(), StatusIs(StatusCode::kPermissionDenied));
}

TEST(SubscriberAuthTest, CreateSnapshot) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, CreateSnapshot)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto under_test = SubscriberAuth(MakeTypicalMockAuth(), mock);
  grpc::ClientContext ctx;
  google::pubsub::v1::CreateSnapshotRequest request;
  auto auth_failure = under_test.CreateSnapshot(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.CreateSnapshot(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(SubscriberAuthTest, GetSnapshot) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, GetSnapshot)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto under_test = SubscriberAuth(MakeTypicalMockAuth(), mock);
  grpc::ClientContext ctx;
  google::pubsub::v1::GetSnapshotRequest request;
  auto auth_failure = under_test.GetSnapshot(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.GetSnapshot(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(SubscriberAuthTest, ListSnapshots) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, ListSnapshots)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto under_test = SubscriberAuth(MakeTypicalMockAuth(), mock);
  grpc::ClientContext ctx;
  google::pubsub::v1::ListSnapshotsRequest request;
  auto auth_failure = under_test.ListSnapshots(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.ListSnapshots(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(SubscriberAuthTest, UpdateSnapshot) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, UpdateSnapshot)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto under_test = SubscriberAuth(MakeTypicalMockAuth(), mock);
  grpc::ClientContext ctx;
  google::pubsub::v1::UpdateSnapshotRequest request;
  auto auth_failure = under_test.UpdateSnapshot(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.UpdateSnapshot(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(SubscriberAuthTest, DeleteSnapshot) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, DeleteSnapshot)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto under_test = SubscriberAuth(MakeTypicalMockAuth(), mock);
  grpc::ClientContext ctx;
  google::pubsub::v1::DeleteSnapshotRequest request;
  auto auth_failure = under_test.DeleteSnapshot(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.DeleteSnapshot(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(SubscriberAuthTest, Seek) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, Seek)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto under_test = SubscriberAuth(MakeTypicalMockAuth(), mock);
  grpc::ClientContext ctx;
  google::pubsub::v1::SeekRequest request;
  auto auth_failure = under_test.Seek(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.Seek(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
