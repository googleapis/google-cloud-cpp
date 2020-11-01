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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SUBSCRIPTION_ADMIN_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SUBSCRIPTION_ADMIN_CONNECTION_H

#include "google/cloud/pubsub/backoff_policy.h"
#include "google/cloud/pubsub/connection_options.h"
#include "google/cloud/pubsub/internal/subscriber_stub.h"
#include "google/cloud/pubsub/retry_policy.h"
#include "google/cloud/pubsub/snapshot.h"
#include "google/cloud/pubsub/subscription.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/internal/pagination_range.h"
#include "google/cloud/status_or.h"
#include <google/pubsub/v1/pubsub.pb.h>
#include <memory>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

/**
 * An input range to stream Cloud Pub/Sub subscriptions.
 *
 * This type models an [input range][cppref-input-range] of
 * `google::pubsub::v1::Subscription` objects. Applications can make a
 * single pass through the results.
 *
 * [cppref-input-range]: https://en.cppreference.com/w/cpp/ranges/input_range
 */
using ListSubscriptionsRange = google::cloud::internal::PaginationRange<
    google::pubsub::v1::Subscription,
    google::pubsub::v1::ListSubscriptionsRequest,
    google::pubsub::v1::ListSubscriptionsResponse>;

/**
 * An input range to stream Cloud Pub/Sub snapshots.
 *
 * This type models an [input range][cppref-input-range] of
 * `google::pubsub::v1::Snapshot` objects. Applications can make a
 * single pass through the results.
 *
 * [cppref-input-range]: https://en.cppreference.com/w/cpp/ranges/input_range
 */
using ListSnapshotsRange = google::cloud::internal::PaginationRange<
    google::pubsub::v1::Snapshot, google::pubsub::v1::ListSnapshotsRequest,
    google::pubsub::v1::ListSnapshotsResponse>;

/**
 * A connection to Cloud Pub/Sub for subscription-related administrative
 * operations.
 *
 * This interface defines pure-virtual methods for each of the user-facing
 * overload sets in `SubscriberAdminClient`. That is, all of
 * `SubscriberAdminClient`'s `CreateSubscription()` overloads will forward to
 * the one pure-virtual `CreateSubscription()` method declared in this
 * interface, and similar for `SubscriberClient`'s other methods. This allows
 * users to inject custom behavior (e.g., with a Google Mock object) in a
 * `SubscriberClient` object for use in their own tests.
 *
 * To create a concrete instance that connects you to the real Cloud Pub/Sub
 * service, see `MakeSubscriptionAdminConnection()`.
 *
 * @par The *Params nested classes
 * Applications may define classes derived from `SubscriptionAdminConnection`,
 * for example, because they want to mock the class. To avoid breaking all such
 * derived classes when we change the number or type of the arguments to the
 * member functions we define lightweight structures to pass the arguments.
 */
class SubscriptionAdminConnection {
 public:
  virtual ~SubscriptionAdminConnection() = 0;

  /// Wrap the arguments for `CreateSubscription()`
  struct CreateSubscriptionParams {
    google::pubsub::v1::Subscription subscription;
  };

  /// Wrap the arguments for `GetSubscription()`
  struct GetSubscriptionParams {
    Subscription subscription;
  };

  /// Wrap the arguments for `UpdateSubscription()`
  struct UpdateSubscriptionParams {
    google::pubsub::v1::UpdateSubscriptionRequest request;
  };

  /// Wrap the arguments for `ListSubscriptions()`
  struct ListSubscriptionsParams {
    std::string project_id;
  };

  /// Wrap the arguments for `DeleteSubscription()`
  struct DeleteSubscriptionParams {
    Subscription subscription;
  };

  /// Wrap the arguments for `ModifyPushConfig()`
  struct ModifyPushConfigParams {
    google::pubsub::v1::ModifyPushConfigRequest request;
  };

  /// Wrap the arguments for `CreateSnapshot()`
  struct CreateSnapshotParams {
    google::pubsub::v1::CreateSnapshotRequest request;
  };

  /// Wrap the arguments for `GetSnapshot()`
  struct GetSnapshotParams {
    Snapshot snapshot;
  };

  /// Wrap the arguments for `ListSnapshots()`
  struct ListSnapshotsParams {
    std::string project_id;
  };

