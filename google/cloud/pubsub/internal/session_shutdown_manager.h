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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SESSION_SHUTDOWN_MANAGER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SESSION_SHUTDOWN_MANAGER_H

#include "google/cloud/pubsub/version.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/status.h"
#include <mutex>
#include <unordered_map>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

/**
 * A helper class to track (and debug) `SubscriptionSession`'s shutdown process
 *
 * The `SubscriptionSession` class needs to implement an orderly shutdown when
 * the application requests it (via a `future<>::cancel()` call) or when the
 * session fails, i.e., the `AsyncStreamingPull()` fails and we have exhausted
 * the retry policies.
 *
 * Once the shutdown is initiated we need to stop any operation that would
 * create more work, including:
 * - New callbacks to the application
 * - Making new calls to `AsyncStreamingPull()`
 * - Handling any responses from `AsyncStreamingPull()`
 * - Creating any new timers to update message leases.
 * - Creating any new `AsyncModifyAckDeadline()` requests to update message
 *   leases.
 *
 * When the shutdown is requested we should also cancel any pending timers, as
 * these can be long and we do not want to wait until they expire. We should
 * also make a best effort attempt to `nack()` any pending messages that are
 * not being handled by the application, as well as any messages that are not
 * being handled by a callback.
 */
class SessionShutdownManager {
 public:
  SessionShutdownManager() = default;
  ~SessionShutdownManager();

  /// Set the promise to signal when the shutdown has completed.
  future<Status> Start(promise<Status> p) {
    done_ = std::move(p);
    return done_.get_future();
  }

  /**
   * Start an operation, using the current thread of control.
   *
   * If the shutdown process has not started, this function calls @p op and
   * increments the count of outstanding functions.
   *
   * Note that this function takes parameters to trace the activity, but this
   * tracing is typically disabled at compile-time.
   */
  template <typename Operation>
  bool StartOperation(char const* caller, char const* name, Operation&& op) {
    std::unique_lock<std::mutex> lk(mu_);
    LogStart(caller, name);
    if (shutdown_) return false;
    ++outstanding_operations_;
    lk.unlock();
    op();
    return true;
  }

  /**
   * Start an asynchronous operation using @p executor.
   *
   * If the shutdown process has not started, this function calls @p op using
   * one of the threads associated with @p executor, and increments the count
   * of outstanding functions.
   *
   * Note that this function takes parameters to trace the activity, but this
   * tracing is typically disabled at compile-time.
   */
  template <typename Operation>
  bool StartAsyncOperation(char const* caller, char const* name,
                           CompletionQueue& executor, Operation&& op) {
    std::unique_lock<std::mutex> lk(mu_);
    LogStart(caller, name);
    if (shutdown_) return false;
    ++outstanding_operations_;
    lk.unlock();
    executor.RunAsync(std::forward<Operation>(op));
    return true;
  }

  /// Record an operation completion, returns true if marked for shutdown.
  bool FinishedOperation(char const* name);

  // Start the shutdown process
  void MarkAsShutdown(char const* caller, Status status);

 private:
  void LogStart(char const* caller, char const* name);
  void SignalOnShutdown(std::unique_lock<std::mutex> lk);

  std::mutex mu_;
  bool shutdown_ = false;
  bool signaled_ = false;
  int outstanding_operations_ = 0;
  Status result_;
  promise<Status> done_;
  std::unordered_map<std::string, int> ops_;
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SESSION_SHUTDOWN_MANAGER_H
