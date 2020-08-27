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

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

/**
 * A connection to the Cloud Pub/Sub service to publish events.
 */
class PublisherConnection {
 public:
  virtual ~PublisherConnection() = 0;

  struct PublishParams {
    Message message;
  };
  virtual future<StatusOr<std::string>> Publish(PublishParams p) = 0;

  struct FlushParams {};
  virtual void Flush(FlushParams) = 0;
};

/**
 * Returns a `PublisherConnection` object to work with Cloud Pub/Sub publisher
 * APIs.
 *
 * The `PublisherConnection` class is not intended for direct use in
 * applications, it is provided for applications wanting to mock the
 * `PublisherClient` behavior in their tests.
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

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_PUBLISHER_CONNECTION_H
