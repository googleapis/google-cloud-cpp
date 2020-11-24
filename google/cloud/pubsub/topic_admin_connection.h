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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TOPIC_ADMIN_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TOPIC_ADMIN_CONNECTION_H

#include "google/cloud/pubsub/backoff_policy.h"
#include "google/cloud/pubsub/connection_options.h"
#include "google/cloud/pubsub/internal/publisher_stub.h"
#include "google/cloud/pubsub/retry_policy.h"
#include "google/cloud/pubsub/subscription.h"
#include "google/cloud/pubsub/topic.h"
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
 * An input range to stream Cloud Pub/Sub topics.
 *
 * This type models an [input range][cppref-input-range] of
 * `google::pubsub::v1::Topic` objects. Applications can make a
 * single pass through the results.
 *
 * [cppref-input-range]: https://en.cppreference.com/w/cpp/ranges/input_range
 */
using ListTopicsRange =
    google::cloud::internal::PaginationRange<google::pubsub::v1::Topic>;

/**
 * An input range to stream the Cloud Pub/Sub subscriptions of a topic.
 *
 * This type models an [input range][cppref-input-range] of
 * `std::string` objects. Applications can make a
 * single pass through the results.
 *
 * [cppref-input-range]: https://en.cppreference.com/w/cpp/ranges/input_range
 */
using ListTopicSubscriptionsRange =
    google::cloud::internal::PaginationRange<std::string>;

/**
 * An input range to stream the Cloud Pub/Sub snapshots of a topic.
 *
 * This type models an [input range][cppref-input-range] of
 * `std::string` objects. Applications can make a
 * single pass through the results.
 *
 * [cppref-input-range]: https://en.cppreference.com/w/cpp/ranges/input_range
 */
using ListTopicSnapshotsRange =
    google::cloud::internal::PaginationRange<std::string>;

/**
 * A connection to Cloud Pub/Sub for topic-related administrative operations.
 *
 * This interface defines pure-virtual methods for each of the user-facing
 * overload sets in `TopicAdminClient`. That is, all of `TopicAdminClient`'s
 * overloads will forward to the one pure-virtual method declared in this
 * interface. This allows users to inject custom behavior (e.g., with a Google
 * Mock object) in a `TopicAdminClient` object for use in their own tests.
 *
 * To create a concrete instance that connects you to the real Cloud Pub/Sub
 * service, see `MakeTopicAdminConnection()`.
 *
 * @par The *Params nested classes
 * Applications may define classes derived from `TopicAdminConnection`, for
 * example, because they want to mock the class. To avoid breaking all such
 * derived classes when we change the number or type of the arguments to the
 * member functions we define lightweight structures to pass the arguments.
 */
class TopicAdminConnection {
 public:
  virtual ~TopicAdminConnection() = 0;

  /// Wrap the arguments for `CreateTopic()`
  struct CreateTopicParams {
    google::pubsub::v1::Topic topic;
  };

  /// Wrap the arguments for `GetTopic()`
  struct GetTopicParams {
    Topic topic;
  };

  /// Wrap the arguments for `UpdateTopic()`
  struct UpdateTopicParams {
    google::pubsub::v1::UpdateTopicRequest request;
  };

  /// Wrap the arguments for `ListTopics()`
  struct ListTopicsParams {
    std::string project_id;
  };

  /// Wrap the arguments for `DeleteTopic()`
  struct DeleteTopicParams {
    Topic topic;
  };

  /// Wrap the arguments for `DetachSubscription()`
  struct DetachSubscriptionParams {
    Subscription subscription;
  };

  /// Wrap the arguments for `ListTopicSubscriptions()`
  struct ListTopicSubscriptionsParams {
    std::string topic_full_name;
  };

  /// Wrap the arguments for `ListTopicSnapshots()`
  struct ListTopicSnapshotsParams {
    std::string topic_full_name;
  };

  /// Defines the interface for `TopicAdminClient::CreateTopic()`
  virtual StatusOr<google::pubsub::v1::Topic> CreateTopic(CreateTopicParams);

  /// Defines the interface for `TopicAdminClient::GetTopic()`
  virtual StatusOr<google::pubsub::v1::Topic> GetTopic(GetTopicParams);

  /// Defines the interface for `TopicAdminClient::UpdateTopic()`
  virtual StatusOr<google::pubsub::v1::Topic> UpdateTopic(UpdateTopicParams);

  /// Defines the interface for `TopicAdminClient::ListTopics()`
  virtual ListTopicsRange ListTopics(ListTopicsParams);

  /// Defines the interface for `TopicAdminClient::DeleteTopic()`
  virtual Status DeleteTopic(DeleteTopicParams);

  /// Defines the interface for `TopicAdminClient::DetachSubscriptions()`
  virtual StatusOr<google::pubsub::v1::DetachSubscriptionResponse>
      DetachSubscription(DetachSubscriptionParams);

  /// Defines the interface for `TopicAdminClient::ListTopicSubscriptions()`
  virtual ListTopicSubscriptionsRange ListTopicSubscriptions(
      ListTopicSubscriptionsParams);

  /// Defines the interface for `TopicAdminClient::ListTopicSnapshots()`
  virtual ListTopicSnapshotsRange ListTopicSnapshots(ListTopicSnapshotsParams);
};

/**
 * Creates a new `TopicAdminConnection` object to work with `TopicAdminClient`.
 *
 * The `TopicAdminConnection` class is provided for applications wanting to mock
 * the `TopicAdminClient` behavior in their tests. It is not intended for direct
 * use.
 *
 * @par Performance
 * Creating a new `TopicAdminConnection` is relatively expensive. This typically
 * initiate connections to the service, and therefore these objects should be
 * shared and reused when possible. Note that gRPC reuses existing OS resources
 * (sockets) whenever possible, so applications may experience better
 * performance on the second (and subsequent) calls to this function with the
 * same `ConnectionOptions` parameters. However, this behavior is not guaranteed
 * and applications should not rely on it.
 *
 * @see `TopicAdminClient`
 *
 * @param options (optional) configure the `PublisherConnection` created by
 *     this function.
 * @param retry_policy control for how long (or how many times) are retryable
 *     RPCs attempted.
 * @param backoff_policy controls the backoff behavior between retry attempts,
 *     typically some form of exponential backoff with jitter.
 */
std::shared_ptr<TopicAdminConnection> MakeTopicAdminConnection(
    ConnectionOptions const& options = ConnectionOptions(),
    std::unique_ptr<pubsub::RetryPolicy const> retry_policy = {},
    std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy = {});

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub

namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

std::shared_ptr<pubsub::TopicAdminConnection> MakeTopicAdminConnection(
    pubsub::ConnectionOptions const& options,
    std::shared_ptr<PublisherStub> stub,
    std::unique_ptr<pubsub::RetryPolicy const> retry_policy,
    std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy);

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TOPIC_ADMIN_CONNECTION_H
