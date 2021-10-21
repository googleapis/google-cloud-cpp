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

#include "google/cloud/pubsub/internal/publisher_logging.h"
#include "google/cloud/internal/log_wrapper.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

using ::google::cloud::internal::LogWrapper;

StatusOr<google::pubsub::v1::Topic> PublisherLogging::CreateTopic(
    grpc::ClientContext& context, google::pubsub::v1::Topic const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::pubsub::v1::Topic const& request) {
        return child_->CreateTopic(context, request);
      },
      context, request, __func__, tracing_options_);
}

StatusOr<google::pubsub::v1::Topic> PublisherLogging::GetTopic(
    grpc::ClientContext& context,
    google::pubsub::v1::GetTopicRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::pubsub::v1::GetTopicRequest const& request) {
        return child_->GetTopic(context, request);
      },
      context, request, __func__, tracing_options_);
}

StatusOr<google::pubsub::v1::Topic> PublisherLogging::UpdateTopic(
    grpc::ClientContext& context,
    google::pubsub::v1::UpdateTopicRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::pubsub::v1::UpdateTopicRequest const& request) {
        return child_->UpdateTopic(context, request);
      },
      context, request, __func__, tracing_options_);
}

StatusOr<google::pubsub::v1::ListTopicsResponse> PublisherLogging::ListTopics(
    grpc::ClientContext& context,
    google::pubsub::v1::ListTopicsRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::pubsub::v1::ListTopicsRequest const& request) {
        return child_->ListTopics(context, request);
      },
      context, request, __func__, tracing_options_);
}

Status PublisherLogging::DeleteTopic(
    grpc::ClientContext& context,
    google::pubsub::v1::DeleteTopicRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::pubsub::v1::DeleteTopicRequest const& request) {
        return child_->DeleteTopic(context, request);
      },
      context, request, __func__, tracing_options_);
}

StatusOr<google::pubsub::v1::DetachSubscriptionResponse>
PublisherLogging::DetachSubscription(
    grpc::ClientContext& context,
    google::pubsub::v1::DetachSubscriptionRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::pubsub::v1::DetachSubscriptionRequest const& request) {
        return child_->DetachSubscription(context, request);
      },
      context, request, __func__, tracing_options_);
}

StatusOr<google::pubsub::v1::ListTopicSubscriptionsResponse>
PublisherLogging::ListTopicSubscriptions(
    grpc::ClientContext& context,
    google::pubsub::v1::ListTopicSubscriptionsRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::pubsub::v1::ListTopicSubscriptionsRequest const& request) {
        return child_->ListTopicSubscriptions(context, request);
      },
      context, request, __func__, tracing_options_);
}

StatusOr<google::pubsub::v1::ListTopicSnapshotsResponse>
PublisherLogging::ListTopicSnapshots(
    grpc::ClientContext& context,
    google::pubsub::v1::ListTopicSnapshotsRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::pubsub::v1::ListTopicSnapshotsRequest const& request) {
        return child_->ListTopicSnapshots(context, request);
      },
      context, request, __func__, tracing_options_);
}

future<StatusOr<google::pubsub::v1::PublishResponse>>
PublisherLogging::AsyncPublish(
    google::cloud::CompletionQueue& cq,
    std::unique_ptr<grpc::ClientContext> context,
    google::pubsub::v1::PublishRequest const& request) {
  return LogWrapper(
      [this](google::cloud::CompletionQueue& cq,
             std::unique_ptr<grpc::ClientContext> context,
             google::pubsub::v1::PublishRequest const& request) {
        return child_->AsyncPublish(cq, std::move(context), request);
      },
      cq, std::move(context), request, __func__, tracing_options_);
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
