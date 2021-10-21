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

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

StatusOr<google::pubsub::v1::Topic> PublisherRoundRobin::CreateTopic(
    grpc::ClientContext& context, google::pubsub::v1::Topic const& request) {
  return Child()->CreateTopic(context, request);
}

StatusOr<google::pubsub::v1::Topic> PublisherRoundRobin::GetTopic(
    grpc::ClientContext& context,
    google::pubsub::v1::GetTopicRequest const& request) {
  return Child()->GetTopic(context, request);
}

StatusOr<google::pubsub::v1::Topic> PublisherRoundRobin::UpdateTopic(
    grpc::ClientContext& context,
    google::pubsub::v1::UpdateTopicRequest const& request) {
  return Child()->UpdateTopic(context, request);
}

StatusOr<google::pubsub::v1::ListTopicsResponse>
PublisherRoundRobin::ListTopics(
    grpc::ClientContext& context,
    google::pubsub::v1::ListTopicsRequest const& request) {
  return Child()->ListTopics(context, request);
}

Status PublisherRoundRobin::DeleteTopic(
    grpc::ClientContext& context,
    google::pubsub::v1::DeleteTopicRequest const& request) {
  return Child()->DeleteTopic(context, request);
}

StatusOr<google::pubsub::v1::DetachSubscriptionResponse>
PublisherRoundRobin::DetachSubscription(
    grpc::ClientContext& context,
    google::pubsub::v1::DetachSubscriptionRequest const& request) {
  return Child()->DetachSubscription(context, request);
}

StatusOr<google::pubsub::v1::ListTopicSubscriptionsResponse>
PublisherRoundRobin::ListTopicSubscriptions(
    grpc::ClientContext& context,
    google::pubsub::v1::ListTopicSubscriptionsRequest const& request) {
  return Child()->ListTopicSubscriptions(context, request);
}

StatusOr<google::pubsub::v1::ListTopicSnapshotsResponse>
PublisherRoundRobin::ListTopicSnapshots(
    grpc::ClientContext& context,
    google::pubsub::v1::ListTopicSnapshotsRequest const& request) {
  return Child()->ListTopicSnapshots(context, request);
}

future<StatusOr<google::pubsub::v1::PublishResponse>>
PublisherRoundRobin::AsyncPublish(
    google::cloud::CompletionQueue& cq,
    std::unique_ptr<grpc::ClientContext> context,
    google::pubsub::v1::PublishRequest const& request) {
  return Child()->AsyncPublish(cq, std::move(context), request);
}

std::shared_ptr<PublisherStub> PublisherRoundRobin::Child() {
  std::lock_guard<std::mutex> lk(mu_);
  auto child = children_[current_];
  current_ = (current_ + 1) % children_.size();
  return child;
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
