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

#include "google/cloud/spanner/backoff_policy.h"
#include "google/cloud/spanner/database.h"
#include "google/cloud/spanner/internal/session.h"
#include "google/cloud/spanner/internal/spanner_stub.h"
#include "google/cloud/spanner/retry_policy.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/status_or.h"
#include <google/spanner/v1/spanner.pb.h>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
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
class SessionPool : public std::enable_shared_from_this<SessionPool> {
 public:
  /**
   * Create a `SessionPool`.
   *
   * The parameters allow the `SessionPool` to make remote calls needed to
   * manage the pool, and to associate `Session`s with the stubs used to
   * create them.
   */
  SessionPool(Database db, std::shared_ptr<SpannerStub> stub,
              std::unique_ptr<RetryPolicy> retry_policy,
              std::unique_ptr<BackoffPolicy> backoff_policy,
              SessionPoolOptions options = SessionPoolOptions());

  /**
   * Allocate a `Session` from the pool, creating a new one if necessary.
   *
   * The returned `SessionHolder` will return the `Session` to this pool, unless
   * `dissociate_from_pool` is true, in which case it is not returned to the
   * pool.  This is used in partitioned operations, since we don't know when all
   * parties are done using the session.
   *
   * @return a `SessionHolder` on success (which is guaranteed not to be
   * `nullptr`), or an error.
   */
  StatusOr<SessionHolder> Allocate(bool dissociate_from_pool = false);

 private:
  /**
   * Release session back to the pool.
   *
   * Note the parameter type is a raw pointer because this method will be called
   * from a `SessionHolder` (aka `unique_ptr`) Deleter, which only has a
   * pointer.
   */
  void Release(Session* session);

  StatusOr<std::vector<std::unique_ptr<Session>>> CreateSessions(
      int num_sessions);

  SessionHolder MakeSessionHolder(std::unique_ptr<Session> session,
                                  bool dissociate_from_pool);

  std::mutex mu_;
  std::condition_variable cond_;
  std::vector<std::unique_ptr<Session>> sessions_;  // GUARDED_BY(mu_)
  int total_sessions_ = 0;                          // GUARDED_BY(mu_)
  bool create_in_progress_ = false;                 // GUARDED_BY(mu_)

  Database db_;
  std::shared_ptr<SpannerStub> stub_;
  std::unique_ptr<RetryPolicy const> retry_policy_prototype_;
  std::unique_ptr<BackoffPolicy const> backoff_policy_prototype_;
  SessionPoolOptions options_;
};

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_SESSION_POOL_H_
