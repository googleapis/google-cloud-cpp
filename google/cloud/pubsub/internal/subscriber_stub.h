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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SUBSCRIBER_STUB_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SUBSCRIBER_STUB_H

#include "google/cloud/pubsub/connection_options.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/async_streaming_read_write_rpc.h"
#include "google/cloud/status_or.h"
#include <google/pubsub/v1/pubsub.grpc.pb.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Define the interface for the gRPC wrapper.
 *
 * We wrap the gRPC-generated `SubscriberStub` to:
 *   - Return a StatusOr<T> instead of using a `grpc::Status` and an "output
 *     parameter" for the response.
 *   - To be able to mock the stubs.
 *   - To be able to decompose some functionality (logging, adding metadata
 *     information) into layers.
 */
class SubscriberStub {
 public:
  virtual ~SubscriberStub() = default;

  /// Create a new subscription.
  virtual StatusOr<google::pubsub::v1::Subscription> CreateSubscription(
      grpc::ClientContext& context,
      google::pubsub::v1::Subscription const& request) = 0;

  /// Get full metadata information about a subscription.
  virtual StatusOr<google::pubsub::v1::Subscription> GetSubscription(
      grpc::ClientContext& context,
      google::pubsub::v1::GetSubscriptionRequest const& request) = 0;

  /// Update an existing subscription.
  virtual StatusOr<google::pubsub::v1::Subscription> UpdateSubscription(
      grpc::ClientContext& context,
      google::pubsub::v1::UpdateSubscriptionRequest const& request) = 0;

  /// List existing subscriptions.
  virtual StatusOr<google::pubsub::v1::ListSubscriptionsResponse>
  ListSubscriptions(
      grpc::ClientContext& context,
      google::pubsub::v1::ListSubscriptionsRequest const& request) = 0;

  /// Delete a subscription.
  virtual Status DeleteSubscription(
      grpc::ClientContext& context,
      google::pubsub::v1::DeleteSubscriptionRequest const& request) = 0;

  /// Start a bi-directional stream to read messages and send ack/nacks.
  virtual std::unique_ptr<::google::cloud::AsyncStreamingReadWriteRpc<
      google::pubsub::v1::StreamingPullRequest,
      google::pubsub::v1::StreamingPullResponse>>
  AsyncStreamingPull(google::cloud::CompletionQueue const& cq,
                     std::unique_ptr<grpc::ClientContext> context) = 0;

  /// Modify the push configuration of an existing subscription.
  virtual Status ModifyPushConfig(
      grpc::ClientContext& context,
      google::pubsub::v1::ModifyPushConfigRequest const& request) = 0;

  /// Get information about an existing snapshot.
  virtual StatusOr<google::pubsub::v1::Snapshot> GetSnapshot(
      grpc::ClientContext& context,
      google::pubsub::v1::GetSnapshotRequest const& request) = 0;

  /// List existing snapshots.
  virtual StatusOr<google::pubsub::v1::ListSnapshotsResponse> ListSnapshots(
      grpc::ClientContext& context,
      google::pubsub::v1::ListSnapshotsRequest const& request) = 0;

  /// Create a new snapshot.
  virtual StatusOr<google::pubsub::v1::Snapshot> CreateSnapshot(
      grpc::ClientContext& context,
      google::pubsub::v1::CreateSnapshotRequest const& request) = 0;

  /// Update an existing snapshot.
  virtual StatusOr<google::pubsub::v1::Snapshot> UpdateSnapshot(
      grpc::ClientContext& context,
      google::pubsub::v1::UpdateSnapshotRequest const& request) = 0;

  /// Delete a snapshot.
  virtual Status DeleteSnapshot(
      grpc::ClientContext& context,
      google::pubsub::v1::DeleteSnapshotRequest const& request) = 0;

  /// Seeks an existing subscription to a point in time or a snapshot.
  virtual StatusOr<google::pubsub::v1::SeekResponse> Seek(
      grpc::ClientContext& context,
      google::pubsub::v1::SeekRequest const& request) = 0;

