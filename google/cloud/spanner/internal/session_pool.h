// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_SESSION_POOL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_SESSION_POOL_H

#include "google/cloud/spanner/backoff_policy.h"
#include "google/cloud/spanner/database.h"
#include "google/cloud/spanner/internal/channel.h"
#include "google/cloud/spanner/internal/session.h"
#include "google/cloud/spanner/internal/spanner_stub.h"
#include "google/cloud/spanner/internal/transaction_impl.h"
#include "google/cloud/spanner/retry_policy.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/backoff_policy.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/future.h"
#include "google/cloud/status_or.h"
#include "absl/container/fixed_array.h"
#include "google/spanner/v1/spanner.pb.h"
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
struct SessionPoolFriendForTest;

// An option for the Clock that the session pool will use. This is an injection
// point to facility unit testing.
struct SessionPoolClockOption {
  using Type = std::shared_ptr<Session::Clock>;
};

// Frequency of when the existing multiplexed sessions is replaced with a new
// multiplexed sessions.
struct MultiplexedSessionReplacementIntervalOption {
  using Type = std::chrono::hours;
};

// Frequency of when background work is performed.
struct MultiplexedSessionBackgroundWorkIntervalOption {
  using Type = std::chrono::minutes;
};

class SessionPool;

/**
 * Create a `SessionPool`.
 *
 * The parameters allow the `SessionPool` to make remote calls needed to manage
 * the pool, and to associate `Session`s with the stubs used to create them.
 * `stubs` must not be empty.
 */
