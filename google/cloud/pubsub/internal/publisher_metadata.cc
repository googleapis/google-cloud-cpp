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
#include "google/cloud/internal/api_client_header.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

PublisherMetadata::PublisherMetadata(std::shared_ptr<PublisherStub> child)
    : child_(std::move(child)),
      x_goog_api_client_(google::cloud::internal::ApiClientHeader()) {}

StatusOr<google::pubsub::v1::Topic> PublisherMetadata::CreateTopic(
    grpc::ClientContext& context, google::pubsub::v1::Topic const& request) {
  SetMetadata(context, "name=" + request.name());
  return child_->CreateTopic(context, request);
}

StatusOr<google::pubsub::v1::Topic> PublisherMetadata::GetTopic(
    grpc::ClientContext& context,
    google::pubsub::v1::GetTopicRequest const& request) {
  SetMetadata(context, "topic=" + request.topic());
  return child_->GetTopic(context, request);
}

StatusOr<google::pubsub::v1::Topic> PublisherMetadata::UpdateTopic(
    grpc::ClientContext& context,
    google::pubsub::v1::UpdateTopicRequest const& request) {
  SetMetadata(context, "topic.name=" + request.topic().name());
  return child_->UpdateTopic(context, request);
}

StatusOr<google::pubsub::v1::ListTopicsResponse> PublisherMetadata::ListTopics(
    grpc::ClientContext& context,
    google::pubsub::v1::ListTopicsRequest const& request) {
  SetMetadata(context, "project=" + request.project());
  return child_->ListTopics(context, request);
}

Status PublisherMetadata::DeleteTopic(
    grpc::ClientContext& context,
    google::pubsub::v1::DeleteTopicRequest const& request) {
  SetMetadata(context, "topic=" + request.topic());
  return child_->DeleteTopic(context, request);
}

StatusOr<google::pubsub::v1::DetachSubscriptionResponse>
PublisherMetadata::DetachSubscription(
    grpc::ClientContext& context,
    google::pubsub::v1::DetachSubscriptionRequest const& request) {
  SetMetadata(context, "subscription=" + request.subscription());
  return child_->DetachSubscription(context, request);
}

StatusOr<google::pubsub::v1::ListTopicSubscriptionsResponse>
PublisherMetadata::ListTopicSubscriptions(
    grpc::ClientContext& context,
    google::pubsub::v1::ListTopicSubscriptionsRequest const& request) {
  SetMetadata(context, "topic=" + request.topic());
  return child_->ListTopicSubscriptions(context, request);
}

StatusOr<google::pubsub::v1::ListTopicSnapshotsResponse>
PublisherMetadata::ListTopicSnapshots(
    grpc::ClientContext& context,
    google::pubsub::v1::ListTopicSnapshotsRequest const& request) {
  SetMetadata(context, "topic=" + request.topic());
  return child_->ListTopicSnapshots(context, request);
}

future<StatusOr<google::pubsub::v1::PublishResponse>>
PublisherMetadata::AsyncPublish(
    google::cloud::CompletionQueue& cq,
    std::unique_ptr<grpc::ClientContext> context,
    google::pubsub::v1::PublishRequest const& request) {
  SetMetadata(*context, "topic=" + request.topic());
  return child_->AsyncPublish(cq, std::move(context), request);
}

void PublisherMetadata::SetMetadata(grpc::ClientContext& context,
                                    std::string const& request_params) {
  context.AddMetadata("x-goog-request-params", request_params);
  context.AddMetadata("x-goog-api-client", x_goog_api_client_);
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
