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

#include "google/cloud/spanner/internal/session_pool.h"
#include "google/cloud/spanner/internal/route_to_leader.h"
#include "google/cloud/spanner/internal/session.h"
#include "google/cloud/spanner/internal/status_utils.h"
#include "google/cloud/spanner/options.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/internal/async_retry_loop.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/retry_loop.h"
#include "google/cloud/log.h"
#include "google/cloud/options.h"
#include "google/cloud/status.h"
#include <grpcpp/grpcpp.h>
#include <algorithm>
#include <chrono>
#include <random>
#include <thread>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::Idempotency;

std::shared_ptr<SessionPool> MakeSessionPool(
    spanner::Database db, std::vector<std::shared_ptr<SpannerStub>> stubs,
    google::cloud::CompletionQueue cq, Options opts) {
  auto pool = std::shared_ptr<SessionPool>(new SessionPool(
      std::move(db), std::move(stubs), std::move(cq), std::move(opts)));
  pool->Initialize();
  return pool;
}

SessionPool::SessionPool(spanner::Database db,
                         std::vector<std::shared_ptr<SpannerStub>> stubs,
                         google::cloud::CompletionQueue cq, Options opts)
    : db_(std::move(db)),
      cq_(std::move(cq)),
      opts_(std::move(opts)),
      retry_policy_prototype_(
          opts_.get<spanner::SpannerRetryPolicyOption>()->clone()),
      backoff_policy_prototype_(
          opts_.get<spanner::SpannerBackoffPolicyOption>()->clone()),
      clock_(opts_.get<SessionPoolClockOption>()),
      max_pool_size_(
          opts_.get<spanner::SessionPoolMaxSessionsPerChannelOption>() *
          static_cast<int>(stubs.size())),
      random_generator_(std::random_device()()),
      channels_(stubs.size()) {
  if (stubs.empty()) {
    google::cloud::internal::ThrowInvalidArgument(
        "SessionPool requires a non-empty set of stubs");
  }

  for (auto i = 0U; i < stubs.size(); ++i) {
    channels_[i] = std::make_shared<Channel>(std::move(stubs[i]));
  }
  // `channels_` is never resized after this point.
  next_dissociated_stub_channel_ = channels_.begin();
}

void SessionPool::Initialize() {
  internal::OptionsSpan span(opts_);
  CreateMultiplexedSession();
  auto const min_sessions = opts_.get<spanner::SessionPoolMinSessionsOption>();
  if (min_sessions > 0) {
    std::unique_lock<std::mutex> lk(mu_);
    Grow(lk, min_sessions, WaitForSessionAllocation::kWait);
  }
  ScheduleBackgroundWork(std::chrono::seconds(5));
}

SessionPool::~SessionPool() {
  // All references to this object are via `shared_ptr`. Since we're in the
  // destructor that implies there can be no concurrent accesses to any member
  // variables, including `current_timer_`.
  //
  // Note that it *is* possible the timer lambda in `ScheduleBackgroundWork`
  // or the response handler in `RefreshExpiringSessions` are executing
  // concurrently. However, since we are in the destructor we know that
  // they must not have successfully finished a call to `lock()` on the
  // `weak_ptr` to `this` they hold. Any in-progress or subsequent `lock()`
  // will now return `nullptr`, in which case no work is done.
  current_timer_.cancel();

  // Send fire-and-forget `AsyncDeleteSession()` calls for all sessions.
  if (HasValidMultiplexedSession(std::unique_lock<std::mutex>(mu_))) {
    AsyncDeleteSession(cq_, GetStub(*multiplexed_session_),
                       multiplexed_session_->session_name())
        .then([](auto result) { auto status = result.get(); });
  }
  for (auto const& session : sessions_) {
    if (session->is_bad()) continue;
    AsyncDeleteSession(cq_, GetStub(*session), session->session_name())
        .then([](auto result) { auto status = result.get(); });
  }
}

