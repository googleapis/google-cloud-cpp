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
 * Receive messages fromthe Cloud Pub/Sub service.
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
 * @par Performance
 *
 * `Subscriber` objects are relatively cheap to create, copy, and move. However,
 * each `Subscriber` object must be created with a
 * `std::shared_ptr<SubscriberConnection>`, which itself is relatively expensive
 * to create. Therefore, connection instances should be shared when possible.
 * See the `MakeSubscriberConnection()` function and the `SubscriberConnection`
 * interface for more details.
 *
 * @par Thread Safety
 *
 * Instances of this class created via copy-construction or copy-assignment
 * share the underlying pool of connections. Access to these copies via multiple
 * threads is guaranteed to work. Two threads operating on the same instance of
 * this class is not guaranteed to work.
 *
 * @par Background Threads
 *
 * This class uses the background threads configured via `ConnectionOptions`.
 * Applications can create their own pool of background threads by (a) creating
 * their own #google::cloud::v1::CompletionQueue, (b) setting this completion
 * queue in `pubsub::ConnectionOptions::DisableBackgroundThreads()`, and (c)
 * attaching any number of threads to the completion queue.
 *
 * @snippet samples.cc custom-thread-pool-subscriber
 *
 * @par Asynchronous Functions
 *
 * Some of the member functions in this class return a `future<T>` (or
 * `future<StatusOr<T>>`) object.  Readers are probably familiar with
 * [`std::future<T>`][std-future-link]. Our version adds a `.then()` function to
 * attach a callback to the future, which is invoked when the future is
 * satisfied. This function returns a `future<U>` where `U` is the return value
 * of the attached function. More details in the #google::cloud::v1::future
 * documentation.
 *
 * @par Error Handling
 *
 * This class uses `StatusOr<T>` to report errors. When an operation fails to
 * perform its work the returned `StatusOr<T>` contains the error details. If
 * the `ok()` member function in the `StatusOr<T>` returns `true` then it
 * contains the expected result. Please consult the #google::cloud::v1::StatusOr
 * documentation for more details.
 *
 * @code
 * namespace pubsub = ::google::cloud::pubsub;
 *
 * auto subscription = pubsub::Subscription("my-project", "my-subscription");
 * auto subscriber = pubsub::Subscriber(MakeSubscriberConnection());
 * subscriber->Subscribe(
 *     subscription, [](Message m, AckHandler h) {
 *         std::cout << "received " << m.message_id() << "\n";
 *         std::move(h).ack();
 *     }).then(
 *     [](future<Status> f) {
 *         auto s = f.get();
 *         if (s) return;
 *         std::cout << "unrecoverable error in subscription: " << s << "\n";
 *     });
 * @endcode
 *
 * [std-future-link]: https://en.cppreference.com/w/cpp/thread/future
 */
class Subscriber {
 public:
  explicit Subscriber(std::shared_ptr<SubscriberConnection> connection)
      : connection_(std::move(connection)) {}

  /**
   * Create a new session to receive messages from @p subscription.
   *
   * @note Callable must be CopyConstructible, as @p cb will be stored in a
   *   [`std::function<>`][std-function-link].
   *
   * @param subscription the Cloud Pub/Sub subscription to receive messages from
   * @param cb an object usable to construct a
   *     `std::function<void(pubsub::Message, pubsub::AckHandler)>`
   * @param options configure the subscription's flow control, maximum message
   *     lease time and other parameters, see `SubscriptionOptions` for details
   * @return a future that is satisfied when the session will no longer receive
   *     messages. For example because there was an unrecoverable error trying
   *     to receive data. Calling `.cancel()` in this object will (eventually)
   *     terminate the session too.
   *
   * [std-function-link]:
   * https://en.cppreference.com/w/cpp/utility/functional/function
   */
  template <typename Callable>
  future<Status> Subscribe(Subscription const& subscription, Callable&& cb,
                           SubscriptionOptions options = {}) {
    std::function<void(Message, AckHandler)> f(std::forward<Callable>(cb));
    return connection_->Subscribe(
        {subscription.FullName(), std::move(f), std::move(options)});
  }

 private:
  std::shared_ptr<SubscriberConnection> connection_;
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SUBSCRIBER_H
