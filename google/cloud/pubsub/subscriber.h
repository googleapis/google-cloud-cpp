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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SUBSCRIBER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SUBSCRIBER_H

#include "google/cloud/pubsub/message.h"
#include "google/cloud/pubsub/subscriber_connection.h"
#include "google/cloud/pubsub/subscription.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/status.h"
#include <functional>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
/**
 * Receive messages from the Cloud Pub/Sub service.
 *
 * This class is used to receive message from a given subscription, with a fixed
 * configuration such as credentials, and background threads. Applications that
 * receive messages from multiple subscriptions need to create separate
 * instances of this class. Applications wanting to receive events with
 * configuration parameters also need to create separate instances.
 *
 * @see https://cloud.google.com/pubsub for an overview of the Cloud Pub/Sub
 *   service.
 *
 * @par Example: subscriber quickstart
 * @snippet samples.cc subscribe
 *
 * @par Performance
 * `Subscriber` objects are relatively cheap to create, copy, and move. However,
 * each `Subscriber` object must be created with a
 * `std::shared_ptr<SubscriberConnection>`, which itself is relatively expensive
 * to create. Therefore, connection instances should be shared when possible.
 * See the `MakeSubscriberConnection()` function and the `SubscriberConnection`
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
 * @snippet samples.cc custom-thread-pool-subscriber
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
 * @par Changing Retry Parameters Example
 * @snippet samples.cc subscriber-retry-settings
 *
 * [std-future-link]: https://en.cppreference.com/w/cpp/thread/future
 */
class Subscriber {
 public:
  explicit Subscriber(std::shared_ptr<SubscriberConnection> connection)
      : connection_(std::move(connection)) {}

  /**
   * Creates a new session to receive messages from @p subscription.
   *
   * @note Callable must be `CopyConstructible`, as @p cb will be stored in a
   *   [`std::function<>`][std-function-link].
   *
   * @par Idempotency
   * @parblock
   * This is an idempotent operation; it only reads messages from the service.
   * Will make multiple attempts to start a connection to the service, subject
   * to the retry policies configured in the `SubscriberConnection`. Once a
   * successful connection is established the library will try to resume the
   * connection even if the connection fails with a permanent error. Resuming
   * the connection is subject to the retry policies as described earlier.
   *
   * Note that calling `AckHandler::ack()` and/or `AckHandler::nack()` is
   * handled differently with respect to retrying. Check the documentation of
   * these functions for details.
   * @endparblock
   *
   * @par Example
   * @snippet samples.cc subscribe
   *
   * @param cb the callable invoked when messages are received. This must be
   *     usable to construct a
   *     `std::function<void(pubsub::Message, pubsub::AckHandler)>`.
   * @return a future that is satisfied when the session will no longer receive
   *     messages. For example, because there was an unrecoverable error trying
   *     to receive data. Calling `.cancel()` in this object will (eventually)
   *     terminate the session and satisfy the future.
   *
   * [std-function-link]:
   * https://en.cppreference.com/w/cpp/utility/functional/function
   */
  template <typename Callable>
  future<Status> Subscribe(Callable&& cb) {
    std::function<void(Message, AckHandler)> f(std::forward<Callable>(cb));
    return connection_->Subscribe({std::move(f)});
  }

 private:
  std::shared_ptr<SubscriberConnection> connection_;
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SUBSCRIBER_H