void SessionPool::ScheduleBackgroundWork(std::chrono::seconds relative_time) {
  std::weak_ptr<SessionPool> pool = shared_from_this();
  current_timer_ =
      cq_.MakeRelativeTimer(relative_time)
          .then([pool](future<StatusOr<std::chrono::system_clock::time_point>>
                           result) {
            if (result.get().ok()) {
              if (auto shared_pool = pool.lock()) {
                shared_pool->DoBackgroundWork();
              }
            }
          });
}

void SessionPool::DoBackgroundWork() {
  MaintainPoolSize();
  RefreshExpiringSessions();
  ScheduleBackgroundWork(std::chrono::seconds(5));
}

// Ensure the pool size conforms to what was specified in the `SessionOptions`,
// creating or deleting sessions as necessary.
void SessionPool::MaintainPoolSize() {
  CreateMultiplexedSession();
  auto const min_sessions = opts_.get<spanner::SessionPoolMinSessionsOption>();
  std::unique_lock<std::mutex> lk(mu_);
  if (create_calls_in_progress_ == 0 && total_sessions_ < min_sessions) {
    Grow(lk, total_sessions_ - min_sessions, WaitForSessionAllocation::kNoWait);
  }
}

// Refresh all sessions whose last-use time is older than the keep-alive
// interval. Issues asynchronous RPCs, so this method does not block.
void SessionPool::RefreshExpiringSessions() {
  std::vector<std::pair<std::shared_ptr<SpannerStub>, std::string>>
      sessions_to_refresh;
  auto now = clock_->Now();
  auto refresh_limit =
      now - opts_.get<spanner::SessionPoolKeepAliveIntervalOption>();
  {
    std::unique_lock<std::mutex> lk(mu_);
    if (last_use_time_lower_bound_ <= refresh_limit) {
      last_use_time_lower_bound_ = now;
      for (auto const& session : sessions_) {
        auto last_use_time = session->last_use_time();
        if (last_use_time <= refresh_limit) {
          sessions_to_refresh.emplace_back(session->channel()->stub,
                                           session->session_name());
          session->update_last_use_time();
        } else if (last_use_time < last_use_time_lower_bound_) {
          last_use_time_lower_bound_ = last_use_time;
        }
      }
    }
  }
  std::weak_ptr<SessionPool> pool = shared_from_this();
  for (auto& refresh : sessions_to_refresh) {
    auto handler =
        [pool, session_name = refresh.second](
            future<StatusOr<google::spanner::v1::ResultSet>> result) {
          auto response = result.get();
          if (!response && IsSessionNotFound(response.status())) {
            if (auto shared_pool = pool.lock()) {
              // The pool still exists, but the bad session may no
              // longer be in the pool because someone else has already
              // tried to use it, discovered that it is bad, and so did
              // not return it (or they are in process of doing all that).
              // But if it is still in the pool, we remove it now.
              shared_pool->Erase(session_name);
            }
          }
        };
    AsyncRefreshSession(cq_, std::move(refresh.first),
                        std::move(refresh.second))
        .then(std::move(handler));
  }
}

void SessionPool::Erase(std::string const& session_name) {
  std::unique_ptr<Session> target;
  std::unique_lock<std::mutex> lk(mu_);
  for (auto& session : sessions_) {
    if (session->session_name() == session_name) {
      target = std::move(session);  // deferred deletion
      session = std::move(sessions_.back());
      sessions_.pop_back();
      break;
    }
  }
}

Status SessionPool::CreateMultiplexedSession() {
  std::unique_lock<std::mutex> lk(mu_);
  if (!HasValidMultiplexedSession(lk)) {
    auto stub = GetStub(std::move(lk));
    auto name = CreateMultiplexedSession(std::move(stub));
    if (!name) return name.status();
    auto session = std::make_shared<Session>(*std::move(name),
                                             /*channel=*/nullptr, clock_);
    std::unique_lock<std::mutex> lk(mu_);
    multiplexed_session_ = std::move(session);
  }
  return Status{};
}

