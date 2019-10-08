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
   * Create a `SessionPool`. Uses `manager` to create, delete, and refresh
   * sessions in the pool.
   */
  SessionPool(SessionManager* manager) : manager_(manager) {}

  /**
   * Allocate a session from the pool, creating a new `Session` if necessary.
   * @returns an error if session creation fails; always returns a non-null
   * pointer on success.
   */
  StatusOr<std::unique_ptr<Session>> Allocate();

  /// Release `session` back to the pool.
  void Release(std::unique_ptr<Session> session);

 private:
  std::mutex mu_;
  std::vector<std::unique_ptr<Session>> sessions_;  // GUARDED_BY(mu_)

  SessionManager* manager_;
};

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_SESSION_POOL_H_