  /// Acknowledge exactly one message.
  virtual future<Status> AsyncAcknowledge(
      google::cloud::CompletionQueue& cq,
      std::unique_ptr<grpc::ClientContext> context,
      google::pubsub::v1::AcknowledgeRequest const& request) = 0;

  /// Modify the acknowledgement deadline for many messages.
  virtual future<Status> AsyncModifyAckDeadline(
      google::cloud::CompletionQueue& cq,
      std::unique_ptr<grpc::ClientContext> context,
      google::pubsub::v1::ModifyAckDeadlineRequest const& request) = 0;
};

class DefaultSubscriberStub : public SubscriberStub {
 public:
  explicit DefaultSubscriberStub(
      std::unique_ptr<google::pubsub::v1::Subscriber::StubInterface> grpc_stub)
      : grpc_stub_(std::move(grpc_stub)) {}

  ~DefaultSubscriberStub() override = default;

  StatusOr<google::pubsub::v1::Subscription> CreateSubscription(
      grpc::ClientContext& client_context,
      google::pubsub::v1::Subscription const& request) override;

  StatusOr<google::pubsub::v1::Subscription> GetSubscription(
      grpc::ClientContext& client_context,
      google::pubsub::v1::GetSubscriptionRequest const& request) override;

  StatusOr<google::pubsub::v1::Subscription> UpdateSubscription(
      grpc::ClientContext& client_context,
      google::pubsub::v1::UpdateSubscriptionRequest const& request) override;

  StatusOr<google::pubsub::v1::ListSubscriptionsResponse> ListSubscriptions(
      grpc::ClientContext& client_context,
      google::pubsub::v1::ListSubscriptionsRequest const& request) override;

  Status DeleteSubscription(
      grpc::ClientContext& client_context,
      google::pubsub::v1::DeleteSubscriptionRequest const& request) override;

  std::unique_ptr<::google::cloud::AsyncStreamingReadWriteRpc<
      google::pubsub::v1::StreamingPullRequest,
      google::pubsub::v1::StreamingPullResponse>>
  AsyncStreamingPull(google::cloud::CompletionQueue const&,
                     std::unique_ptr<grpc::ClientContext>) override;

  Status ModifyPushConfig(
      grpc::ClientContext& client_context,
      google::pubsub::v1::ModifyPushConfigRequest const& request) override;

  StatusOr<google::pubsub::v1::Snapshot> GetSnapshot(
      grpc::ClientContext& client_context,
      google::pubsub::v1::GetSnapshotRequest const& request) override;

  StatusOr<google::pubsub::v1::ListSnapshotsResponse> ListSnapshots(
      grpc::ClientContext& client_context,
      google::pubsub::v1::ListSnapshotsRequest const& request) override;

  StatusOr<google::pubsub::v1::Snapshot> CreateSnapshot(
      grpc::ClientContext& client_context,
      google::pubsub::v1::CreateSnapshotRequest const& request) override;

  StatusOr<google::pubsub::v1::Snapshot> UpdateSnapshot(
      grpc::ClientContext& client_context,
      google::pubsub::v1::UpdateSnapshotRequest const& request) override;

  Status DeleteSnapshot(
      grpc::ClientContext& client_context,
      google::pubsub::v1::DeleteSnapshotRequest const& request) override;

  StatusOr<google::pubsub::v1::SeekResponse> Seek(
      grpc::ClientContext& client_context,
      google::pubsub::v1::SeekRequest const& request) override;

  future<Status> AsyncModifyAckDeadline(
      google::cloud::CompletionQueue& cq,
      std::unique_ptr<grpc::ClientContext> context,
      google::pubsub::v1::ModifyAckDeadlineRequest const& request) override;

  future<Status> AsyncAcknowledge(
      google::cloud::CompletionQueue& cq,
      std::unique_ptr<grpc::ClientContext> context,
      google::pubsub::v1::AcknowledgeRequest const& request) override;

 private:
  std::unique_ptr<google::pubsub::v1::Subscriber::StubInterface> grpc_stub_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SUBSCRIBER_STUB_H