StatusOr<std::string> SessionPool::CreateMultiplexedSession(
    std::shared_ptr<SpannerStub> stub) const {
  google::spanner::v1::CreateSessionRequest request;
  request.set_database(db_.FullName());
  auto* session = request.mutable_session();
  auto const& labels = opts_.get<spanner::SessionPoolLabelsOption>();
  if (!labels.empty()) {
    session->mutable_labels()->insert(labels.begin(), labels.end());
  }
  auto const& role = opts_.get<spanner::SessionCreatorRoleOption>();
  if (!role.empty()) session->set_creator_role(role);
  session->set_multiplexed(true);

  auto response = RetryLoop(
      retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
      google::cloud::Idempotency::kIdempotent,
      [&stub](grpc::ClientContext& context, Options const& options,
              google::spanner::v1::CreateSessionRequest const& request) {
        RouteToLeader(context);  // always for CreateSession()
        return stub->CreateSession(context, options, request);
      },
      opts_, request, __func__);
  if (!response) return std::move(response).status();
  return response->name();
}

bool SessionPool::HasValidMultiplexedSession(
    std::unique_lock<std::mutex> const&) const {
  return multiplexed_session_ && !multiplexed_session_->is_bad();
}

/*
 * Grow the session pool by creating up to `sessions_to_create` sessions and
 * adding them to the pool.  Note that `lk` may be released and reacquired in
 * this method.
 *
 * TODO(#4029) eliminate the `wait` parameter and do all creation
 * asynchronously. The main obstacle is making existing tests pass.
 */
Status SessionPool::Grow(std::unique_lock<std::mutex>& lk,
                         int sessions_to_create,
                         WaitForSessionAllocation wait) {
  auto create_counts = ComputeCreateCounts(sessions_to_create);
  if (!create_counts.ok() || create_counts->empty()) {
    return create_counts.status();
  }
  create_calls_in_progress_ += static_cast<int>(create_counts->size());

  // Create all the sessions without the lock held (the lock will be
  // reacquired independently when the remote calls complete).
  lk.unlock();
  Status status = CreateSessions(*create_counts, wait);
  lk.lock();
  return status;
}

StatusOr<std::vector<SessionPool::CreateCount>>
SessionPool::ComputeCreateCounts(int sessions_to_create) {
  if (total_sessions_ == max_pool_size_) {
    // Can't grow the pool since we're already at max size.
    return internal::ResourceExhaustedError("session pool exhausted",
                                            GCP_ERROR_INFO());
  }

  // Compute how many Sessions to create on each Channel, trying to keep the
  // number of Sessions on each channel equal.
  //
  // However, the counts may become unequal over time, and we do not want
  // to delete sessions just to make the counts equal, so do the best we
  // can within those constraints.
  int target_total_sessions =
      (std::min)(total_sessions_ + sessions_to_create, max_pool_size_);

  // Sort the channels in *descending* order of session count.
  absl::FixedArray<std::shared_ptr<Channel>> channels_by_count = channels_;
  std::sort(channels_by_count.begin(), channels_by_count.end(),
            [](std::shared_ptr<Channel> const& lhs,
               std::shared_ptr<Channel> const& rhs) {
              // Use `>` to sort in descending order.
              return lhs->session_count > rhs->session_count;
            });

  // Compute the number of new Sessions to create on each channel.
  int sessions_remaining = target_total_sessions;
  int channels_remaining = static_cast<int>(channels_.size());
  std::vector<CreateCount> create_counts;
  for (auto& channel : channels_by_count) {
    // The target number of sessions for this channel, rounded up.
    int target =
        (sessions_remaining + channels_remaining - 1) / channels_remaining;
    --channels_remaining;
    if (channel->session_count < target) {
      int sessions_to_create = target - channel->session_count;
      create_counts.push_back({channel, sessions_to_create});
      // Subtract the number of Sessions this channel will have after creation
      // finishes from the remaining sessions count.
      sessions_remaining -= target;
    } else {
      // This channel is already over its target. Don't create any Sessions
      // on it, just update the remaining sessions count.
      sessions_remaining -= channel->session_count;
    }
  }
  return create_counts;
}

