// Copyright 2022 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_BLOCKING_PUBLISHER_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_BLOCKING_PUBLISHER_CONNECTION_H

#include "google/cloud/pubsub/backoff_policy.h"
#include "google/cloud/pubsub/connection_options.h"
#include "google/cloud/pubsub/internal/publisher_stub.h"
#include "google/cloud/pubsub/message.h"
#include "google/cloud/pubsub/publisher_options.h"
#include "google/cloud/pubsub/retry_policy.h"
#include "google/cloud/pubsub/topic.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/non_constructible.h"
#include "google/cloud/status_or.h"
#include <initializer_list>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace pubsub {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A connection to the Cloud Pub/Sub service to publish events.
 *
 * This interface defines pure-virtual methods for each of the user-facing
 * overload sets in `BlockingPublisher`. That is, all of `BlockingPublisher`'s
 * overloads will forward to the one pure-virtual method declared in this
 * interface. This allows users to inject custom behavior (e.g., with a Google
 * Mock object) in a `BlockingPublisher` object for use in their own tests.
 *
 * To create a concrete instance that connects you to the real Cloud Pub/Sub
 * service, see `MakeBlockingPublisherConnection()`.
 *
 * @par The *Params nested classes
 * Applications may define classes derived from `PublisherConnection`, for
 * example, because they want to mock the class. To avoid breaking all such
 * derived classes when we change the number or type of the arguments to the
 * member functions we define lightweight structures to pass the arguments.
 */
class BlockingPublisherConnection {
 public:
  virtual ~BlockingPublisherConnection() = 0;

  /// Wrap the arguments for `Publish()`
  struct PublishParams {
    Topic topic;
    Message message;
  };

  /// Defines the interface for `BlockingPublisher::Publish()`
  virtual StatusOr<std::string> Publish(PublishParams p);

  /// Returns the options configured at initialization time.
  virtual Options options() { return Options{}; }
};

/**
 * Creates a new `BlockingPublisherConnection` object to work with
 * `BlockingPublisher`.
 *
 * The `BlockingPublisherConnection` class is provided for applications wanting
 * to mock the `BlockingPublisher` behavior in their tests. It is not intended
 * for direct use.
 *
 * @par Performance
 * Creating a new `BlockingPublisherConnection` is relatively expensive. This
 * typically initiates connections to the service, and therefore these objects
 * should be shared and reused when possible. Note that gRPC reuses existing OS
 * resources (sockets) whenever possible, so applications may experience better
 * performance on the second (and subsequent) calls to this function with the
 * same `Options` from `GrpcOptionList` and `CommonOptionList`. However, this
 * behavior is not guaranteed and applications should not rely on it.
 *
 * @see `BlockingPublisherConnection`
 *
 * @param opts The options to use for this call. Expected options are any of
 *     the types in the following option lists.
 *       - `google::cloud::CommonOptionList`
 *       - `google::cloud::GrpcOptionList`
 *       - `google::cloud::pubsub::PolicyOptionList`
 *       - `google::cloud::pubsub::PublisherOptionList`
 */
std::shared_ptr<BlockingPublisherConnection> MakeBlockingPublisherConnection(
    Options opts = {});

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_BLOCKING_PUBLISHER_CONNECTION_H
