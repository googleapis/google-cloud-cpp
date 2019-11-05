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

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {

namespace spanner_proto = ::google::spanner::v1;

SessionPool::SessionPool(Database db, std::shared_ptr<SpannerStub> stub,
                         std::unique_ptr<RetryPolicy> retry_policy,
                         std::unique_ptr<BackoffPolicy> backoff_policy,
                         SessionPoolOptions options)
    : db_(std::move(db)),
      stub_(std::move(stub)),
      retry_policy_(std::move(retry_policy)),
      backoff_policy_(std::move(backoff_policy)),
      options_(options) {
  // Ensure the options have sensible values.
  options_.min_sessions = (std::max)(options_.min_sessions, 0);
  options_.max_sessions = (std::max)(options_.max_sessions, 1);
  if (options_.max_sessions < options_.min_sessions) {
    options_.max_sessions = options_.min_sessions;
  }
  options_.max_idle_sessions = (std::max)(options_.max_idle_sessions, 0);
  options_.write_sessions_fraction =
      (std::max)(options_.write_sessions_fraction, 0.0);
  options_.write_sessions_fraction =
      (std::min)(options_.write_sessions_fraction, 1.0);

  // Eagerly initialize the pool with `min_sessions` sessions.
  if (options_.min_sessions == 0) {
    return;
  }
  auto sessions = CreateSessions(options_.min_sessions);
  if (sessions.ok()) {
    std::unique_lock<std::mutex> lk(mu_);
    total_sessions_ += static_cast<int>(sessions->size());
    sessions_.insert(sessions_.end(),
                     std::make_move_iterator(sessions->begin()),
                     std::make_move_iterator(sessions->end()));
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
    if (total_sessions_ >= options_.max_sessions) {
      if (options_.action_on_exhaustion == ActionOnExhaustion::FAIL) {
        return Status(StatusCode::kResourceExhausted, "session pool exhausted");
      }
      cond_.wait(lk, [this] {
        return !sessions_.empty() || total_sessions_ < options_.max_sessions;
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
      cond_.wait(lk,
                 [this] { return !sessions_.empty() || !create_in_progress_; });
      continue;
    }

    // Add `min_sessions` to the pool (plus the one we're going to return),
    // subject to the `max_sessions` cap.
    int sessions_to_create = (std::min)(
        options_.min_sessions + 1, options_.max_sessions - total_sessions_);
    create_in_progress_ = true;
    lk.unlock();
    // TODO(#307) do we need to limit the call rate here?
    auto sessions = CreateSessions(sessions_to_create);
    lk.lock();
    create_in_progress_ = false;
    if (!sessions.ok() || sessions->empty()) {
      // If the error was anything other than ResourceExhausted, return it.
      if (sessions.status().code() != StatusCode::kResourceExhausted) {
        return sessions.status();
      }
      // Fail if we're supposed to, otherwise try again.
      if (options_.action_on_exhaustion == ActionOnExhaustion::FAIL) {
        return Status(StatusCode::kResourceExhausted, "session pool exhausted");
      }
      continue;
    }

    total_sessions_ += static_cast<int>(sessions->size());
    // Return one of the sessions and add the rest to the pool.
    auto session = std::move(sessions->back());
    sessions->pop_back();
    if (dissociate_from_pool) {
      --total_sessions_;
    }
    if (!sessions->empty()) {
      sessions_.insert(sessions_.end(),
                       std::make_move_iterator(sessions->begin()),
                       std::make_move_iterator(sessions->end()));
      lk.unlock();
      // Wake up everyone that was waiting for a session.
      cond_.notify_all();
    }
    return {MakeSessionHolder(std::move(session), dissociate_from_pool)};
  }
}

void SessionPool::Release(Session* session) {
  std::unique_lock<std::mutex> lk(mu_);
  bool notify = sessions_.empty();
  sessions_.emplace_back(session);
  // If sessions_ was empty, wake up someone who was waiting for a session.
  if (notify) {
    lk.unlock();
    cond_.notify_one();
  }
}

StatusOr<std::vector<std::unique_ptr<Session>>> SessionPool::CreateSessions(
    int num_sessions) {
  spanner_proto::BatchCreateSessionsRequest request;
  request.set_database(db_.FullName());
  request.set_session_count(std::int32_t{num_sessions});
  auto response = RetryLoop(
      retry_policy_->clone(), backoff_policy_->clone(), true,
      [this](grpc::ClientContext& context,
             spanner_proto::BatchCreateSessionsRequest const& request) {
        return stub_->BatchCreateSessions(context, request);
      },
      request, __func__);
  if (!response) {
    return response.status();
  }
  std::vector<std::unique_ptr<Session>> sessions;
  sessions.reserve(response->session_size());
  for (auto& session : *response->mutable_session()) {
    sessions.push_back(google::cloud::internal::make_unique<Session>(
        std::move(*session.mutable_name())));
  }
  return {std::move(sessions)};
}

SessionHolder SessionPool::MakeSessionHolder(std::unique_ptr<Session> session,
                                             bool dissociate_from_pool) {
  if (dissociate_from_pool) {
    // Uses the default deleter; the `Session` is not returned to the pool.
    return {std::move(session)};
  }
  std::weak_ptr<SessionPool> pool = shared_from_this();
  return SessionHolder(session.release(), [pool](Session* session) {
    auto shared_pool = pool.lock();
    // If `pool` is still alive, release the `Session` to it; otherwise just
    // delete the `Session`.
    if (shared_pool) {
      shared_pool->Release(session);
    } else {
      delete session;
    }
  });
}

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