Status SessionPool::CreateSessions(
    std::vector<CreateCount> const& create_counts,
    WaitForSessionAllocation wait) {
  Status return_status;
  auto const& labels = opts_.get<spanner::SessionPoolLabelsOption>();
  auto const& role = opts_.get<spanner::SessionCreatorRoleOption>();
  for (auto const& op : create_counts) {
    switch (wait) {
      case WaitForSessionAllocation::kWait: {
        auto status =
            CreateSessionsSync(op.channel, labels, role, op.session_count);
        if (!status.ok()) {
          return_status = status;
        }
        break;
      }
      case WaitForSessionAllocation::kNoWait:
        CreateSessionsAsync(op.channel, labels, role, op.session_count);
        break;
    }
  }
  return return_status;
}

StatusOr<SessionHolder> SessionPool::Allocate(bool dissociate_from_pool) {
  return Allocate(std::unique_lock<std::mutex>(mu_), dissociate_from_pool);
}

StatusOr<SessionHolder> SessionPool::Multiplexed() {
  std::unique_lock<std::mutex> lk(mu_);
  // If we don't have a multiplexed session (yet), use a regular one.
  if (!HasValidMultiplexedSession(lk)) return Allocate(std::move(lk), false);
  return multiplexed_session_;
}

std::shared_ptr<SpannerStub> SessionPool::GetStub(Session const& session) {
  auto const& channel = session.channel();
  if (channel) return channel->stub;

  // Multiplexed sessions, or sessions that were created for partitioned
  // Reads/Queries, do not have their own channel/stub, so return a stub
  // to use by round-robining between the channels.
  return GetStub(std::unique_lock<std::mutex>(mu_));
}

StatusOr<SessionHolder> SessionPool::Allocate(std::unique_lock<std::mutex> lk,
                                              bool dissociate_from_pool) {
  // We choose to ignore the internal::CurrentOptions() here as it is
  // non-deterministic when RPCs to create sessions are actually made.
  // It is clearer if we just stick with the construction-time Options.
  internal::OptionsSpan span(opts_);
  for (;;) {
    if (!sessions_.empty()) {
      // return the most recently used session.
      auto session = std::move(sessions_.back());
      sessions_.pop_back();
      if (dissociate_from_pool) {
        --total_sessions_;
        auto const& channel = session->channel();
        if (channel) {
          --channel->session_count;
        }
      }
      return {MakeSessionHolder(std::move(session), dissociate_from_pool)};
    }

    // If the pool is at its max size, fail or wait until someone returns a
    // session to the pool then try again.
    if (total_sessions_ >= max_pool_size_) {
      if (opts_.get<spanner::SessionPoolActionOnExhaustionOption>() ==
          spanner::ActionOnExhaustion::kFail) {
        return internal::ResourceExhaustedError("session pool exhausted",
                                                GCP_ERROR_INFO());
      }
      Wait(lk, [this] {
        return !sessions_.empty() || total_sessions_ < max_pool_size_;
      });
      continue;
    }

    // Create new sessions for the pool.
    //
    // TODO(#4028) Currently we only allow one thread to do this at a time; a
    // possible enhancement is tracking the number of waiters and issuing more
    // simultaneous calls if additional sessions are needed. We can also use the
    // number of waiters in the `sessions_to_create` calculation below.
    if (create_calls_in_progress_ > 0) {
      Wait(lk, [this] {
        return !sessions_.empty() || create_calls_in_progress_ == 0;
      });
      continue;
    }

    // Try to add some sessions to the pool; for now add `min_sessions` plus
    // one for the `Session` this caller is waiting for.
    auto const min_sessions =
        opts_.get<spanner::SessionPoolMinSessionsOption>();
    auto status = Grow(lk, min_sessions + 1, WaitForSessionAllocation::kWait);
    if (!status.ok()) {
      return status;
    }
  }
}