std::shared_ptr<SessionPool> MakeSessionPool(
    spanner::Database db, std::vector<std::shared_ptr<SpannerStub>> stubs,
    google::cloud::CompletionQueue cq, Options opts);

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
  ~SessionPool();

  /**
   * Allocates a "regular" session from the pool, which only supports a
   * single transaction at a time, whether read-write or read-only, creating
   * a new one if necessary.
   *
   * The returned `SessionHolder` will return the `Session` to this pool,
   * unless `dissociate_from_pool` is true, in which case it is not returned
   * to the pool.  This is used in partitioned operations, since we don't
   * know when all parties are done using the session.
   *
   * @return a `SessionHolder` on success (which is guaranteed not to be
   * `nullptr`), or an error.
   */
  StatusOr<SessionHolder> Allocate(Session::Mode mode = Session::Mode::kPooled);

  /**
   * Returns the multiplexed session, which allows an unbounded number of
   * concurrent operations, and has no affinity to a single gRPC channel.
   * A multiplexed session is long-lived, but does not require keep-alive
   * requests when idle.
   *
   * May fallback to a "regular" session if no multiplexed session has
   * been allocated.
   *
   * @return a `SessionHolder` on success (which is guaranteed not to be
   * `nullptr`), or an error.
   */
  StatusOr<SessionHolder> Multiplexed(
      Session::Mode mode = Session::Mode::kMultiplexed);

  /**
   * Return a `SpannerStub` to be used when making calls using `session`.
   */
  std::shared_ptr<SpannerStub> GetStub(Session const& session);
  std::shared_ptr<SpannerStub> GetStub(Session const& session,
                                       TransactionContext& context);

  /**
   * Returns the number of sessions in the session pool plus the number of
   * sessions allocated to running transactions.
   *
   * @note This function should only be used for testing as other threads
   *   could be modifying the underlying value immediately after it returns.
   */
  int total_sessions() const;

 private:
  friend std::shared_ptr<SessionPool> MakeSessionPool(
      spanner::Database, std::vector<std::shared_ptr<SpannerStub>>,
      google::cloud::CompletionQueue, Options);

  /**
   * Construct a `SessionPool`.
   *
   * @warning Must call Initialize() once immediately after the constructor.
   */
  SessionPool(spanner::Database db,
              std::vector<std::shared_ptr<SpannerStub>> stubs,
              google::cloud::CompletionQueue cq, Options opts);

  void Initialize();

  // Represents a request to create `session_count` sessions on `channel`
  // See `ComputeCreateCounts` and `CreateSessions`.
  struct CreateCount {
    std::shared_ptr<Channel> channel;
    int session_count;
  };
  enum class WaitForSessionAllocation { kWait, kNoWait };

  // Allocate a session from the pool.
  StatusOr<SessionHolder> Allocate(std::unique_lock<std::mutex>,
                                   Session::Mode mode);

  // Returns a stub to use by round-robining between the channels.
  std::shared_ptr<SpannerStub> GetStub(std::unique_lock<std::mutex>);

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

  Status CreateMultiplexedSession(
      std::unique_lock<std::mutex>& lk);  // EXCLUSIVE_LOCKS_REQUIRED(mu_)
  Status CreateMultiplexedSessionSync(std::shared_ptr<SpannerStub>);
  future<StatusOr<google::spanner::v1::Session>> CreateMultiplexedSessionAsync(
      std::shared_ptr<SpannerStub>);
  Status HandleMultiplexedCreateSessionDone(
      StatusOr<google::spanner::v1::Session> response);
  bool HasValidMultiplexedSession(std::unique_lock<std::mutex> const&) const;

  Status Grow(std::unique_lock<std::mutex>& lk, int sessions_to_create,
              WaitForSessionAllocation wait);  // EXCLUSIVE_LOCKS_REQUIRED(mu_)
  StatusOr<std::vector<CreateCount>> ComputeCreateCounts(
      int sessions_to_create);  // EXCLUSIVE_LOCKS_REQUIRED(mu_)
  Status CreateSessions(std::vector<CreateCount> const& create_counts,
                        WaitForSessionAllocation wait);  // LOCKS_EXCLUDED(mu_)
  Status CreateSessionsSync(std::shared_ptr<Channel> const& channel,
                            std::map<std::string, std::string> const& labels,
                            std::string const& role,
                            int num_sessions);  // LOCKS_EXCLUDED(mu_)
  void CreateSessionsAsync(std::shared_ptr<Channel> const& channel,
                           std::map<std::string, std::string> const& labels,
                           std::string const& role,
                           int num_sessions);  // LOCKS_EXCLUDED(mu_)

  SessionHolder MakeSessionHolder(std::unique_ptr<Session> session,
                                  Session::Mode mode);

  friend struct SessionPoolFriendForTest;  // To test Async*()
  // Asynchronous calls used to maintain the pool.
  future<StatusOr<google::spanner::v1::BatchCreateSessionsResponse>>
  AsyncBatchCreateSessions(CompletionQueue& cq,
                           std::shared_ptr<SpannerStub> const& stub,
                           std::map<std::string, std::string> const& labels,
                           std::string const& role, int num_sessions);
  future<Status> AsyncDeleteSession(CompletionQueue& cq,
                                    std::shared_ptr<SpannerStub> const& stub,
                                    std::string session_name);
  future<StatusOr<google::spanner::v1::ResultSet>> AsyncRefreshSession(
      CompletionQueue& cq, std::shared_ptr<SpannerStub> const& stub,
      std::string session_name);

  Status HandleBatchCreateSessionsDone(
      std::shared_ptr<Channel> const& channel,
      StatusOr<google::spanner::v1::BatchCreateSessionsResponse> response);

  void ScheduleMultiplexedBackgroundWork(std::chrono::seconds relative_time);
  void DoMultiplexedBackgroundWork();
  void ReplaceMultiplexedSession();

  void ScheduleBackgroundWork(std::chrono::seconds relative_time);
  void DoBackgroundWork();
  void MaintainPoolSize();
  void RefreshExpiringSessions();

  // Remove the named session from the pool (if it is present).
  void Erase(std::string const& session_name);

  // Performs the necessary bookkeeping when a session is removed from use.
  void DecrementSessionCount(std::unique_lock<std::mutex> const& lk,
                             Session const& session);

  spanner::Database const db_;
  google::cloud::CompletionQueue cq_;
  Options const opts_;
  std::unique_ptr<spanner::RetryPolicy const> retry_policy_prototype_;
  std::unique_ptr<spanner::BackoffPolicy const> backoff_policy_prototype_;
  std::shared_ptr<Session::Clock> clock_;
  int const max_pool_size_;
  std::mt19937 random_generator_;
  std::chrono::hours multiplexed_session_replacement_interval_;
  std::chrono::minutes multiplexed_session_background_interval_;

  mutable std::mutex mu_;
  std::condition_variable cond_;
  StatusOr<SessionHolder> multiplexed_session_;     // GUARDED_BY(mu_)
  std::vector<std::unique_ptr<Session>> sessions_;  // GUARDED_BY(mu_)
  // total_sessions_ tracks the number of sessions in the pool, a.k.a.
  // sessions_.size(), plus the sessions that have been allocated.
  int total_sessions_ = 0;            // GUARDED_BY(mu_)
  int create_calls_in_progress_ = 0;  // GUARDED_BY(mu_)
  int num_waiting_for_session_ = 0;   // GUARDED_BY(mu_)

  // Lower bound on all `sessions_[i]->last_use_time()` values.
  Session::Clock::time_point last_use_time_lower_bound_ =
      clock_->Now();  // GUARDED_BY(mu_)

  future<void> current_multiplexed_timer_;
  future<void> current_timer_;

  // `channels_` is guaranteed to be non-empty and will not be resized after
  // the constructor runs.
  // n.b. `FixedArray` iterators are never invalidated.
  using ChannelVec = absl::FixedArray<std::shared_ptr<Channel>>;
  ChannelVec channels_;                                 // GUARDED_BY(mu_)
  ChannelVec::iterator next_dissociated_stub_channel_;  // GUARDED_BY(mu_)
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_SESSION_POOL_H
