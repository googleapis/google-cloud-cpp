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

#ifndef GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_SESSION_POOL_H
#define GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_SESSION_POOL_H

#include "google/cloud/spanner/backoff_policy.h"
#include "google/cloud/spanner/database.h"
#include "google/cloud/spanner/internal/session.h"
#include "google/cloud/spanner/internal/spanner_stub.h"
#include "google/cloud/spanner/retry_policy.h"
#include "google/cloud/spanner/session_pool_options.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/background_threads.h"
#include "google/cloud/future.h"
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
   * Construct a `SessionPool`.
   *
   * @warning callers should use the `MakeSessionPool()` factory method rather
   * than calling the constructor and `Initialize()` directly.
   */
  SessionPool(Database db, std::vector<std::shared_ptr<SpannerStub>> stubs,
              SessionPoolOptions options,
              std::unique_ptr<BackgroundThreads> background_threads,
              std::unique_ptr<RetryPolicy> retry_policy,
              std::unique_ptr<BackoffPolicy> backoff_policy);

  ~SessionPool();

  /**
   * If using the factory method is not possible, call `Initialize()` exactly
   * once, immediately after constructing the pool. This is necessary because
   * we are unable to call shared_from_this() in the constructor.
   */
  void Initialize();

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

  /**
   * Return a `SpannerStub` to be used when making calls using `session`.
   */
  std::shared_ptr<SpannerStub> GetStub(Session const& session);

 private:
  struct ChannelInfo {
    explicit ChannelInfo(std::shared_ptr<SpannerStub> stub_param)
        : stub(std::move(stub_param)) {}
    std::shared_ptr<SpannerStub> const stub;
    int session_count = 0;
  };

  // Release session back to the pool.
  void Release(std::unique_ptr<Session> session);

  // Called when a thread needs to wait for a `Session` to become available.
  // @p specifies the condition to wait for.
  template <typename Predicate>
  void Wait(std::unique_lock<std::mutex>& lk, Predicate&& p) {
    ++num_waiting_for_session_;
    cond_.wait(lk, std::forward<Predicate>(p));
    --num_waiting_for_session_;
  }

  Status CreateSessions(std::unique_lock<std::mutex>& lk, ChannelInfo& channel,
                        std::map<std::string, std::string> const& labels,
                        int num_sessions);  // EXCLUSIVE_LOCKS_REQUIRED(mu_)

  SessionHolder MakeSessionHolder(std::unique_ptr<Session> session,
                                  bool dissociate_from_pool);

  void UpdateNextChannelForCreateSessions();  // EXCLUSIVE_LOCKS_REQUIRED(mu_)

  void ScheduleBackgroundWork(std::chrono::seconds relative_time);
  void DoBackgroundWork();

  Database const db_;
  SessionPoolOptions const options_;
  std::unique_ptr<BackgroundThreads> const background_threads_;
  std::unique_ptr<RetryPolicy const> retry_policy_prototype_;
  std::unique_ptr<BackoffPolicy const> backoff_policy_prototype_;
  int const max_pool_size_;

  std::mutex mu_;
  std::condition_variable cond_;
  std::vector<std::unique_ptr<Session>> sessions_;  // GUARDED_BY(mu_)
  int total_sessions_ = 0;                          // GUARDED_BY(mu_)
  bool create_in_progress_ = false;                 // GUARDED_BY(mu_)
  int num_waiting_for_session_ = 0;                 // GUARDED_BY(mu_)
  future<void> current_timer_;

  // `channels_` is guaranteed to be non-empty and will not be resized after
  // the constructor runs (so the iterators are guaranteed to always be valid).
  // TODO(#566) replace `vector` with `absl::FixedArray` when available.
  std::vector<ChannelInfo> channels_;  // GUARDED_BY(mu_)
  std::vector<ChannelInfo>::iterator
      next_channel_for_create_sessions_;  // GUARDED_BY(mu_)
  std::vector<ChannelInfo>::iterator
      next_dissociated_stub_channel_;  // GUARDED_BY(mu_)
};

/**
 * Create a `SessionPool`.
 *
 * The parameters allow the `SessionPool` to make remote calls needed to manage
 * the pool, and to associate `Session`s with the stubs used to create them.
 * `stubs` must not be empty.
 */
std::shared_ptr<SessionPool> MakeSessionPool(
    Database db, std::vector<std::shared_ptr<SpannerStub>> stubs,
    SessionPoolOptions options,
    std::unique_ptr<BackgroundThreads> background_threads,
    std::unique_ptr<RetryPolicy> retry_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy);

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_SESSION_POOL_H
