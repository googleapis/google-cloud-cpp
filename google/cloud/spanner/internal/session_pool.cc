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
#include "google/cloud/internal/make_unique.h"
#include "google/cloud/status.h"
#include <algorithm>
#include <random>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {

namespace spanner_proto = ::google::spanner::v1;

std::shared_ptr<SessionPool> MakeSessionPool(
    Database db, std::vector<std::shared_ptr<SpannerStub>> stubs,
    SessionPoolOptions options, std::unique_ptr<RetryPolicy> retry_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy) {
  auto pool = std::make_shared<SessionPool>(
      std::move(db), std::move(stubs), std::move(options),
      std::move(retry_policy), std::move(backoff_policy));
  pool->Initialize();
  return pool;
}

SessionPool::SessionPool(Database db,
                         std::vector<std::shared_ptr<SpannerStub>> stubs,
                         SessionPoolOptions options,
                         std::unique_ptr<RetryPolicy> retry_policy,
                         std::unique_ptr<BackoffPolicy> backoff_policy)
    : db_(std::move(db)),
      options_(std::move(
          options.EnforceConstraints(static_cast<int>(stubs.size())))),
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
    channels_.emplace_back(std::move(stub));
  }
  // `channels_` is never resized after this point.
  next_dissociated_stub_channel_ = channels_.begin();
  next_channel_for_create_sessions_ = channels_.begin();
}

void SessionPool::Initialize() {
  // Eagerly initialize the pool with `min_sessions` sessions.
  // TODO(#307) this was moved to `Initialize` in preparation of using
  // `shared_from_this()` in the process of creating sessions, which cannot
  // be done in the constructor.
  if (options_.min_sessions() > 0) {
    std::unique_lock<std::mutex> lk(mu_);
    int num_channels = static_cast<int>(channels_.size());
    int sessions_per_channel = options_.min_sessions() / num_channels;
    // If the number of sessions doesn't divide evenly by the number of
    // channels, add one extra session to the first `extra_sessions` channels.
    int extra_sessions = options_.min_sessions() % num_channels;
    for (auto& channel : channels_) {
      int num_sessions = sessions_per_channel;
      if (extra_sessions > 0) {
        ++num_sessions;
        --extra_sessions;
      }
      // Just ignore failures; we'll try again when the caller requests a
      // session, and we'll be in a position to return an error at that time.
      (void)CreateSessions(lk, channel, options_.labels(), num_sessions);
    }
  }
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
      }
      return {MakeSessionHolder(std::move(session), dissociate_from_pool)};
    }

    // If the pool is at its max size, fail or wait until someone returns a
    // session to the pool then try again.
    if (total_sessions_ >= max_pool_size_) {
      if (options_.action_on_exhaustion() == ActionOnExhaustion::FAIL) {
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

    // Add `min_sessions` to the pool (plus the one we're going to return),
    // subject to the `max_sessions_per_channel` cap.
    ChannelInfo& channel = *next_channel_for_create_sessions_;
    int sessions_to_create =
        (std::min)(options_.min_sessions() + 1,
                   options_.max_sessions_per_channel() - channel.session_count);
    auto create_status =
        CreateSessions(lk, channel, options_.labels(), sessions_to_create);
    if (!create_status.ok()) {
      return create_status;
    }
    // Wake up everyone that was waiting for a session.
    cond_.notify_all();
  }
}

std::shared_ptr<SpannerStub> SessionPool::GetStub(Session const& session) {
  std::shared_ptr<SpannerStub> stub = session.stub();
  if (!stub) {
    // Sessions that were created for partitioned Reads/Queries do not have
    // their own stub; return one to use by round-robining between the channels.
    std::unique_lock<std::mutex> lk(mu_);
    stub = next_dissociated_stub_channel_->stub;
    if (++next_dissociated_stub_channel_ == channels_.end()) {
      next_dissociated_stub_channel_ = channels_.begin();
    }
  }
  return stub;
}

void SessionPool::Release(std::unique_ptr<Session> session) {
  std::unique_lock<std::mutex> lk(mu_);
  if (session->is_bad()) {
    // Once we have support for background processing, we may want to signal
    // that to replenish this bad session.
    --total_sessions_;
    return;
  }
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
    std::unique_lock<std::mutex>& lk, ChannelInfo& channel,
    std::map<std::string, std::string> const& labels, int num_sessions) {
  create_in_progress_ = true;
  lk.unlock();
  spanner_proto::BatchCreateSessionsRequest request;
  request.set_database(db_.FullName());
  request.mutable_session_template()->mutable_labels()->insert(labels.begin(),
                                                               labels.end());
  request.set_session_count(std::int32_t{num_sessions});
  auto const& stub = channel.stub;
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
  channel.session_count += sessions_created;
  total_sessions_ += sessions_created;
  sessions_.reserve(sessions_.size() + sessions_created);
  for (auto& session : *response->mutable_session()) {
    sessions_.push_back(google::cloud::internal::make_unique<Session>(
        std::move(*session.mutable_name()), stub));
  }
  // Shuffle the pool so we distribute returned sessions across channels.
  std::shuffle(sessions_.begin(), sessions_.end(),
               std::mt19937(std::random_device()()));
  if (&*next_channel_for_create_sessions_ == &channel) {
    UpdateNextChannelForCreateSessions();
  }
  return Status();
}

void SessionPool::UpdateNextChannelForCreateSessions() {
  next_channel_for_create_sessions_ = channels_.begin();
  for (auto it = channels_.begin(); it != channels_.end(); ++it) {
    if (it->session_count < next_channel_for_create_sessions_->session_count) {
      next_channel_for_create_sessions_ = it;
    }
  }
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
