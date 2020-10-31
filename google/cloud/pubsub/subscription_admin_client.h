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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SUBSCRIPTION_ADMIN_CLIENT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SUBSCRIPTION_ADMIN_CLIENT_H

#include "google/cloud/pubsub/snapshot_builder.h"
#include "google/cloud/pubsub/subscription_admin_connection.h"
#include "google/cloud/pubsub/subscription_builder.h"
#include "google/cloud/pubsub/version.h"
#include <memory>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

/**
 * Performs subscription administrative operations in Cloud Pub/Sub.
 *
 * Applications use this class to perform subscription administrative operations
 * on [Cloud Pub/Sub][pubsub-doc-link].
 *
 * @par Performance
 * `SubscriptionAdminClient` objects are relatively cheap to create, copy, and
 * move. However, each `SubscriptionAdminClient` object must be created with a
 * `std::shared_ptr<SubscriptionAdminConnection>`, which itself is relatively
 * expensive to create. Therefore, connection instances should be shared when
 * possible. See the `MakeSubscriptionAdminConnection()` function and the
 * `SubscriptionAdminConnection` interface for more details.
 *
 * @par Thread Safety
 * Instances of this class created via copy-construction or copy-assignment
 * share the underlying pool of connections. Access to these copies via multiple
 * threads is guaranteed to work. Two threads operating on the same instance of
 * this class is not guaranteed to work.
 *
 * @par Error Handling
 * This class uses `StatusOr<T>` to report errors. When an operation fails to
 * perform its work the returned `StatusOr<T>` contains the error details. If
 * the `ok()` member function in the `StatusOr<T>` returns `true` then it
 * contains the expected result. Please consult the
 * [`StatusOr<T>` documentation](#google::cloud::v1::StatusOr) for more details.
 *
 * [pubsub-doc-link]: https://cloud.google.com/pubsub/docs
 */
class SubscriptionAdminClient {
 public:
  explicit SubscriptionAdminClient(
      std::shared_ptr<SubscriptionAdminConnection> connection);

  /**
   * The default constructor is deleted.
   *
   * Use `SubscriberClient(std::shared_ptr<SubscriberConnection>)`
   */
  SubscriptionAdminClient() = delete;

  /**
   * Creates a new subscription in Cloud Pub/Sub.
   *
   * @par Idempotency
   * This operation is idempotent, the state of the system is the same after one
   * or several calls, and therefore it is always retried. It might return a
   * status code of `kAlreadyExists` as a consequence of retrying a successful
   * (but reported as failed) request.
   *
   * @par Example: Create a Pull Subscription
   * @snippet samples.cc create-subscription
   *
   * @par Example: Create a Push Subscription
   * @snippet samples.cc create-push-subscription
   *
   * @param topic the topic that the subscription will attach to
   * @param subscription the name for the subscription
   * @param builder any additional configuration for the subscription
   */
  StatusOr<google::pubsub::v1::Subscription> CreateSubscription(
      Topic const& topic, Subscription const& subscription,
      SubscriptionBuilder builder = {}) {
    return connection_->CreateSubscription(
        {std::move(builder).BuildCreateRequest(topic, subscription)});
  }

  /**
   * Gets the metadata for an existing Cloud Pub/Sub subscription.
   *
   * @par Idempotency
   * This is a read-only operation and therefore always idempotent and retried.
   *
   * @par Example
   * @snippet samples.cc get-subscription
   */
  StatusOr<google::pubsub::v1::Subscription> GetSubscription(
      Subscription subscription) {
    return connection_->GetSubscription({std::move(subscription)});
  }

  /**
   * Updates an existing subscription in Cloud Pub/Sub.
   *
   * @par Idempotency
   * This operation is idempotent, the state of the system is the same after one
   * or several calls, and therefore it is always retried.
   *
   * @par Example
   * @snippet samples.cc update-subscription
   *
   * @param subscription the name for the subscription
   * @param builder any additional configuration for the subscription
   */
  StatusOr<google::pubsub::v1::Subscription> UpdateSubscription(
      Subscription const& subscription, SubscriptionBuilder builder) {
    return connection_->UpdateSubscription(
        {std::move(builder).BuildUpdateRequest(subscription)});
  }

  /**
   * Lists all the subscriptions for a given project id.
   *
   * @par Idempotency
   * This is a read-only operation and therefore always idempotent and retried.
   *
   * @par Example
   * @snippet samples.cc list-subscriptions
   */
  ListSubscriptionsRange ListSubscriptions(std::string const& project_id) {
    return connection_->ListSubscriptions({"projects/" + project_id});
  }

  /**
   * Deletes an existing subscription in Cloud Pub/Sub.
   *
   * @par Idempotency
   * This operation is idempotent, the state of the system is the same after one
   * or several calls, and therefore it is always retried. It might return a
   * status code of `kNotFound` as a consequence of retrying a successful
   * (but reported as failed) request.
   *
   * @par Example
   * @snippet samples.cc delete-subscription
   *
   * @param subscription the name of the subscription to be deleted.
   */
  Status DeleteSubscription(Subscription subscription) {
    return connection_->DeleteSubscription({std::move(subscription)});
  }

  /**
   * Modifies an existing subscription's push configuration.
   *
   * This can change a push subscription into a pull subscription (by setting
   * an empty push config), change the push endpoint, or change a pull
   * subscription into a push config.
   *
   * @par Idempotency
   * This operation is idempotent, the state of the system is the same after one
   * or several calls, and therefore it is always retried.
   *
   * @par Example
   * @snippet samples.cc modify-push-config
   *
   * @param subscription the name of the subscription to be modified.
   * @param builder a description of the changes to be made.
   */
  Status ModifyPushSubscription(Subscription const& subscription,
                                PushConfigBuilder builder) {
    return connection_->ModifyPushConfig(
        {std::move(builder).BuildModifyPushConfig(subscription)});
  }

