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

#include "google/cloud/spanner/internal/session_pool.h"
#include "google/cloud/spanner/internal/connection_impl.h"
#include "google/cloud/spanner/internal/retry_loop.h"
#include "google/cloud/spanner/internal/session.h"
#include "google/cloud/background_threads.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/internal/make_unique.h"
#include "google/cloud/log.h"
#include "google/cloud/status.h"
#include <algorithm>
#include <chrono>
#include <random>
#include <thread>
#include <vector>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {

namespace spanner_proto = ::google::spanner::v1;

std::shared_ptr<SessionPool> MakeSessionPool(
    Database db, std::vector<std::shared_ptr<SpannerStub>> stubs,
    SessionPoolOptions options,
    std::unique_ptr<BackgroundThreads> background_threads,
    std::unique_ptr<RetryPolicy> retry_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy) {
  auto pool = std::make_shared<SessionPool>(
      std::move(db), std::move(stubs), std::move(options),
      std::move(background_threads), std::move(retry_policy),
      std::move(backoff_policy));
  pool->Initialize();
  return pool;
}

SessionPool::SessionPool(Database db,
                         std::vector<std::shared_ptr<SpannerStub>> stubs,
                         SessionPoolOptions options,
                         std::unique_ptr<BackgroundThreads> background_threads,
                         std::unique_ptr<RetryPolicy> retry_policy,
                         std::unique_ptr<BackoffPolicy> backoff_policy)
    : db_(std::move(db)),
      options_(std::move(
          options.EnforceConstraints(static_cast<int>(stubs.size())))),
      background_threads_(std::move(background_threads)),
      retry_policy_prototype_(std::move(retry_policy)),
      backoff_policy_prototype_(std::move(backoff_policy)),
      max_pool_size_(options_.max_sessions_per_channel() *
                     static_cast<int>(stubs.size())) {
  if (stubs.empty()) {
    google::cloud::internal::ThrowInvalidArgument(
        "SessionPool requires a non-empty set of stubs");
  }

  channels_.reserve(stubs.size());
  for (auto& stub : stubs) {
    channels_.push_back(std::make_shared<Channel>(std::move(stub)));
  }
  // `channels_` is never resized after this point.
  next_dissociated_stub_channel_ = channels_.begin();
}

void SessionPool::Initialize() {
  if (options_.min_sessions() > 0) {
    std::unique_lock<std::mutex> lk(mu_);
    (void)Grow(lk, options_.min_sessions(), WaitForSessionAllocation::kWait);
  }
  ScheduleBackgroundWork(std::chrono::seconds(5));
}

SessionPool::~SessionPool() {
  // All references to this object are via `shared_ptr`; since we're in the
  // destructor that implies there can be no concurrent accesses to any member
  // variables, including `current_timer_`.
  //
  // Note that it *is* possible the timer lambda in `ScheduleBackgroundWork`
  // is executing concurrently. However, since we are in the destructor we know
  // that the lambda must not have yet successfully finished a call to `lock()`
  // on the `weak_ptr` to `this` it holds. Any subsequent or in-progress calls
  // must return `nullptr`, and the lambda will not do any work nor reschedule
  // the timer.
  current_timer_.cancel();
}

void SessionPool::ScheduleBackgroundWork(std::chrono::seconds relative_time) {
  // See the comment in the destructor about the thread safety of this method.
  std::weak_ptr<SessionPool> pool = shared_from_this();
  current_timer_ =
      background_threads_->cq()
          .MakeRelativeTimer(relative_time)
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
  // TODO(#1171) Implement SessionPool session refresh
  ScheduleBackgroundWork(std::chrono::seconds(5));
}

// Ensure the pool size conforms to what was specified in the `SessionOptions`,
// creating or deleting sessions as necessary.
void SessionPool::MaintainPoolSize() {
  std::unique_lock<std::mutex> lk(mu_);
  if (!create_in_progress_ && total_sessions_ < options_.min_sessions()) {
    Grow(lk, total_sessions_ - options_.min_sessions(),
         WaitForSessionAllocation::kNoWait);
  }
}

/**
 * Grow the session pool by creating up to `sessions_to_create` sessions and
 * adding them to the pool.  Note that `lk` may be released and reacquired in
 * this method.
 *
 * TODO(#1271) eliminate the `wait` parameter and do all creation
 * asynchronously. The main obstacle is making existing tests pass.
 */
