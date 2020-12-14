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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_PUBLISHER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_PUBLISHER_H

#include "google/cloud/pubsub/connection_options.h"
#include "google/cloud/pubsub/publisher_connection.h"
#include "google/cloud/pubsub/publisher_options.h"
#include "google/cloud/pubsub/version.h"

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
/**
 * Publish messages to the Cloud Pub/Sub service.
 *
 * This class is used to publish message to a given topic, with a fixed
 * configuration such as credentials, batching, background threads, etc.
 * Applications that publish messages to multiple topics need to create separate
 * instances of this class. Applications wanting to publish events with
 * different batching configuration also need to create separate instances.
 *
 * @see https://cloud.google.com/pubsub for an overview of the Cloud Pub/Sub
 *   service.
 *
 * @par Example
 * @snippet samples.cc publish
 *
 * @par Message Ordering
 * A `Publisher` configured to preserve message ordering will sequence the
 * messages that share a common ordering key (see
 * `MessageBuilder::SetOrderingKey()`). Messages will be batched by ordering
 * key, and new batches will wait until the status of the previous batch is
 * known. On an error, all pending and queued messages are discarded, and the
 * publisher rejects any new messages for the ordering key that experienced
 * problems. The application must call `Publisher::ResumePublishing()` to
 * to restore publishing.
 *
 * @par Performance
 * `Publisher` objects are relatively cheap to create, copy, and move. However,
 * each `Publisher` object must be created with a
 * `std::shared_ptr<PublisherConnection>`, which itself is relatively expensive
 * to create. Therefore, connection instances should be shared when possible.
 * See the `MakePublisherConnection()` method and the `PublisherConnection`
 * interface for more details.
 *
 * @par Thread Safety
 * Instances of this class created via copy-construction or copy-assignment
 * share the underlying pool of connections. Access to these copies via multiple
 * threads is guaranteed to work. Two threads operating on the same instance of
 * this class is not guaranteed to work.
 *
 * @par Background Threads
 * This class uses the background threads configured via `ConnectionOptions`.
 * Applications can create their own pool of background threads by (a) creating
 * their own #google::cloud::v1::CompletionQueue, (b) setting this completion
 * queue in `pubsub::ConnectionOptions::DisableBackgroundThreads()`, and (c)
 * attaching any number of threads to the completion queue.
 *
 * @par Example: using a custom thread pool
 * @snippet samples.cc custom-thread-pool-publisher
 *
 * @par Asynchronous Functions
 * Some of the member functions in this class return a `future<T>` (or
 * `future<StatusOr<T>>`) object.  Readers are probably familiar with
 * [`std::future<T>`][std-future-link]. Our version adds a `.then()` function to
 * attach a callback to the future, which is invoked when the future is
 * satisfied. This function returns a `future<U>` where `U` is the return value
 * of the attached function. More details in the #google::cloud::v1::future
 * documentation.
 *
 * @par Error Handling
 * This class uses `StatusOr<T>` to report errors. When an operation fails to
 * perform its work the returned `StatusOr<T>` contains the error details. If
 * the `ok()` member function in the `StatusOr<T>` returns `true` then it
 * contains the expected result. Please consult the #google::cloud::v1::StatusOr
 * documentation for more details.
 *
 * @par Batching Configuration Example
 * @snippet samples.cc publisher-options
 *
 * [std-future-link]: https://en.cppreference.com/w/cpp/thread/future
 */
class Publisher {
 public:
  explicit Publisher(std::shared_ptr<PublisherConnection> connection,
                     PublisherOptions options = {});

  //@{
  Publisher(Publisher const&) = default;
  Publisher& operator=(Publisher const&) = default;
  Publisher(Publisher&&) noexcept = default;
  Publisher& operator=(Publisher&&) noexcept = default;
  //@}

  //@{
  friend bool operator==(Publisher const& a, Publisher const& b) {
    return a.connection_ == b.connection_;
  }
  friend bool operator!=(Publisher const& a, Publisher const& b) {
    return !(a == b);
  }
  //@}

  /**
   * Publishes a message to this publisher's topic
   *
   * Note that the message may be batched, depending on the Publisher's
   * configuration. It could be delayed until the batch has enough messages,
   * or enough data, or enough time has elapsed. See the `PublisherOptions`
   * documentation for more details.
   *
   * @par Idempotency
   * This is a non-idempotent operation, but the client library will
   * automatically retry RPCs that fail with transient errors. As Cloud Pub/Sub
   * has "at least once" delivery semantics applications are expected to handle
   * duplicate messages without problems. The application can disable retries
   * by changing the retry policy, please see the example below.
   *
   * @par Example
   * @snippet samples.cc publish
   *
   * @par Disabling Retries Example
   * @snippet samples.cc publisher-disable-retries
   *
   * @par Changing Retry Parameters Example
   * @snippet samples.cc publisher-retry-settings
   *
   * @return a future that becomes satisfied when the message is published or on
   *     a unrecoverable error.
   */
  future<StatusOr<std::string>> Publish(Message m) {
    return connection_->Publish({std::move(m)});
  }

  /**
   * Forcibly publishes any batched messages.
   *
   * As applications can configure a `Publisher` to buffer messages, it is
   * sometimes useful to flush them before any of the normal criteria to send
   * the RPCs is met.
   *
   * @par Idempotency
   * See the description in `Publish()`.
   *
   * @par Example
   * @snippet samples.cc publish-custom-attributes
   *
   * @note This function does not return any status or error codes, the
   *     application can use the `future<StatusOr<std::string>>` returned in
   *     each `Publish()` call to find out what the results are.
   */
  void Flush() { connection_->Flush({}); }

  /**
   * Resumes publishing after an error.
   *
   * If the publisher options have message ordering enabled (see
   * `PublisherOptions::message_ordering()`) all messages for a key that
   * experience failure will be rejected until the application calls this
   * function.
   *
   * @par Idempotency
   * This function never initiates a remote RPC, so there are no considerations
   * around retrying it. Note, however, that more than one `Publish()` request
   * may fail for the same ordering key. The application needs to call this
   * function after **each** error before it can resume publishing messages
   * with the same ordering key.
   *
   * @par Example
   * @snippet samples.cc resume-publish
   */
  void ResumePublish(std::string ordering_key) {
    connection_->ResumePublish({std::move(ordering_key)});
  }

 private:
  std::shared_ptr<PublisherConnection> connection_;
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_PUBLISHER_H
