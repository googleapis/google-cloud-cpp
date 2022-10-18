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
#include "google/cloud/internal/async_read_write_stream_auth.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

StatusOr<google::pubsub::v1::Subscription> SubscriberAuth::CreateSubscription(
    grpc::ClientContext& context,
    google::pubsub::v1::Subscription const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->CreateSubscription(context, request);
}

StatusOr<google::pubsub::v1::Subscription> SubscriberAuth::GetSubscription(
    grpc::ClientContext& context,
    google::pubsub::v1::GetSubscriptionRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->GetSubscription(context, request);
}

StatusOr<google::pubsub::v1::Subscription> SubscriberAuth::UpdateSubscription(
    grpc::ClientContext& context,
    google::pubsub::v1::UpdateSubscriptionRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->UpdateSubscription(context, request);
}

StatusOr<google::pubsub::v1::ListSubscriptionsResponse>
SubscriberAuth::ListSubscriptions(
    grpc::ClientContext& context,
    google::pubsub::v1::ListSubscriptionsRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->ListSubscriptions(context, request);
}

Status SubscriberAuth::DeleteSubscription(
    grpc::ClientContext& context,
    google::pubsub::v1::DeleteSubscriptionRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->DeleteSubscription(context, request);
}

Status SubscriberAuth::ModifyPushConfig(
    grpc::ClientContext& context,
    google::pubsub::v1::ModifyPushConfigRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->ModifyPushConfig(context, request);
}

std::unique_ptr<SubscriberStub::AsyncPullStream>
SubscriberAuth::AsyncStreamingPull(
    google::cloud::CompletionQueue& cq,
    std::unique_ptr<grpc::ClientContext> context,
    google::pubsub::v1::StreamingPullRequest const& request) {
  using StreamAuth = google::cloud::internal::AsyncStreamingReadWriteRpcAuth<
      google::pubsub::v1::StreamingPullRequest,
      google::pubsub::v1::StreamingPullResponse>;

  auto& child = child_;
  auto call = [child, cq,
               request](std::unique_ptr<grpc::ClientContext> ctx) mutable {
    return child->AsyncStreamingPull(cq, std::move(ctx), request);
  };
  return absl::make_unique<StreamAuth>(
      std::move(context), auth_, StreamAuth::StreamFactory(std::move(call)));
}

future<Status> SubscriberAuth::AsyncAcknowledge(
    google::cloud::CompletionQueue& cq,
    std::unique_ptr<grpc::ClientContext> context,
    google::pubsub::v1::AcknowledgeRequest const& request) {
  auto& child = child_;
  return auth_->AsyncConfigureContext(std::move(context))
      .then([cq, child,
             request](future<StatusOr<std::unique_ptr<grpc::ClientContext>>>
                          f) mutable {
        auto context = f.get();
        if (!context) return make_ready_future(std::move(context).status());
        return child->AsyncAcknowledge(cq, *std::move(context), request);
      });
}

future<Status> SubscriberAuth::AsyncModifyAckDeadline(
    google::cloud::CompletionQueue& cq,
    std::unique_ptr<grpc::ClientContext> context,
    google::pubsub::v1::ModifyAckDeadlineRequest const& request) {
  auto& child = child_;
  return auth_->AsyncConfigureContext(std::move(context))
      .then([cq, child,
             request](future<StatusOr<std::unique_ptr<grpc::ClientContext>>>
                          f) mutable {
        auto context = f.get();
        if (!context) return make_ready_future(std::move(context).status());
        return child->AsyncModifyAckDeadline(cq, *std::move(context), request);
      });
}

StatusOr<google::pubsub::v1::Snapshot> SubscriberAuth::CreateSnapshot(
    grpc::ClientContext& context,
    google::pubsub::v1::CreateSnapshotRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->CreateSnapshot(context, request);
}

StatusOr<google::pubsub::v1::ListSnapshotsResponse>
SubscriberAuth::ListSnapshots(
    grpc::ClientContext& context,
    google::pubsub::v1::ListSnapshotsRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->ListSnapshots(context, request);
}

StatusOr<google::pubsub::v1::Snapshot> SubscriberAuth::GetSnapshot(
    grpc::ClientContext& context,
    google::pubsub::v1::GetSnapshotRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->GetSnapshot(context, request);
}

StatusOr<google::pubsub::v1::Snapshot> SubscriberAuth::UpdateSnapshot(
    grpc::ClientContext& context,
    google::pubsub::v1::UpdateSnapshotRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->UpdateSnapshot(context, request);
}

Status SubscriberAuth::DeleteSnapshot(
    grpc::ClientContext& context,
    google::pubsub::v1::DeleteSnapshotRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->DeleteSnapshot(context, request);
}

StatusOr<google::pubsub::v1::SeekResponse> SubscriberAuth::Seek(
    grpc::ClientContext& context,
    google::pubsub::v1::SeekRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->Seek(context, request);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
