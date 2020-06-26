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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SUBSCRIBER_CLIENT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SUBSCRIBER_CLIENT_H

#include "google/cloud/pubsub/create_subscription_builder.h"
#include "google/cloud/pubsub/subscriber_connection.h"
#include "google/cloud/pubsub/version.h"
#include <memory>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

/**
 * Performs subscriber operations in Cloud Pub/Sub.
 *
 * Applications use this class to perform operations on
 * [Cloud Pub/Sub][pubsub-doc-link].
 *
 * @par Performance
 *
 * `SubscriberClient` objects are relatively cheap to create, copy, and move.
 * However, each `SubscriberClient` object must be created with a
 * `std::shared_ptr<SubscriberConnection>`, which itself is relatively
 * expensive to create. Therefore, connection instances should be shared when
 * possible. See the `MakeSubscriberConnection()` function and the
 * `SubscriberConnection` interface for more details.
 *
 * @par Thread Safety
 *
 * Instances of this class created via copy-construction or copy-assignment
 * share the underlying pool of connections. Access to these copies via multiple
 * threads is guaranteed to work. Two threads operating on the same instance of
 * this class is not guaranteed to work.
 *
 * @par Error Handling
 *
 * This class uses `StatusOr<T>` to report errors. When an operation fails to
 * perform its work the returned `StatusOr<T>` contains the error details. If
 * the `ok()` member function in the `StatusOr<T>` returns `true` then it
 * contains the expected result. Please consult the
 * [`StatusOr<T>` documentation](#google::cloud::v1::StatusOr) for more details.
 *
 * [pubsub-doc-link]: https://cloud.google.com/pubsub/docs
 */
class SubscriberClient {
 public:
  explicit SubscriberClient(std::shared_ptr<SubscriberConnection> connection);

  /**
   * The default constructor is deleted.
   *
   * Use `SubscriberClient(std::shared_ptr<SubscriberConnection>)`
   */
  SubscriberClient() = delete;

  /**
   * Create a new subscription in Cloud Pub/Sub.
   *
   * @par Idempotency
   * This is not an idempotent operation and therefore it is never retried.
   *
   * @par Example
   * @snippet samples.cc create-subscription
   *
   * @param builder the configuration for the new subscription.
   */
  StatusOr<google::pubsub::v1::Subscription> CreateSubscription(
      CreateSubscriptionBuilder builder) {
    return connection_->CreateSubscription({std::move(builder).as_proto()});
  }

  /**
   * List all the subscriptions for a given project id.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always treated as
   * idempotent.
   *
   * @par Example
   * @snippet samples.cc list-subscriptions
   */
  ListSubscriptionsRange ListSubscriptions(std::string const& project_id) {
    return connection_->ListSubscriptions({"projects/" + project_id});
  }

  /**
   * Delete an existing subscription in Cloud Pub/Sub.
   *
   * @par Idempotency
   * This is not an idempotent operation and therefore it is never retried.
   *
   * @par Example
   * @snippet samples.cc delete-subscription
   *
   * @param subscription the name of the subscription to be deleted.
   */
  Status DeleteSubscription(Subscription subscription) {
    return connection_->DeleteSubscription({std::move(subscription)});
  }

 private:
  std::shared_ptr<SubscriberConnection> connection_;
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SUBSCRIBER_CLIENT_H