Status SessionPool::Grow(std::unique_lock<std::mutex>& lk,
                         int sessions_to_create,
                         WaitForSessionAllocation wait) {
  int num_channels = static_cast<int>(channels_.size());
  int session_limit = options_.max_sessions_per_channel() * num_channels;
  if (total_sessions_ == session_limit) {
    // Can't grow the pool since we're already at max size.
    return Status(StatusCode::kResourceExhausted, "session pool exhausted");
  }

  create_in_progress_ = true;

  // Compute how many Sessions to create on each Channel, trying to keep the
  // number of Sessions on each channel equal.
  //
  // However, the counts may become unequal over time, and we do not want
  // to delete sessions just to make the counts equal, so do the best we
  // can within those constraints.
  int target_total_sessions =
      (std::min)(total_sessions_ + sessions_to_create, session_limit);

  // Sort the channels in *descending* order of session count.
  std::vector<std::shared_ptr<Channel>> channels_by_count = channels_;
  std::sort(channels_by_count.begin(), channels_by_count.end(),
            [](std::shared_ptr<Channel> const& lhs,
               std::shared_ptr<Channel> const& rhs) {
              // Use `>` to sort in descending order.
              return lhs->session_count > rhs->session_count;
            });

  // Compute the number of new Sessions to create on each channel.
  int sessions_remaining = target_total_sessions;
  int channels_remaining = num_channels;
  std::vector<std::pair<std::shared_ptr<Channel>, int>> create_counts;
  for (auto& channel : channels_by_count) {
    // The target number of sessions for this channel, rounded up.
    int target =
        (sessions_remaining + channels_remaining - 1) / channels_remaining;
    --channels_remaining;
    if (channel->session_count < target) {
      int sessions_to_create = target - channel->session_count;
      create_counts.emplace_back(channel, sessions_to_create);
      // Subtract the number of Sessions this channel will have after creation
      // finishes from the remaining sessions count.
      sessions_remaining -= target;
    } else {
      // This channel is already over its target. Don't create any Sessions
      // on it, just update the remaining sessions count.
      sessions_remaining -= channel->session_count;
    }
  }

  // Create all the sessions (note that `lk` can be released during creation,
  // which is why we don't do this directly in the loop above).
  for (auto& op : create_counts) {
    switch (wait) {
      case WaitForSessionAllocation::kWait: {
        auto status =
            CreateSessions(lk, op.first, options_.labels(), op.second);
        if (!status.ok()) {
          create_in_progress_ = false;
          return status;
        }
      } break;
      case WaitForSessionAllocation::kNoWait:
        // TODO(#1172) make an async create call
        break;
    }
  }

  // TODO(#1172) when we implement kNoWait, setting create_in_progress_ and
  // notifying cond_ should only happen after the calls finish.
  create_in_progress_ = false;
  // Wake up everyone that was waiting for a session.
  cond_.notify_all();
  return Status();
}

StatusOr<SessionHolder> SessionPool::Allocate(bool dissociate_from_pool) {
  std::unique_lock<std::mutex> lk(mu_);
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
      if (options_.action_on_exhaustion() == ActionOnExhaustion::kFail) {
        return Status(StatusCode::kResourceExhausted, "session pool exhausted");
      }
      Wait(lk, [this] {
        return !sessions_.empty() || total_sessions_ < max_pool_size_;
      });
      continue;
    }

    // Create new sessions for the pool.
    //
    // TODO(#307) Currently we only allow one thread to do this at a time; a
    // possible enhancement is tracking the number of waiters and issuing more
    // simulaneous calls if additional sessions are needed. We can also use the
    // number of waiters in the `sessions_to_create` calculation below.
    if (create_in_progress_) {
      Wait(lk, [this] { return !sessions_.empty() || !create_in_progress_; });
      continue;
    }

    // Try to add some sessions to the pool; for now add `min_sessions` plus
    // one for the `Session` this caller is waiting for.
    auto status =
        Grow(lk, options_.min_sessions() + 1, WaitForSessionAllocation::kWait);
    if (!status.ok()) {
      return status;
    }
  }
}

std::shared_ptr<SpannerStub> SessionPool::GetStub(Session const& session) {
  auto const& channel = session.channel();
  if (channel) {
    return channel->stub;
  }

  // Sessions that were created for partitioned Reads/Queries do not have
  // their own channel/stub; return a stub to use by round-robining between
  // the channels.
  std::unique_lock<std::mutex> lk(mu_);
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
//
// Requires `lk` has locked `mu_` prior to this call. `lk` will be dropped
// while the RPC is in progress and then reacquired.
Status SessionPool::CreateSessions(
    std::unique_lock<std::mutex>& lk, std::shared_ptr<Channel> const& channel,
    std::map<std::string, std::string> const& labels, int num_sessions) {
  create_in_progress_ = true;
  lk.unlock();
  spanner_proto::BatchCreateSessionsRequest request;
  request.set_database(db_.FullName());
  request.mutable_session_template()->mutable_labels()->insert(labels.begin(),
                                                               labels.end());
  request.set_session_count(std::int32_t{num_sessions});
  auto const& stub = channel->stub;
  auto response = RetryLoop(
      retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
      true,
      [&stub](grpc::ClientContext& context,
              spanner_proto::BatchCreateSessionsRequest const& request) {
        return stub->BatchCreateSessions(context, request);
      },
      request, __func__);
  lk.lock();
  create_in_progress_ = false;
  if (!response.ok()) {
    return response.status();
  }
  // Add sessions to the pool and update counters for `channel` and the pool.
  int sessions_created = response->session_size();
  channel->session_count += sessions_created;
  total_sessions_ += sessions_created;
  sessions_.reserve(sessions_.size() + sessions_created);
  for (auto& session : *response->mutable_session()) {
    sessions_.push_back(google::cloud::internal::make_unique<Session>(
        std::move(*session.mutable_name()), channel));
  }
  // Shuffle the pool so we distribute returned sessions across channels.
  std::shuffle(sessions_.begin(), sessions_.end(),
               std::mt19937(std::random_device()()));
  return Status();
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

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
