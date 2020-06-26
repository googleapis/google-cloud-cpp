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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SUBSCRIBER_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SUBSCRIBER_CONNECTION_H

#include "google/cloud/pubsub/connection_options.h"
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
 * A connection to Cloud Pub/Sub for subscriber operations.
 *
 * This interface defines pure-virtual methods for each of the user-facing
 * overload sets in `SubscriberClient`. That is, all of `SubscriberClient`'s
 * `CreateSubscription()` overloads will forward to the one pure-virtual
 * `CreateSubscription()` method declared in this interface, and similar for
 * `SubscriberClient`'s other methods. This allows users to inject custom
 * behavior (e.g., with a Google Mock object) in a `SubscriberClient` object for
 * use in their own tests.
 *
 * To create a concrete instance that connects you to the real Cloud Pub/Sub
 * service, see `MakeSubscriberConnection()`.
 */
class SubscriberConnection {
 public:
  virtual ~SubscriberConnection() = 0;

  //@{
  /**
   * Define the arguments for each member function.
   *
   * Applications may define classes derived from `SubscriberConnection`, for
   * example, because they want to mock the class. To avoid breaking all such
   * derived classes when we change the number or type of the arguments to the
   * member functions we define light weight structures to pass the arguments.
   */
  /// Wrap the arguments for `CreateSubscription()`
  struct CreateSubscriptionParams {
    google::pubsub::v1::Subscription subscription;
  };

  struct ListSubscriptionsParams {
    std::string project_id;
  };

  /// Wrap the arguments for `DeleteSubscription()`
  struct DeleteSubscriptionParams {
    Subscription subscription;
  };
  //@}

  /// Defines the interface for `Client::CreateSubscription()`
  virtual StatusOr<google::pubsub::v1::Subscription> CreateSubscription(
      CreateSubscriptionParams) = 0;

  /// Defines the interface for `Client::ListSubscriptions()`
  virtual ListSubscriptionsRange ListSubscriptions(ListSubscriptionsParams) = 0;

  /// Defines the interface for `Client::DeleteSubscription()`
  virtual Status DeleteSubscription(DeleteSubscriptionParams) = 0;
};

/**
 * Returns an SubscriberConnection object to work with Cloud Pub/Sub subscriber
 * APIs.
 *
 * The `SubscriberConnection` class is not intended for direct use in
 * applications, it is provided for applications wanting to mock the
 * `SubscriberClient` behavior in their tests.
 *
 * @see `SubscriberConnection`
 *
 * @param options (optional) configure the `SubscriberConnection` created by
 *     this function.
 */
std::shared_ptr<SubscriberConnection> MakeSubscriberConnection(
    ConnectionOptions const& options = ConnectionOptions());

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SUBSCRIBER_CONNECTION_H
