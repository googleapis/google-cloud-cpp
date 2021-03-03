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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_PUBLISHER_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_PUBLISHER_CONNECTION_H

#include "google/cloud/pubsub/backoff_policy.h"
#include "google/cloud/pubsub/connection_options.h"
#include "google/cloud/pubsub/internal/publisher_stub.h"
#include "google/cloud/pubsub/message.h"
#include "google/cloud/pubsub/publisher_options.h"
#include "google/cloud/pubsub/retry_policy.h"
#include "google/cloud/pubsub/topic.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/future.h"
#include "google/cloud/status_or.h"
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

/**
 * A connection to the Cloud Pub/Sub service to publish events.
 *
 * This interface defines pure-virtual methods for each of the user-facing
 * overload sets in `Publisher`. That is, all of `Publisher`'s overloads will
 * forward to the one pure-virtual method declared in this interface. This
 * allows users to inject custom behavior (e.g., with a Google Mock object) in a
 * `Publisher` object for use in their own tests.
 *
 * To create a concrete instance that connects you to the real Cloud Pub/Sub
 * service, see `MakePublisherConnection()`.
 *
 * @par The *Params nested classes
 * Applications may define classes derived from `PublisherConnection`, for
 * example, because they want to mock the class. To avoid breaking all such
 * derived classes when we change the number or type of the arguments to the
 * member functions we define lightweight structures to pass the arguments.
 */
class PublisherConnection {
 public:
  virtual ~PublisherConnection() = 0;

  /// Wrap the arguments for `Publish()`
  struct PublishParams {
    Message message;
  };

  /// Wrap the arguments for `Flush()`
  struct FlushParams {};

  /// Wrap the arguments for `ResumePublish()`
  struct ResumePublishParams {
    std::string ordering_key;
  };

  /// Defines the interface for `Publisher::Publish()`
  virtual future<StatusOr<std::string>> Publish(PublishParams p);

  /// Defines the interface for `Publisher::Flush()`
  virtual void Flush(FlushParams);

  /// Defines the interface for `Publisher::ResumePublish()`
  virtual void ResumePublish(ResumePublishParams p);
};

/**
 * Creates a new `PublisherConnection` object to work with `Publisher`.
 *
 * The `PublisherConnection` class is not intended for direct use in
 * applications, it is provided for applications wanting to mock the
 * `PublisherClient` behavior in their tests.
 *
 * @par Performance
 * Creating a new `PublisherConnection` is relatively expensive. This typically
 * initiate connections to the service, and therefore these objects should be
 * shared and reused when possible. Note that gRPC reuses existing OS resources
 * (sockets) whenever possible, so applications may experience better
 * performance on the second (and subsequent) calls to this function with the
 * same `ConnectionOptions` parameters. However, this behavior is not guaranteed
 * and applications should not rely on it.
 *
 * @see `PublisherConnection`
 *
 * @param topic the Cloud Pub/Sub topic used by the returned
 *     `PublisherConnection`.
 * @param options configure the batching policy and other parameters in the
 *     returned connection.
 * @param connection_options (optional) general configuration for this
 *    connection, this type is also used to configure `pubsub::Subscriber`.
 * @param retry_policy (optional) configure the retry loop. This is only used
 *    if `retry_publish_failures()` is enabled in @p options.
 * @param backoff_policy (optional) configure the backoff period between
 *    retries. This is only used if `retry_publish_failures()` is enabled in
 *    @p options.
 */
std::shared_ptr<PublisherConnection> MakePublisherConnection(
    Topic topic, PublisherOptions options,
    ConnectionOptions connection_options = {},
    std::unique_ptr<RetryPolicy const> retry_policy = {},
    std::unique_ptr<BackoffPolicy const> backoff_policy = {});

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub

namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

std::shared_ptr<pubsub::PublisherConnection> MakePublisherConnection(
    pubsub::Topic topic, pubsub::PublisherOptions options,
    pubsub::ConnectionOptions connection_options,
    std::shared_ptr<PublisherStub> stub,
    std::unique_ptr<pubsub::RetryPolicy const> retry_policy,
    std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy);

std::shared_ptr<pubsub::PublisherConnection> MakePublisherConnection(
    pubsub::Topic topic, pubsub::PublisherOptions options,
    pubsub::ConnectionOptions connection_options,
    std::vector<std::shared_ptr<PublisherStub>> stubs,
    std::unique_ptr<pubsub::RetryPolicy const> retry_policy,
    std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy);

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_PUBLISHER_CONNECTION_H
