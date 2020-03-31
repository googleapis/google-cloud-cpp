// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_SESSION_POOL_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_SESSION_POOL_OPTIONS_H

#include "google/cloud/spanner/version.h"
#include <algorithm>
#include <chrono>
#include <map>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

// What action to take if the session pool is exhausted.
enum class ActionOnExhaustion { kBlock, kFail };

/**
 * Controls the session pool maintained by a `spanner::Client`.
 *
 * Creating Cloud Spanner sessions is an expensive operation. The
 * [recommended practice][spanner-sessions-doc] is to maintain a cache (or pool)
 * of sessions in the client side. This class controls the initial size of this
 * pool, and how the pool grows (or shrinks) as needed.
 *
 * @note If no sessions are available to perform an operation the client library
 *     blocks until new sessions are available (either released by other threads
 *     or allocated on-demand, depending on the active constraints). It is
 *     also possible to configure the client to fail a request when the session
 *     pool is exhausted.
 *
 * [spanner-sessions-doc]: https://cloud.google.com/spanner/docs/sessions
 */
class SessionPoolOptions {
 public:
  /**
   * Enforce the stated constraints on the option values, altering them if
   * necessary. This can't be done in the setters, since we don't yet know
   * the number of channels, and it would also constrain the order in which
   * the fields must be set.
   *
   * @p num_channels the number of RPC channels in use by the pool.
   */
  SessionPoolOptions& EnforceConstraints(int num_channels) {
    min_sessions_ = (std::max)(min_sessions_, 0);
    max_sessions_per_channel_ = (std::max)(max_sessions_per_channel_, 1);
    min_sessions_ =
        (std::min)(min_sessions_, max_sessions_per_channel_ * num_channels);
    max_idle_sessions_ = (std::max)(max_idle_sessions_, 0);
    return *this;
  }

  /**
   * Set the minimum number of sessions to keep in the pool.
   * Values <= 0 are treated as 0.
   * This value will effectively be reduced if it exceeds the overall limit on
   * the number of sessions (`max_sessions_per_channel` * number of channels).
   */
  SessionPoolOptions& set_min_sessions(int count) {
    min_sessions_ = count;
    return *this;
  }

  /// Return the minimum number of sessions to keep in the pool.
  int min_sessions() const { return min_sessions_; }

  /**
   * Set the maximum number of sessions to create on each channel.
   * Values <= 1 are treated as 1.
   */
  SessionPoolOptions& set_max_sessions_per_channel(int count) {
    max_sessions_per_channel_ = count;
    return *this;
  }

  /// Return the minimum number of sessions to keep in the pool.
  int max_sessions_per_channel() const { return max_sessions_per_channel_; }

  /**
   * Set the maximum number of sessions to keep in the pool in an idle state.
   * Values <= 0 are treated as 0.
   */
  SessionPoolOptions& set_max_idle_sessions(int count) {
    max_idle_sessions_ = count;
    return *this;
  }

  /// Return the maximum number of idle sessions to keep in the pool.
  int max_idle_sessions() const { return max_idle_sessions_; }

  /// Set whether to block or fail on pool exhaustion.
  SessionPoolOptions& set_action_on_exhaustion(ActionOnExhaustion action) {
    action_on_exhaustion_ = action;
    return *this;
  }

  /**
   * Return the action to take (kBlock or kFail) when attempting to allocate a
   * session when the pool is exhausted.
   */
  ActionOnExhaustion action_on_exhaustion() const {
    return action_on_exhaustion_;
  }

  /*
   * Set the interval at which we refresh sessions so they don't get
   * collected by the backend GC. The GC collects objects older than 60
   * minutes, so any duration below that (less some slack to allow the calls
   * to be made to refresh the sessions) should suffice.
   */
  SessionPoolOptions& set_keep_alive_interval(std::chrono::seconds interval) {
    keep_alive_interval_ = interval;
    return *this;
  }

  /// Return the interval at which we refresh sessions to prevent GC.
  std::chrono::seconds keep_alive_interval() const {
    return keep_alive_interval_;
  }

  /**
   * Set the labels used when creating sessions within the pool.
   *  * Label keys must match `[a-z]([-a-z0-9]{0,61}[a-z0-9])?`.
   *  * Label values must match `([a-z]([-a-z0-9]{0,61}[a-z0-9])?)?`.
   *  * The maximum number of labels is 64.
   */
  SessionPoolOptions& set_labels(std::map<std::string, std::string> labels) {
    labels_ = std::move(labels);
    return *this;
  }

  /// Return the labels used when creating sessions within the pool.
  std::map<std::string, std::string> const& labels() const { return labels_; }

 private:
  int min_sessions_ = 0;
  int max_sessions_per_channel_ = 100;
  int max_idle_sessions_ = 0;
  ActionOnExhaustion action_on_exhaustion_ = ActionOnExhaustion::kBlock;
  std::chrono::seconds keep_alive_interval_ = std::chrono::minutes(55);
  std::map<std::string, std::string> labels_;
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_SESSION_POOL_OPTIONS_H