std::shared_ptr<SpannerStub> SessionPool::GetStub(
    std::unique_lock<std::mutex>) {
  auto stub = (*next_dissociated_stub_channel_)->stub;
  if (++next_dissociated_stub_channel_ == channels_.end()) {
    next_dissociated_stub_channel_ = channels_.begin();
  }
  return stub;
}

void SessionPool::Release(std::unique_ptr<Session> session) {
  std::unique_lock<std::mutex> lk(mu_);
  if (session->is_bad()) {
    // Once we have support for background processing, we may want to signal
    // that to replenish this bad session.
    --total_sessions_;
    auto const& channel = session->channel();
    if (channel) {
      --channel->session_count;
    }
    return;
  }
  session->update_last_use_time();
  sessions_.push_back(std::move(session));
  if (num_waiting_for_session_ > 0) {
    lk.unlock();
    cond_.notify_one();
  }
}

// Creates `num_sessions` on `channel` and adds them to the pool.
Status SessionPool::CreateSessionsSync(
    std::shared_ptr<Channel> const& channel,
    std::map<std::string, std::string> const& labels, std::string const& role,
    int num_sessions) {
  google::spanner::v1::BatchCreateSessionsRequest request;
  request.set_database(db_.FullName());
  if (!labels.empty()) {
    request.mutable_session_template()->mutable_labels()->insert(labels.begin(),
                                                                 labels.end());
  }
  if (!role.empty()) {
    request.mutable_session_template()->set_creator_role(role);
  }
  request.set_session_count(std::int32_t{num_sessions});
  auto const& stub = channel->stub;
  auto const& current = internal::CurrentOptions();
  auto response = RetryLoop(
      retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
      google::cloud::Idempotency::kIdempotent,
      [&stub](grpc::ClientContext& context, Options const& options,
              google::spanner::v1::BatchCreateSessionsRequest const& request) {
        RouteToLeader(context);  // always for BatchCreateSessions()
        return stub->BatchCreateSessions(context, options, request);
      },
      current, request, __func__);
  return HandleBatchCreateSessionsDone(channel, std::move(response));
}

void SessionPool::CreateSessionsAsync(
    std::shared_ptr<Channel> const& channel,
    std::map<std::string, std::string> const& labels, std::string const& role,
    int num_sessions) {
  std::weak_ptr<SessionPool> pool = shared_from_this();
  AsyncBatchCreateSessions(cq_, channel->stub, labels, role, num_sessions)
      .then(
          [pool, channel](
              future<StatusOr<google::spanner::v1::BatchCreateSessionsResponse>>
                  result) {
            if (auto shared_pool = pool.lock()) {
              shared_pool->HandleBatchCreateSessionsDone(
                  channel, std::move(result).get());
            }
          });
}

SessionHolder SessionPool::MakeSessionHolder(std::unique_ptr<Session> session,
                                             bool dissociate_from_pool) {
  if (dissociate_from_pool) {
    // Uses the default deleter; the `Session` is not returned to the pool.
    return {std::move(session)};
  }
  std::weak_ptr<SessionPool> pool = shared_from_this();
  return SessionHolder(session.release(), [pool](Session* s) {
    std::unique_ptr<Session> session(s);
    // If `pool` is still alive, release the `Session` to it.
    if (auto shared_pool = pool.lock()) {
      shared_pool->Release(std::move(session));
    }
  });
}

