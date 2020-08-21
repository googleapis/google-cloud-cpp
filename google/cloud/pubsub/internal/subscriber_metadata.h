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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SUBSCRIBER_METADATA_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SUBSCRIBER_METADATA_H

#include "google/cloud/pubsub/internal/subscriber_stub.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/tracing_options.h"
#include <memory>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

class SubscriberMetadata : public SubscriberStub {
 public:
  explicit SubscriberMetadata(std::shared_ptr<SubscriberStub> child);

  StatusOr<google::pubsub::v1::Subscription> CreateSubscription(
      grpc::ClientContext& context,
      google::pubsub::v1::Subscription const& request) override;

  StatusOr<google::pubsub::v1::Subscription> GetSubscription(
      grpc::ClientContext& context,
      google::pubsub::v1::GetSubscriptionRequest const& request) override;

  StatusOr<google::pubsub::v1::Subscription> UpdateSubscription(
      grpc::ClientContext& context,
      google::pubsub::v1::UpdateSubscriptionRequest const& request) override;

  StatusOr<google::pubsub::v1::ListSubscriptionsResponse> ListSubscriptions(
      grpc::ClientContext& context,
      google::pubsub::v1::ListSubscriptionsRequest const& request) override;

  Status DeleteSubscription(
      grpc::ClientContext& context,
      google::pubsub::v1::DeleteSubscriptionRequest const& request) override;

  Status ModifyPushConfig(
      grpc::ClientContext& context,
      google::pubsub::v1::ModifyPushConfigRequest const& request) override;

  future<StatusOr<google::pubsub::v1::PullResponse>> AsyncPull(
      google::cloud::CompletionQueue& cq,
      std::unique_ptr<grpc::ClientContext> context,
      google::pubsub::v1::PullRequest const& request) override;

  future<Status> AsyncAcknowledge(
      google::cloud::CompletionQueue& cq,
      std::unique_ptr<grpc::ClientContext> context,
      google::pubsub::v1::AcknowledgeRequest const& request) override;

  future<Status> AsyncModifyAckDeadline(
      google::cloud::CompletionQueue& cq,
      std::unique_ptr<grpc::ClientContext> context,
      google::pubsub::v1::ModifyAckDeadlineRequest const& request) override;

  StatusOr<google::pubsub::v1::Snapshot> CreateSnapshot(
      grpc::ClientContext& context,
      google::pubsub::v1::CreateSnapshotRequest const& request) override;

  StatusOr<google::pubsub::v1::Snapshot> GetSnapshot(
      grpc::ClientContext& context,
      google::pubsub::v1::GetSnapshotRequest const& request) override;

  StatusOr<google::pubsub::v1::ListSnapshotsResponse> ListSnapshots(
      grpc::ClientContext& context,
      google::pubsub::v1::ListSnapshotsRequest const& request) override;

  StatusOr<google::pubsub::v1::Snapshot> UpdateSnapshot(
      grpc::ClientContext& context,
      google::pubsub::v1::UpdateSnapshotRequest const& request) override;

  Status DeleteSnapshot(
      grpc::ClientContext& context,
      google::pubsub::v1::DeleteSnapshotRequest const& request) override;

  StatusOr<google::pubsub::v1::SeekResponse> Seek(
      grpc::ClientContext& context,
      google::pubsub::v1::SeekRequest const& request) override;

 private:
  void SetMetadata(grpc::ClientContext& context,
                   std::string const& request_params);

  std::shared_ptr<SubscriberStub> child_;
  std::string x_goog_api_client_;
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SUBSCRIBER_METADATA_H