  /// Wrap the arguments for `UpdateSnapshot()`
  struct UpdateSnapshotParams {
    google::pubsub::v1::UpdateSnapshotRequest request;
  };

  /// Wrap the arguments for `DeleteSnapshot()`
  struct DeleteSnapshotParams {
    Snapshot snapshot;
  };

  /// Wrap the arguments for `Seek()`
  struct SeekParams {
    google::pubsub::v1::SeekRequest request;
  };

  /// Defines the interface for `SubscriptionAdminClient::CreateSubscription()`
  virtual StatusOr<google::pubsub::v1::Subscription> CreateSubscription(
      CreateSubscriptionParams);

  /// Defines the interface for `SubscriptionAdminClient::GetSubscription()`
  virtual StatusOr<google::pubsub::v1::Subscription> GetSubscription(
      GetSubscriptionParams);

  /// Defines the interface for `SubscriptionAdminClient::UpdateSubscription()`
  virtual StatusOr<google::pubsub::v1::Subscription> UpdateSubscription(
      UpdateSubscriptionParams);

  /// Defines the interface for `SubscriptionAdminClient::ListSubscriptions()`
  virtual ListSubscriptionsRange ListSubscriptions(ListSubscriptionsParams);

  /// Defines the interface for `SubscriptionAdminClient::DeleteSubscription()`
  virtual Status DeleteSubscription(DeleteSubscriptionParams);

  /// Defines the interface for `SubscriptionAdminClient::ModifyPushConfig()`
  virtual Status ModifyPushConfig(ModifyPushConfigParams);

  /// Defines the interface for `SnapshotAdminClient::CreateSnapshot()`
  virtual StatusOr<google::pubsub::v1::Snapshot> CreateSnapshot(
      CreateSnapshotParams);

  /// Defines the interface for `SnapshotAdminClient::GetSnapshot()`
  virtual StatusOr<google::pubsub::v1::Snapshot> GetSnapshot(GetSnapshotParams);

  /// Defines the interface for `SnapshotAdminClient::UpdateSnapshot()`
  virtual StatusOr<google::pubsub::v1::Snapshot> UpdateSnapshot(
      UpdateSnapshotParams);

  /// Defines the interface for `SubscriptionAdminClient::ListSnapshots()`
  virtual ListSnapshotsRange ListSnapshots(ListSnapshotsParams);

  /// Defines the interface for `SnapshotAdminClient::DeleteSnapshot()`
  virtual Status DeleteSnapshot(DeleteSnapshotParams);

  /// Defines the interface for `SubscriptionAdminClient::Seek()`
  virtual StatusOr<google::pubsub::v1::SeekResponse> Seek(SeekParams);
};

/**
 * Returns an SubscriberConnection object to work with Cloud Pub/Sub subscriber
 * APIs.
 *
 * The `SubscriberConnection` class is not intended for direct use in
 * applications, it is provided for applications wanting to mock the
 * `SubscriberClient` behavior in their tests.
 *
 * @par Performance
 * Creating a new `SubscriptionAdminConnection` is relatively expensive. This
 * typically initiate connections to the service, and therefore these objects
 * should be shared and reused when possible. Note that gRPC reuses existing OS
 * resources (sockets) whenever possible, so applications may experience better
 * performance on the second (and subsequent) calls to this function with the
 * same `ConnectionOptions` parameters. However, this behavior is not guaranteed
 * and applications should not rely on it.
 *
 * @see `SubscriberConnection`
 *
 * @param options (optional) configure the `SubscriberConnection` created by
 *     this function.
 * @param retry_policy control for how long (or how many times) are retryable
 *     RPCs attempted.
 * @param backoff_policy controls the backoff behavior between retry attempts,
 *     typically some form of exponential backoff with jitter.
 */
std::shared_ptr<SubscriptionAdminConnection> MakeSubscriptionAdminConnection(
    ConnectionOptions const& options = ConnectionOptions(),
    std::unique_ptr<pubsub::RetryPolicy const> retry_policy = {},
    std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy = {});

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub

namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

std::shared_ptr<pubsub::SubscriptionAdminConnection>
MakeSubscriptionAdminConnection(
    pubsub::ConnectionOptions const& options,
    std::shared_ptr<SubscriberStub> stub,
    std::unique_ptr<pubsub::RetryPolicy const> retry_policy,
    std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy);

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SUBSCRIPTION_ADMIN_CONNECTION_H
