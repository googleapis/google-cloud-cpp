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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TOPIC_ADMIN_CLIENT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TOPIC_ADMIN_CLIENT_H

#include "google/cloud/pubsub/snapshot.h"
#include "google/cloud/pubsub/topic_admin_connection.h"
#include "google/cloud/pubsub/topic_mutation_builder.h"
#include "google/cloud/pubsub/version.h"
#include <memory>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

/**
 * Performs publisher operations in Cloud Pub/Sub.
 *
 * Applications use this class to perform operations on
 * [Cloud Pub/Sub][pubsub-doc-link].
 *
 * @par Performance
 *
 * `PublisherClient` objects are cheap to create, copy, and move. However,
 * each `PublisherClient` object must be created with a
 * `std::shared_ptr<PublisherConnection>`, which itself is relatively
 * expensive to create. Therefore, connection instances should be shared when
 * possible. See the `MakeTopicAdminConnection()` function and the
 * `PublisherConnection` interface for more details.
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
class TopicAdminClient {
 public:
  explicit TopicAdminClient(std::shared_ptr<TopicAdminConnection> connection);

  /**
   * The default constructor is deleted.
   *
   * Use `PublisherClient(std::shared_ptr<PublisherConnection>)`
   */
  TopicAdminClient() = delete;

  /**
   * Create a new topic in Cloud Pub/Sub.
   *
   * @par Idempotency
   * This operation is idempotent, as it succeeds only once, therefore the
   * library retries the call. It might return a status code of
   * `kAlreadyExists` if successful more than one time.
   *
   * @par Example
   * @snippet samples.cc create-topic
   *
   * @param builder the configuration for the new topic, includes the name.
   */
  StatusOr<google::pubsub::v1::Topic> CreateTopic(
      TopicMutationBuilder builder) {
    return connection_->CreateTopic({std::move(builder).BuildCreateMutation()});
  }

  /**
   * Get information about an existing Cloud Pub/Sub topic.
   *
   * @par Idempotency
   * This is a read-only operation and therefore always idempotent and retried.
   *
   * @par Example
   * @snippet samples.cc get-topic
   */
  StatusOr<google::pubsub::v1::Topic> GetTopic(Topic topic) {
    return connection_->GetTopic({std::move(topic)});
  }

  /**
   * Update the configuration of an existing Cloud Pub/Sub topic.
   *
   * @par Idempotency
   * This operation is idempotent, the state of the system is the same after one
   * or several calls, and thefeore it is always retried.
   *
   * @par Example
   * @snippet samples.cc update-topic
   *
   * @param builder the configuration for the new topic, includes the name.
   */
  StatusOr<google::pubsub::v1::Topic> UpdateTopic(
      TopicMutationBuilder builder) {
    return connection_->UpdateTopic({std::move(builder).BuildUpdateMutation()});
  }

  /**
   * List all the topics for a given project id.
   *
   * @par Idempotency
   * This is a read-only operation and therefore always idempotent and retried.
   *
   * @par Example
   * @snippet samples.cc list-topics
   */
  ListTopicsRange ListTopics(std::string const& project_id) {
    return connection_->ListTopics({"projects/" + project_id});
  }

  /**
   * Delete an existing topic in Cloud Pub/Sub.
   *
   * @par Idempotency
   * This operation is idempotent, the state of the system is the same after one
   * or several calls, and therefore it is always retried.
   *
   * @par Example
   * @snippet samples.cc delete-topic
   *
   * @param topic the name of the topic to be deleted.
   */
  Status DeleteTopic(Topic topic) {
    return connection_->DeleteTopic({std::move(topic)});
  }

  /**
   * Detaches an existing subscription.
   *
   * This operation stops the subscription from receiving any further messages,
   * it drops any messages still retained by the subscription, and any
   * outstanding pull requests will fail with `FAILED_PRECONDITION`.
   *
   * @warning this feature is **not GA** and may require registering your
   *   project in an early access program.
   *
   * @par Idempotency
   * This operation is idempotent, the state of the system is the same after one
   * or several calls, and therefore it is always retried.
   *
   * @par Example
   * @snippet samples.cc detach-subscription
   *
   * @param subscription the name of the subscription to detach.
   */
  StatusOr<google::pubsub::v1::DetachSubscriptionResponse> DetachSubscription(
      Subscription subscription) {
    return connection_->DetachSubscription({std::move(subscription)});
  }

  /**
   * List all the subscription names for a given topic.
   *
   * @note
   * The returned range contains fully qualified subscription names, e.g.,
   * `"projects/my-project/subscriptions/my-subscription"`. Applications may
   * need to parse these names to use with other APIs.
   *
   * @par Idempotency
   * This is a read-only operation and therefore always idempotent and retried.
   *
   * @par Example
   * @snippet samples.cc list-topic-subscriptions
   */
  ListTopicSubscriptionsRange ListTopicSubscriptions(Topic const& topic) {
    return connection_->ListTopicSubscriptions({topic.FullName()});
  }

  /**
   * List all the subscription names for a given topic.
   *
   * @note
   * The returned range contains fully qualified snapshot names, e.g.,
   * `"projects/my-project/snapshots/my-subscription"`. Applications may
   * need to parse these names to use with other APIs.
   *
   * @par Idempotency
   * This is a read-only operation and therefore always idempotent and retried.
   *
   * @par Example
   * @snippet samples.cc list-topic-snapshots
   */
  ListTopicSnapshotsRange ListTopicSnapshots(Topic const& topic) {
    return connection_->ListTopicSnapshots({topic.FullName()});
  }

 private:
  std::shared_ptr<TopicAdminConnection> connection_;
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TOPIC_ADMIN_CLIENT_H