future<StatusOr<google::spanner::v1::BatchCreateSessionsResponse>>
SessionPool::AsyncBatchCreateSessions(
    CompletionQueue& cq, std::shared_ptr<SpannerStub> const& stub,
    std::map<std::string, std::string> const& labels, std::string const& role,
    int num_sessions) {
  google::spanner::v1::BatchCreateSessionsRequest request;
  request.set_database(db_.FullName());
  if (!labels.empty()) {
    request.mutable_session_template()->mutable_labels()->insert(labels.begin(),
                                                                 labels.end());
  }
  if (!role.empty()) {
    request.mutable_session_template()->set_creator_role(role);
  }
  request.set_session_count(std::int32_t{num_sessions});
  return google::cloud::internal::AsyncRetryLoop(
      retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
      Idempotency::kIdempotent, cq,
      [stub](CompletionQueue& cq, std::shared_ptr<grpc::ClientContext> context,
             internal::ImmutableOptions options,
             google::spanner::v1::BatchCreateSessionsRequest const& request) {
        RouteToLeader(*context);  // always for BatchCreateSessions()
        return stub->AsyncBatchCreateSessions(cq, std::move(context),
                                              std::move(options), request);
      },
      internal::SaveCurrentOptions(), std::move(request), __func__);
}

future<Status> SessionPool::AsyncDeleteSession(
    CompletionQueue& cq, std::shared_ptr<SpannerStub> const& stub,
    std::string session_name) {
  google::spanner::v1::DeleteSessionRequest request;
  request.set_name(std::move(session_name));
  return google::cloud::internal::AsyncRetryLoop(
      retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
      Idempotency::kIdempotent, cq,
      [stub](CompletionQueue& cq, std::shared_ptr<grpc::ClientContext> context,
             google::cloud::internal::ImmutableOptions options,
             google::spanner::v1::DeleteSessionRequest const& request) {
        return stub->AsyncDeleteSession(cq, std::move(context),
                                        std::move(options), request);
      },
      internal::SaveCurrentOptions(), std::move(request), __func__);
}

/// Refresh the session `session_name` by executing a `SELECT 1` query on it.
future<StatusOr<google::spanner::v1::ResultSet>>
SessionPool::AsyncRefreshSession(CompletionQueue& cq,
                                 std::shared_ptr<SpannerStub> const& stub,
                                 std::string session_name) {
  google::spanner::v1::ExecuteSqlRequest request;
  request.set_session(std::move(session_name));
  // Single-use transaction with strong concurrency.
  request.set_sql("SELECT 1;");
  request.mutable_request_options()->set_priority(
      google::spanner::v1::RequestOptions::PRIORITY_LOW);
  return google::cloud::internal::AsyncRetryLoop(
      retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
      Idempotency::kIdempotent, cq,
      [stub](CompletionQueue& cq, std::shared_ptr<grpc::ClientContext> context,
             google::cloud::internal::ImmutableOptions options,
             google::spanner::v1::ExecuteSqlRequest const& request) {
        // Read-only transaction, so no route-to-leader.
        return stub->AsyncExecuteSql(cq, std::move(context), std::move(options),
                                     request);
      },
      internal::SaveCurrentOptions(), std::move(request), __func__);
}

Status SessionPool::HandleBatchCreateSessionsDone(
    std::shared_ptr<Channel> const& channel,
    StatusOr<google::spanner::v1::BatchCreateSessionsResponse> response) {
  std::unique_lock<std::mutex> lk(mu_);
  --create_calls_in_progress_;
  if (!response.ok()) {
    return response.status();
  }
  // Add sessions to the pool and update counters for `channel` and the pool.
  auto const sessions_created = response->session_size();
  channel->session_count += sessions_created;
  total_sessions_ += sessions_created;
  sessions_.reserve(sessions_.size() + sessions_created);
  for (auto& session : *response->mutable_session()) {
    sessions_.push_back(std::make_unique<Session>(
        std::move(*session.mutable_name()), channel, clock_));
  }
  // Shuffle the pool so we distribute returned sessions across channels.
  std::shuffle(sessions_.begin(), sessions_.end(), random_generator_);

  // Wake up anyone who was waiting for a `Session`.
  lk.unlock();
  cond_.notify_all();
  return Status();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
