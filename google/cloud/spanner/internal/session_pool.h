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

#ifndef GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_SESSION_POOL_H_
#define GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_SESSION_POOL_H_

#include "google/cloud/spanner/internal/session.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/status_or.h"
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <vector>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {

// Interface used by the `SessionPool` to manage sessions.
class SessionManager {
 public:
  virtual ~SessionManager() = default;
  // Create up to `num_sessions` sessions (note that fewer may be returned).
  // `num_sessions` must be > 0 or an error is returned.
  virtual StatusOr<std::vector<std::unique_ptr<Session>>> CreateSessions(
      int num_sessions) = 0;
};

// What action to take if the session pool is exhausted.
enum class ActionOnExhaustion { BLOCK, FAIL };

struct SessionPoolOptions {
  // The minimum number of sessions to keep in the pool.
  // Values <= 0 are treated as 0.
  int min_sessions = 0;

  // The maximum number of sessions to create. This should be the number
  // of channels * 100.
  // Values <= 1 are treated as 1.
  // If this is less than `min_sessions`, it will be set to `min_sessions`.
  int max_sessions = 100;  // Channel Count * 100

  // The maximum number of sessions that can be in the pool in an idle state.
  // Values <= 0 are treated as 0.
  int max_idle_sessions = 0;

  // The fraction of sessions to prepare for write in advance.
  // This fraction represents observed cloud spanner usage as of May 2019.
  // Values <= 0.0 are treated as 0.0; values >= 1.0 are treated as 1.0.
  double write_sessions_fraction = 0.25;

  // Decide whether to block or fail on pool exhaustion.
  ActionOnExhaustion action_on_exhaustion = ActionOnExhaustion::BLOCK;

  // This is the interval at which we refresh sessions so they don't get
  // collected by the backend GC. The GC collects objects older than 60
  // minutes, so any duration below that (less some slack to allow the calls
  // to be made to refresh the sessions) should suffice.
  std::chrono::minutes keep_alive_interval = std::chrono::minutes(55);
};

/**
 * Maintains a pool of `Session` objects.
 *
 * Session creation is relatively expensive (30-100ms), so we keep a pool of
 * Sessions to avoid incurring the overhead of creating a Session for every
 * Transaction. Typically, we will allocate a `Session` from the pool the
 * first time we use a `Transaction`, then return it to the pool when the
 * `Transaction` finishes.
 *
 * Allocation from the pool is LIFO to take advantage of the fact the Spanner
 * backends maintain a cache of sessions which is valid for 30 seconds, so
 * re-using Sessions as quickly as possible has performance advantages.
 */
class SessionPool {
 public:
  /**
   * Create a `SessionPool`.
   * Uses `manager` to create, delete, and refresh sessions in the pool.
   */
  SessionPool(SessionManager* manager,
              SessionPoolOptions options = SessionPoolOptions());

  /**
   * Allocate a session from the pool, creating a new `Session` if necessary.
   * If `dissociate_from_pool` is true, the caller does not intend to return
   * this `Session` to the pool.
   * @returns an error if session creation fails; always returns a non-null
   * pointer on success.
   */
  StatusOr<std::unique_ptr<Session>> Allocate(
      bool dissociate_from_pool = false);

  /// Release `session` back to the pool.
  void Release(std::unique_ptr<Session> session);

 private:
  std::mutex mu_;
  std::condition_variable cond_;
  std::vector<std::unique_ptr<Session>> sessions_;  // GUARDED_BY(mu_)
  int total_sessions_ = 0;                          // GUARDED_BY(mu_)
  bool create_in_progress_ = false;                 // GUARDED_BY(mu_)

  SessionManager* manager_;
  SessionPoolOptions options_;
};

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_SESSION_POOL_H_
