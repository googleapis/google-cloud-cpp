// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_PUBLISHER_AUTH_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_PUBLISHER_AUTH_H

#include "google/cloud/pubsub/internal/publisher_stub.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/internal/unified_grpc_credentials.h"
#include <memory>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class PublisherAuth : public PublisherStub {
 public:
  PublisherAuth(
      std::shared_ptr<google::cloud::internal::GrpcAuthenticationStrategy> auth,
      std::shared_ptr<PublisherStub> child)
      : auth_(std::move(auth)), child_(std::move(child)) {}

  StatusOr<google::pubsub::v1::Topic> CreateTopic(
      grpc::ClientContext& context,
      google::pubsub::v1::Topic const& request) override;

  StatusOr<google::pubsub::v1::Topic> GetTopic(
      grpc::ClientContext& context,
      google::pubsub::v1::GetTopicRequest const& request) override;

  StatusOr<google::pubsub::v1::Topic> UpdateTopic(
      grpc::ClientContext& context,
      google::pubsub::v1::UpdateTopicRequest const& request) override;

  StatusOr<google::pubsub::v1::ListTopicsResponse> ListTopics(
      grpc::ClientContext& context,
      google::pubsub::v1::ListTopicsRequest const& request) override;

  Status DeleteTopic(
      grpc::ClientContext& context,
      google::pubsub::v1::DeleteTopicRequest const& request) override;

  StatusOr<google::pubsub::v1::DetachSubscriptionResponse> DetachSubscription(
      grpc::ClientContext& context,
      google::pubsub::v1::DetachSubscriptionRequest const& request) override;

  StatusOr<google::pubsub::v1::ListTopicSubscriptionsResponse>
  ListTopicSubscriptions(
      grpc::ClientContext& context,
      google::pubsub::v1::ListTopicSubscriptionsRequest const& request)
      override;

  StatusOr<google::pubsub::v1::ListTopicSnapshotsResponse> ListTopicSnapshots(
      grpc::ClientContext& context,
      google::pubsub::v1::ListTopicSnapshotsRequest const& request) override;

  future<StatusOr<google::pubsub::v1::PublishResponse>> AsyncPublish(
      google::cloud::CompletionQueue& cq,
      std::unique_ptr<grpc::ClientContext> context,
      google::pubsub::v1::PublishRequest const& request) override;

 private:
  std::shared_ptr<google::cloud::internal::GrpcAuthenticationStrategy> auth_;
  std::shared_ptr<PublisherStub> child_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_PUBLISHER_AUTH_H
