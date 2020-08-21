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
#include "google/cloud/internal/api_client_header.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

SubscriberMetadata::SubscriberMetadata(std::shared_ptr<SubscriberStub> child)
    : child_(std::move(child)),
      x_goog_api_client_(google::cloud::internal::ApiClientHeader()) {}

StatusOr<google::pubsub::v1::Subscription>
SubscriberMetadata::CreateSubscription(
    grpc::ClientContext& context,
    google::pubsub::v1::Subscription const& request) {
  SetMetadata(context, "name=" + request.name());
  return child_->CreateSubscription(context, request);
}

StatusOr<google::pubsub::v1::Subscription> SubscriberMetadata::GetSubscription(
    grpc::ClientContext& context,
    google::pubsub::v1::GetSubscriptionRequest const& request) {
  SetMetadata(context, "subscription=" + request.subscription());
  return child_->GetSubscription(context, request);
}

StatusOr<google::pubsub::v1::Subscription>
SubscriberMetadata::UpdateSubscription(
    grpc::ClientContext& context,
    google::pubsub::v1::UpdateSubscriptionRequest const& request) {
  SetMetadata(context, "subscription.name=" + request.subscription().name());

  return child_->UpdateSubscription(context, request);
}

StatusOr<google::pubsub::v1::ListSubscriptionsResponse>
SubscriberMetadata::ListSubscriptions(
    grpc::ClientContext& context,
    google::pubsub::v1::ListSubscriptionsRequest const& request) {
  SetMetadata(context, "project=" + request.project());
  return child_->ListSubscriptions(context, request);
}

Status SubscriberMetadata::DeleteSubscription(
    grpc::ClientContext& context,
    google::pubsub::v1::DeleteSubscriptionRequest const& request) {
  SetMetadata(context, "subscription=" + request.subscription());
  return child_->DeleteSubscription(context, request);
}

Status SubscriberMetadata::ModifyPushConfig(
    grpc::ClientContext& context,
    google::pubsub::v1::ModifyPushConfigRequest const& request) {
  SetMetadata(context, "subscription=" + request.subscription());
  return child_->ModifyPushConfig(context, request);
}

future<StatusOr<google::pubsub::v1::PullResponse>>
SubscriberMetadata::AsyncPull(google::cloud::CompletionQueue& cq,
                              std::unique_ptr<grpc::ClientContext> context,
                              google::pubsub::v1::PullRequest const& request) {
  SetMetadata(*context, "subscription=" + request.subscription());
  return child_->AsyncPull(cq, std::move(context), request);
}

future<Status> SubscriberMetadata::AsyncAcknowledge(
    google::cloud::CompletionQueue& cq,
    std::unique_ptr<grpc::ClientContext> context,
    google::pubsub::v1::AcknowledgeRequest const& request) {
  SetMetadata(*context, "subscription=" + request.subscription());
  return child_->AsyncAcknowledge(cq, std::move(context), request);
}

future<Status> SubscriberMetadata::AsyncModifyAckDeadline(
    google::cloud::CompletionQueue& cq,
    std::unique_ptr<grpc::ClientContext> context,
    google::pubsub::v1::ModifyAckDeadlineRequest const& request) {
  SetMetadata(*context, "subscription=" + request.subscription());
  return child_->AsyncModifyAckDeadline(cq, std::move(context), request);
}

StatusOr<google::pubsub::v1::Snapshot> SubscriberMetadata::CreateSnapshot(
    grpc::ClientContext& context,
    google::pubsub::v1::CreateSnapshotRequest const& request) {
  SetMetadata(context, "name=" + request.name());
  return child_->CreateSnapshot(context, request);
}

StatusOr<google::pubsub::v1::ListSnapshotsResponse>
SubscriberMetadata::ListSnapshots(
    grpc::ClientContext& context,
    google::pubsub::v1::ListSnapshotsRequest const& request) {
  SetMetadata(context, "project=" + request.project());
  return child_->ListSnapshots(context, request);
}

StatusOr<google::pubsub::v1::Snapshot> SubscriberMetadata::GetSnapshot(
    grpc::ClientContext& context,
    google::pubsub::v1::GetSnapshotRequest const& request) {
  SetMetadata(context, "snapshot=" + request.snapshot());
  return child_->GetSnapshot(context, request);
}

StatusOr<google::pubsub::v1::Snapshot> SubscriberMetadata::UpdateSnapshot(
    grpc::ClientContext& context,
    google::pubsub::v1::UpdateSnapshotRequest const& request) {
  SetMetadata(context, "snapshot.name=" + request.snapshot().name());
  return child_->UpdateSnapshot(context, request);
}

Status SubscriberMetadata::DeleteSnapshot(
    grpc::ClientContext& context,
    google::pubsub::v1::DeleteSnapshotRequest const& request) {
  SetMetadata(context, "snapshot=" + request.snapshot());
  return child_->DeleteSnapshot(context, request);
}

StatusOr<google::pubsub::v1::SeekResponse> SubscriberMetadata::Seek(
    grpc::ClientContext& context,
    google::pubsub::v1::SeekRequest const& request) {
  SetMetadata(context, "subscription=" + request.subscription());
  return child_->Seek(context, request);
}

void SubscriberMetadata::SetMetadata(grpc::ClientContext& context,
                                     std::string const& request_params) {
  context.AddMetadata("x-goog-request-params", request_params);
  context.AddMetadata("x-goog-api-client", x_goog_api_client_);
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