  /**
   * Creates a new snapshot for a subscription with a server-assigned name.
   *
   * @par Idempotency
   * This is not an idempotent operation, repeated calls would create multiple
   * snapshots with different names assigned by the service, and therefore
   * it is never retried.
   *
   * @param subscription the name of the subscription
   * @param builder additional configuration for the snapshot, e.g., labels
   */
  // TODO(#4792) - add missing example once it is testable
  StatusOr<google::pubsub::v1::Snapshot> CreateSnapshot(
      Subscription const& subscription, SnapshotBuilder builder = {}) {
    return connection_->CreateSnapshot(
        {std::move(builder).BuildCreateRequest(subscription)});
  }

  /**
   * Creates a new snapshot for a subscription with a given name.
   *
   * @par Idempotency
   * This operation is idempotent, the state of the system is the same after one
   * or several calls, and therefore it is always retried.
   *
   * @par Example
   * @snippet samples.cc create-snapshot-with-name
   *
   * @param subscription the name of the subscription
   * @param snapshot the name of the snapshot
   * @param builder additional configuration for the snapshot, e.g., labels
   *
   * @see https://cloud.google.com/pubsub/docs/replay-overview for a detailed
   *     description of Cloud Pub/Sub's snapshots.
   */
  StatusOr<google::pubsub::v1::Snapshot> CreateSnapshot(
      Subscription const& subscription, Snapshot const& snapshot,
      SnapshotBuilder builder = {}) {
    return connection_->CreateSnapshot(
        {std::move(builder).BuildCreateRequest(subscription, snapshot)});
  }

  /**
   * Gets information about an existing snapshot.
   *
   * @par Idempotency
   * This is a read-only operation and therefore always idempotent and retried.
   *
   * @par Example
   * @snippet samples.cc get-snapshot
   *
   * @param snapshot the name of the snapshot
   *
   * @see https://cloud.google.com/pubsub/docs/replay-overview for a detailed
   *     description of Cloud Pub/Sub's snapshots.
   */
  StatusOr<google::pubsub::v1::Snapshot> GetSnapshot(Snapshot const& snapshot) {
    return connection_->GetSnapshot({snapshot});
  }

  /**
   * Updates an existing snapshot.
   *
   * @par Idempotency
   * This operation is idempotent, the state of the system is the same after one
   * or several calls, and therefore it is always retried.
   *
   * @par Example
   * @snippet samples.cc update-snapshot
   *
   * @param snapshot the name of the snapshot
   * @param builder the changes applied to the snapshot
   *
   * @see https://cloud.google.com/pubsub/docs/replay-overview for a detailed
   *     description of Cloud Pub/Sub's snapshots.
   */
  StatusOr<google::pubsub::v1::Snapshot> UpdateSnapshot(
      Snapshot const& snapshot, SnapshotBuilder builder) {
    return connection_->UpdateSnapshot(
        {std::move(builder).BuildUpdateRequest(snapshot)});
  }

  /**
   * Lists all the snapshots for a given project id.
   *
   * @par Idempotency
   * This is a read-only operation and therefore always idempotent and retried.
   *
   * @par Example
   * @snippet samples.cc list-snapshots
   *
   * @see https://cloud.google.com/pubsub/docs/replay-overview for a detailed
   *     description of Cloud Pub/Sub's snapshots.
   */
  ListSnapshotsRange ListSnapshots(std::string const& project_id) {
    return connection_->ListSnapshots({"projects/" + project_id});
  }

  /**
   * Deletes a snapshot.
   *
   * @par Idempotency
   * This operation is idempotent, the state of the system is the same after one
   * or several calls, and therefore it is always retried.
   *
   * @par Example
   * @snippet samples.cc create-snapshot-with-name
   *
   * @param snapshot the name of the snapshot
   *
   * @see https://cloud.google.com/pubsub/docs/replay-overview for a detailed
   *     description of Cloud Pub/Sub's snapshots.
   */
  Status DeleteSnapshot(Snapshot const& snapshot) {
    return connection_->DeleteSnapshot({snapshot});
  }

  /**
   * Seeks a subscription to its state at @p timestamp.
   *
   * Messages retained in the subscription that were published before
   * @p timestamp are marked as acknowledged, while messages published after
   * @p timestamp are marked as unacknowledged.
   *
   * @par Idempotency
   * This operation is idempotent, the state of the system is the same after one
   * or several calls, and therefore it is always retried.
   *
   * @par Example
   * @snippet samples.cc seek-with-timestamp
   *
   * @see https://cloud.google.com/pubsub/docs/replay-overview for a detailed
   *     description of Cloud Pub/Sub's `Seek()` functionality.
   */
  StatusOr<google::pubsub::v1::SeekResponse> Seek(
      Subscription const& subscription,
      std::chrono::system_clock::time_point timestamp);

  /**
   * Seeks a subscription to its state at @p snapshot.
   *
   * @par Idempotency
   * This operation is idempotent, the state of the system is the same after one
   * or several calls, and therefore it is always retried.
   *
   * @par Example
   * @snippet samples.cc seek-with-timestamp
   *
   * @see https://cloud.google.com/pubsub/docs/replay-overview for a detailed
   *     description of Cloud Pub/Sub's `Seek()` functionality.
   */
  StatusOr<google::pubsub::v1::SeekResponse> Seek(
      Subscription const& subscription, Snapshot const& snapshot);

 private:
  std::shared_ptr<SubscriptionAdminConnection> connection_;
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SUBSCRIPTION_ADMIN_CLIENT_H
