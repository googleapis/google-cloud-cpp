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

#include "google/cloud/pubsub/internal/subscriber_round_robin.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

StatusOr<google::pubsub::v1::Subscription>
SubscriberRoundRobin::CreateSubscription(
    grpc::ClientContext& context,
    google::pubsub::v1::Subscription const& request) {
  return Child()->CreateSubscription(context, request);
}

StatusOr<google::pubsub::v1::Subscription>
SubscriberRoundRobin::GetSubscription(
    grpc::ClientContext& context,
    google::pubsub::v1::GetSubscriptionRequest const& request) {
  return Child()->GetSubscription(context, request);
}

StatusOr<google::pubsub::v1::Subscription>
SubscriberRoundRobin::UpdateSubscription(
    grpc::ClientContext& context,
    google::pubsub::v1::UpdateSubscriptionRequest const& request) {
  return Child()->UpdateSubscription(context, request);
}

StatusOr<google::pubsub::v1::ListSubscriptionsResponse>
SubscriberRoundRobin::ListSubscriptions(
    grpc::ClientContext& context,
    google::pubsub::v1::ListSubscriptionsRequest const& request) {
  return Child()->ListSubscriptions(context, request);
}

Status SubscriberRoundRobin::DeleteSubscription(
    grpc::ClientContext& context,
    google::pubsub::v1::DeleteSubscriptionRequest const& request) {
  return Child()->DeleteSubscription(context, request);
}

Status SubscriberRoundRobin::ModifyPushConfig(
    grpc::ClientContext& context,
    google::pubsub::v1::ModifyPushConfigRequest const& request) {
  return Child()->ModifyPushConfig(context, request);
}

std::unique_ptr<SubscriberStub::AsyncPullStream>
SubscriberRoundRobin::AsyncStreamingPull(
    google::cloud::CompletionQueue& cq,
    std::unique_ptr<grpc::ClientContext> context,
    google::pubsub::v1::StreamingPullRequest const& request) {
  return Child()->AsyncStreamingPull(cq, std::move(context), request);
}

StatusOr<google::pubsub::v1::Snapshot> SubscriberRoundRobin::CreateSnapshot(
    grpc::ClientContext& context,
    google::pubsub::v1::CreateSnapshotRequest const& request) {
  return Child()->CreateSnapshot(context, request);
}

StatusOr<google::pubsub::v1::ListSnapshotsResponse>
SubscriberRoundRobin::ListSnapshots(
    grpc::ClientContext& context,
    google::pubsub::v1::ListSnapshotsRequest const& request) {
  return Child()->ListSnapshots(context, request);
}

StatusOr<google::pubsub::v1::Snapshot> SubscriberRoundRobin::GetSnapshot(
    grpc::ClientContext& context,
    google::pubsub::v1::GetSnapshotRequest const& request) {
  return Child()->GetSnapshot(context, request);
}

StatusOr<google::pubsub::v1::Snapshot> SubscriberRoundRobin::UpdateSnapshot(
    grpc::ClientContext& context,
    google::pubsub::v1::UpdateSnapshotRequest const& request) {
  return Child()->UpdateSnapshot(context, request);
}

Status SubscriberRoundRobin::DeleteSnapshot(
    grpc::ClientContext& context,
    google::pubsub::v1::DeleteSnapshotRequest const& request) {
  return Child()->DeleteSnapshot(context, request);
}

StatusOr<google::pubsub::v1::SeekResponse> SubscriberRoundRobin::Seek(
    grpc::ClientContext& context,
    google::pubsub::v1::SeekRequest const& request) {
  return Child()->Seek(context, request);
}

std::shared_ptr<SubscriberStub> SubscriberRoundRobin::Child() {
  std::lock_guard<std::mutex> lk(mu_);
  auto child = children_[current_];
  current_ = (current_ + 1) % children_.size();
  return child;
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
