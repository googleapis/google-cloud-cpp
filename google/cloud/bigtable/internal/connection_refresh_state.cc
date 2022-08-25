// Copyright 2018 Google Inc.
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

#include "google/cloud/bigtable/internal/connection_refresh_state.h"
#include "google/cloud/log.h"
#include <chrono>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

/**
 * Time after which we bail out waiting for a connection to become ready.
 *
 * This number was copied from the Java client and there doesn't seem to be a
 * well-founded reason for it to be exactly this. It should not bee too large
 * since waiting for a connection to become ready is not cancellable.
 */
auto constexpr kConnectionReadyTimeout = std::chrono::seconds(10);

}  // namespace

ConnectionRefreshState::ConnectionRefreshState(
    std::shared_ptr<internal::CompletionQueueImpl> const& cq_impl,
    std::chrono::milliseconds min_conn_refresh_period,
    std::chrono::milliseconds max_conn_refresh_period)
    : min_conn_refresh_period_(min_conn_refresh_period),
      max_conn_refresh_period_(max_conn_refresh_period),
      rng_(std::random_device{}()),
      timers_(std::make_shared<OutstandingTimers>(cq_impl)) {}

std::chrono::milliseconds ConnectionRefreshState::RandomizedRefreshDelay() {
  std::lock_guard<std::mutex> lk(mu_);
  return std::chrono::milliseconds(
      std::uniform_int_distribution<decltype(max_conn_refresh_period_)::rep>(
          min_conn_refresh_period_.count(),
          max_conn_refresh_period_.count())(rng_));
}

bool ConnectionRefreshState::enabled() const {
  return max_conn_refresh_period_.count() != 0;
}

void ScheduleChannelRefresh(
    std::shared_ptr<internal::CompletionQueueImpl> const& cq_impl,
    std::shared_ptr<ConnectionRefreshState> const& state,
    std::shared_ptr<grpc::Channel> const& channel) {
  // The timers will only hold weak pointers to the channel or to the
  // completion queue, so if either of them are destroyed, the timer chain
  // will simply not continue.
  std::weak_ptr<grpc::Channel> weak_channel(channel);
  std::weak_ptr<internal::CompletionQueueImpl> weak_cq_impl(cq_impl);
  auto cq = CompletionQueue(cq_impl);
  using TimerFuture = future<StatusOr<std::chrono::system_clock::time_point>>;
  auto timer_future =
      cq.MakeRelativeTimer(state->RandomizedRefreshDelay())
          .then([weak_channel, weak_cq_impl, state](TimerFuture fut) {
            if (!fut.get()) {
              // Timer cancelled.
              return;
            }
            auto channel = weak_channel.lock();
            if (!channel) return;
            auto cq_impl = weak_cq_impl.lock();
            if (!cq_impl) return;
            auto cq = CompletionQueue(cq_impl);
            cq.AsyncWaitConnectionReady(
                  channel,
                  std::chrono::system_clock::now() + kConnectionReadyTimeout)
                .then([weak_channel, weak_cq_impl, state](future<Status> fut) {
                  auto conn_status = fut.get();
                  if (!conn_status.ok()) {
                    GCP_LOG(WARNING) << "Failed to refresh connection. Error: "
                                     << conn_status;
                  }
                  auto channel = weak_channel.lock();
                  if (!channel) return;
                  auto cq_impl = weak_cq_impl.lock();
                  if (!cq_impl) return;
                  ScheduleChannelRefresh(cq_impl, state, channel);
                });
          });
  state->timers().RegisterTimer(std::move(timer_future));
}

void OutstandingTimers::RegisterTimer(future<void> fut) {
  std::unique_lock<std::mutex> lk(mu_);
  if (shutdown_) {
    lk.unlock();
    fut.cancel();
    return;
  }

  auto id = id_generator_++;
  auto self = shared_from_this();
  auto timer = fut.then([self, id](future<void>) {
    // If the completion queue is being destroyed, we can afford not
    // ignoring this continuation. Most likely nobody cares anymore.
    auto cq_impl = self->weak_cq_impl_.lock();
    if (!cq_impl) return;
    auto cq = CompletionQueue(cq_impl);
    // Do not run in-line to avoid deadlocks when the timer is immediately
    // satisfied.
    cq.RunAsync([self, id] { self->DeregisterTimer(id); });
  });
  bool const inserted =
      timers_.emplace(std::make_pair(id, std::move(timer))).second;
  if (!inserted) Terminate("Duplicate timer identifier");
}

void OutstandingTimers::DeregisterTimer(std::uint64_t id) {
  std::unique_lock<std::mutex> lk(mu_);
  // `CancelAll` might have emptied the `timers_` map, so `id` might not point
  // to a valid timer, but it's OK.
  timers_.erase(id);
}

void OutstandingTimers::CancelAll() {
  std::unordered_map<std::uint64_t, future<void>> to_cancel;
  {
    std::lock_guard<std::mutex> lk(mu_);
    if (shutdown_) {
      // Already cancelled
      return;
    }
    shutdown_ = true;
    // We don't want to fire the timer continuations with the lock held to avoid
    // deadlocks, so we shouldn't call `cancel()` here.
    to_cancel.swap(timers_);
  }
  for (auto& fut : to_cancel) {
    fut.second.cancel();
  }
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
