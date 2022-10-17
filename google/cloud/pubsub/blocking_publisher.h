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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_BLOCKING_PUBLISHER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_BLOCKING_PUBLISHER_H

#include "google/cloud/pubsub/blocking_publisher_connection.h"
#include "google/cloud/pubsub/publisher_options.h"
#include "google/cloud/pubsub/version.h"
#include <string>

namespace google {
namespace cloud {
namespace pubsub {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
/**
 * Publish messages to the Cloud Pub/Sub service.
 *
 * This class is used to publish messages to any given topic. It is intended
 * for low-volume publishers: applications sending less than one message per
 * second may find this class easier to use than `Publisher`, which can handle
 * thousands of messages per second.
 *
 * @see https://cloud.google.com/pubsub for an overview of the Cloud Pub/Sub
 *   service.
 *
 * @par Example
 * @snippet blocking_samples.cc blocking-publish
 *
 * @par Performance
 * `BlockingPublisher` objects are relatively cheap to create, copy, and move.
 * However, each `BlockingPublisher` object must be created with a
 * `std::shared_ptr<BlockingPublisherConnection>`, which itself is relatively
 * expensive to create. Therefore, connection instances should be shared when
 * possible. See the `MakeBlockingPublisherConnection()` method and the
 * `BlockingPublisherConnection` interface for more details.
 *
 * @par Thread Safety
 * Instances of this class created via copy-construction or copy-assignment
 * share the underlying pool of connections. Access to these copies via multiple
 * threads is guaranteed to work. Two threads operating on the same instance of
 * this class is not guaranteed to work.
 *
 * @par Background Threads
 * This class uses the background threads configured via the `Options` from
 * `GrpcOptionList`. Applications can create their own pool of background
 * threads by (a) creating their own #google::cloud::CompletionQueue, (b)
 * passing this completion queue as a `GrpcCompletionQueueOption`, and (c)
 * attaching any number of threads to the completion queue.
 *
 * @par Error Handling
 * This class uses `StatusOr<T>` to report errors. When an operation fails to
 * perform its work the returned `StatusOr<T>` contains the error details. If
 * the `ok()` member function in the `StatusOr<T>` returns `true` then it
 * contains the expected result. Please consult the #google::cloud::StatusOr
 * documentation for more details.
 *
 */
class BlockingPublisher {
 public:
  explicit BlockingPublisher(
      std::shared_ptr<BlockingPublisherConnection> connection,
      Options opts = {});

  BlockingPublisher(BlockingPublisher const&) = default;
  BlockingPublisher& operator=(BlockingPublisher const&) = default;
  BlockingPublisher(BlockingPublisher&&) = default;
  BlockingPublisher& operator=(BlockingPublisher&&) = default;

  friend bool operator==(BlockingPublisher const& a,
                         BlockingPublisher const& b) {
    return a.connection_ == b.connection_;
  }
  friend bool operator!=(BlockingPublisher const& a,
                         BlockingPublisher const& b) {
    return !(a == b);
  }

  /**
   * Publishes the @p message on the topic @p topic.
   *
   * @par Idempotency
   * This is a non-idempotent operation, but the client library will
   * automatically retry RPCs that fail with transient errors. As Cloud Pub/Sub
   * has "at least once" delivery semantics applications are expected to handle
   * duplicate messages without problems. The application can disable retries
   * by changing the retry policy, please see the example below.
   *
   * @par Example
   * @snippet blocking_samples.cc blocking-publish
   *
   * @par Example
   * @snippet blocking_samples.cc blocking-publish-no-retry
   *
   * @return On success, the server-assigned ID of the message. IDs are
   *     guaranteed to be unique within the topic.
   */
  StatusOr<std::string> Publish(Topic topic, Message message,
                                Options opts = {});

 private:
  std::shared_ptr<BlockingPublisherConnection> connection_;
  Options options_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_BLOCKING_PUBLISHER_H
