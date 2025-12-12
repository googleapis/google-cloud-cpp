// Copyright 2025 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_DYNAMIC_CHANNEL_POOL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_DYNAMIC_CHANNEL_POOL_H

#include "google/cloud/bigtable/internal/connection_refresh_state.h"
#include "google/cloud/bigtable/options.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/internal/clock.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/version.h"
#include <cmath>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

template <typename T>
class ChannelUsage : public std::enable_shared_from_this<ChannelUsage<T>> {
 public:
  using Clock = ::google::cloud::internal::SteadyClock;
  ChannelUsage() : clock_(std::make_shared<Clock>()) {}
  explicit ChannelUsage(std::shared_ptr<T> stub, std::shared_ptr<Clock> clock =
                                                     std::make_shared<Clock>())
      : stub_(std::move(stub)), clock_(std::move(clock)) {}

  StatusOr<int> average_outstanding_rpcs() {
    std::unique_lock<std::mutex> lk(mu_);
    if (!last_refresh_status_.ok()) return last_refresh_status_;
    if (measurements_.empty()) return 0;
    auto time_point = clock_->Now() - std::chrono::seconds(60);
    auto iter = std::find_if(
        measurements_.begin(), measurements_.end(),
        [&](Measurement const& m) { return m.timestamp >= time_point; });
    int num_measurements = 0;
    int last_minute_sum = std::accumulate(
        iter, measurements_.end(), 0, [&](int a, Measurement const& b) mutable {
          ++num_measurements;
          return a + b.outstanding_rpcs;
        });
    measurements_.erase(measurements_.begin(), iter);
    return num_measurements > 0 ? last_minute_sum / num_measurements : 0;
  }

  StatusOr<int> instant_outstanding_rpcs() {
    std::unique_lock<std::mutex> lk(mu_);
    if (!last_refresh_status_.ok()) return last_refresh_status_;
    return outstanding_rpcs_;
  }

  ChannelUsage& set_last_refresh_status(Status s) {
    std::unique_lock<std::mutex> lk(mu_);
    last_refresh_status_ = std::move(s);
    return *this;
  }

  // A channel can only be set if the current value is nullptr. This mutator
  // exists only so that we can obtain a std::weak_ptr to the ChannelUsage
  // object that will eventually hold the channel.
  ChannelUsage& set_channel(std::shared_ptr<T> channel) {
    std::unique_lock<std::mutex> lk(mu_);
    if (!stub_) stub_ = std::move(channel);
    return *this;
  }

  std::weak_ptr<ChannelUsage<T>> MakeWeak() { return this->shared_from_this(); }

  std::shared_ptr<T> AcquireStub() {
    std::unique_lock<std::mutex> lk(mu_);
    ++outstanding_rpcs_;
    auto time = clock_->Now();
    measurements_.emplace_back(outstanding_rpcs_, time);
    return stub_;
  }

  void ReleaseStub() {
    std::unique_lock<std::mutex> lk(mu_);
    --outstanding_rpcs_;
    measurements_.emplace_back(outstanding_rpcs_, clock_->Now());
  }

 private:
  mutable std::mutex mu_;
  std::shared_ptr<T> stub_;
  std::shared_ptr<Clock> clock_;
  int outstanding_rpcs_ = 0;
  Status last_refresh_status_;
  struct Measurement {
    Measurement(int outstanding_rpcs, std::chrono::steady_clock::time_point p)
        : outstanding_rpcs(outstanding_rpcs), timestamp(p) {}
    int outstanding_rpcs;
    std::chrono::steady_clock::time_point timestamp;
  };
  std::deque<Measurement> measurements_;
};

template <typename T>
class DynamicChannelPool
    : public std::enable_shared_from_this<DynamicChannelPool<T>> {
 public:
  using StubFactoryFn = std::function<std::shared_ptr<ChannelUsage<T>>(
      std::uint32_t id, bool prime_channel)>;

  static std::shared_ptr<DynamicChannelPool> Create(
      CompletionQueue cq,
      std::vector<std::shared_ptr<ChannelUsage<T>>> initial_channels,
      std::shared_ptr<ConnectionRefreshState> refresh_state,
      StubFactoryFn stub_factory_fn,
      bigtable::experimental::DynamicChannelPoolSizingPolicy sizing_policy =
          {}) {
    std::cout << __PRETTY_FUNCTION__ << ": enter" << std::endl;
    auto pool = std::shared_ptr<DynamicChannelPool>(new DynamicChannelPool(
        std::move(cq), std::move(initial_channels), std::move(refresh_state),
        std::move(stub_factory_fn), std::move(sizing_policy)));
    std::cout << __PRETTY_FUNCTION__ << ": return pool" << std::endl;
    return pool;
  }

  ~DynamicChannelPool() {
    std::unique_lock<std::mutex> lk(mu_);
    // Eventually the channel refresh chain will terminate after this class is
    // destroyed. But only after the timer futures expire on the CompletionQueue
    // performing this work. We might as well cancel those timer futures now.
    refresh_state_->timers().CancelAll();
    if (remove_channel_poll_timer_.valid()) remove_channel_poll_timer_.cancel();
    if (remove_channel_poll_timer_.valid()) {
      pool_resize_cooldown_timer_.cancel();
    }
  }

  // This is a snapshot aka dirty read as the size could immediately change
  // after this function returns.
  std::size_t size() const {
    std::unique_lock<std::mutex> lk(mu_);
    return channels_.size();
  }

  // If the pool is not under a pool_resize_cooldown_timer_, call
  // CheckPoolChannelHealth.
  // Pick two random channels from channels_ and return the channel with the
  // lower number of outstanding_rpcs. This is the "quick" path.
  // If one or both of the random channels have been marked unhealthy after a
  // refresh, continue choosing random channels to find a pair of healthy
  // channels to compare. Any channels found to be unhealthy are moved from
  // channels_ to draining_channels_ and call ScheduleRemoveChannel.
  // If there is only one health channel in the pool, use it.
  // If there are no healthy channels in channels_, create a new channel and
  // use that one. Also call ScheduleAddChannel to replenish channels_.
  std::shared_ptr<ChannelUsage<T>> GetChannelRandomTwoLeastUsed() {
    std::unique_lock<std::mutex> lk(mu_);
    std::cout << __PRETTY_FUNCTION__ << ": channels_size()=" << channels_.size()
              << std::endl;
    //    for (auto const& c : channels_) {
    //      std::cout << __PRETTY_FUNCTION__ << ": channel=" << c.get() <<
    //      std::endl;
    //    }

    if (!pool_resize_cooldown_timer_.valid()) {
      CheckPoolChannelHealth(lk);
    } else if (pool_resize_cooldown_timer_.is_ready()) {
      (void)pool_resize_cooldown_timer_.get();
      CheckPoolChannelHealth(lk);
    }

    //    std::cout << __PRETTY_FUNCTION__ << ": finished
    //    CheckPoolChannelHealth"
    //              << std::endl;
    std::vector<
        typename std::vector<std::shared_ptr<ChannelUsage<T>>>::iterator>
        iterators(channels_.size());

    std::iota(iterators.begin(), iterators.end(), channels_.begin());
    std::shuffle(iterators.begin(), iterators.end(), rng_);

    //    typename std::vector<typename std::vector<
    //        std::shared_ptr<ChannelUsage<T>>>::iterator>::iterator
    auto shuffle_iter = iterators.begin();
    //    typename
    //    std::vector<std::shared_ptr<ChannelUsage<T>>>::iterator
    auto channel_1_iter = *shuffle_iter;
    std::shared_ptr<ChannelUsage<T>> c = *channel_1_iter;
    //    std::cout << __PRETTY_FUNCTION__
    //              << ": check channel 1=" << c.get() << std::endl;
    auto channel_1_rpcs = shuffle_iter != iterators.end()
                              ? (*channel_1_iter)->average_outstanding_rpcs()
                              : Status{StatusCode::kNotFound, ""};
    ++shuffle_iter;
    //    typename
    //    std::vector<std::shared_ptr<ChannelUsage<T>>>::iterator
    auto channel_2_iter = *shuffle_iter;
    // We want to snapshot these outstanding_rpcs values.
    //    std::cout << __PRETTY_FUNCTION__
    //              << ": check channel 2=" << (channel_2)->get() << std::endl;
    auto channel_2_rpcs = shuffle_iter != iterators.end()
                              ? (*channel_2_iter)->average_outstanding_rpcs()
                              : Status{StatusCode::kNotFound, ""};
    // This is the ideal (and most common ) case so we try it first.
    //    std::cout << __PRETTY_FUNCTION__ << ": compare channel rpcs" <<
    //    std::endl;
    if (channel_1_rpcs.ok() && channel_2_rpcs.ok()) {
      std::cout << __PRETTY_FUNCTION__ << ": 2 ok channels, returning smaller"
                << std::endl;
      return *channel_1_rpcs < *channel_2_rpcs ? *channel_1_iter
                                               : *channel_2_iter;
    }

    // We have one or more bad channels. Spending time finding a good channel
    // will be cheaper than trying to use a bad channel in the long run.
    std::vector<
        typename std::vector<std::shared_ptr<ChannelUsage<T>>>::iterator>
        bad_channel_iters;

    while (!channel_1_rpcs.ok() && shuffle_iter != iterators.end()) {
      bad_channel_iters.push_back(channel_1_iter);
      ++shuffle_iter;
      channel_1_iter = *shuffle_iter;
      channel_1_rpcs = shuffle_iter != iterators.end()
                           ? (*channel_1_iter)->average_outstanding_rpcs()
                           : Status{StatusCode::kNotFound, ""};
    }

    while (!channel_2_rpcs.ok() && shuffle_iter != iterators.end()) {
      bad_channel_iters.push_back(channel_2_iter);
      ++shuffle_iter;
      channel_2_iter = *shuffle_iter;
      channel_2_rpcs = shuffle_iter != iterators.end()
                           ? (*channel_2_iter)->average_outstanding_rpcs()
                           : Status{StatusCode::kNotFound, ""};
    }

    EvictBadChannels(lk, bad_channel_iters);
    ScheduleRemoveChannel(lk);

    if (channel_1_rpcs.ok() && channel_2_rpcs.ok()) {
      std::cout << __PRETTY_FUNCTION__ << ": 2 ok channels" << std::endl;
      return *channel_1_rpcs < *channel_2_rpcs ? *channel_1_iter
                                               : *channel_2_iter;
    }
    if (channel_1_rpcs.ok()) {
      std::cout << __PRETTY_FUNCTION__ << ": ONLY channel_1 ok" << std::endl;
      // Schedule repopulating the pool.
      ScheduleAddChannel(lk);
      return *channel_1_iter;
    }
    if (channel_2_rpcs.ok()) {
      std::cout << __PRETTY_FUNCTION__ << ": ONLY channel_2 ok" << std::endl;
      // Schedule repopulating the pool.
      ScheduleAddChannel(lk);
      return *channel_2_iter;
    }

    // We have no usable channels in the entire pool; this is bad.
    // Create a channel immediately to unblock application.
    std::cout << __PRETTY_FUNCTION__ << ": NO USABLE CHANNELS" << std::endl;
    channels_.push_back(stub_factory_fn_(next_channel_id_++, true));
    std::swap(channels_.front(), channels_.back());
    // Schedule repopulating the pool.
    ScheduleAddChannel(lk);
    return channels_.front();
  }

 private:
  DynamicChannelPool(
      CompletionQueue cq,
      std::vector<std::shared_ptr<ChannelUsage<T>>> initial_wrapped_channels,
      std::shared_ptr<ConnectionRefreshState> refresh_state,
      StubFactoryFn stub_factory_fn,
      bigtable::experimental::DynamicChannelPoolSizingPolicy sizing_policy)
      : cq_(std::move(cq)),
        refresh_state_(std::move(refresh_state)),
        stub_factory_fn_(std::move(stub_factory_fn)),
        channels_(std::move(initial_wrapped_channels)),
        sizing_policy_(std::move(sizing_policy)),
        next_channel_id_(static_cast<std::uint32_t>(channels_.size())) {
    sizing_policy_.minimum_channel_pool_size = channels_.size();
  }

  struct ChannelAddVisitor {
    std::size_t pool_size;
    explicit ChannelAddVisitor(std::size_t pool_size) : pool_size(pool_size) {}
    std::size_t operator()(
        typename bigtable::experimental::DynamicChannelPoolSizingPolicy::
            DiscreteChannels const& c) {
      return c.number;
    }

    std::size_t operator()(
        typename bigtable::experimental::DynamicChannelPoolSizingPolicy::
            PercentageOfPoolSize const& c) {
      return static_cast<std::size_t>(
          std::floor(static_cast<double>(pool_size) * c.percentage));
    }
  };

  // Determines the number of channels to add and reserves the channel ids to
  // be used. Lastly, it calls CompletionQueue::RunAsync with a callback that
  // executes AddChannel with the reserved ids.
  void ScheduleAddChannel(std::unique_lock<std::mutex> const&) {
    std::size_t num_channels_to_add;
    // If we're undersized due to bad channels, get us back to the minimum size.
    if (channels_.size() < sizing_policy_.minimum_channel_pool_size) {
      num_channels_to_add =
          sizing_policy_.minimum_channel_pool_size - channels_.size();
    } else {
      num_channels_to_add =
          std::min(sizing_policy_.maximum_channel_pool_size - channels_.size(),
                   absl::visit(ChannelAddVisitor(channels_.size()),
                               sizing_policy_.channels_to_add_per_resize));
    }
    std::vector<int> new_channel_ids;
    new_channel_ids.reserve(num_channels_to_add);
    for (std::size_t i = 0; i < num_channels_to_add; ++i) {
      new_channel_ids.push_back(next_channel_id_++);
    }

    std::weak_ptr<DynamicChannelPool<T>> foo = this->shared_from_this();
    cq_.RunAsync([new_channel_ids = std::move(new_channel_ids),
                  weak = std::move(foo)]() {
      if (auto self = weak.lock()) {
        self->AddChannel(new_channel_ids);
      }
    });
  }

  // Creates the new channels using the stub_factory_fn and only after that
  // locks the mutex to add the new channels.
  void AddChannel(std::vector<int> const& new_channel_ids) {
    std::vector<std::shared_ptr<ChannelUsage<T>>> new_stubs;
    new_stubs.reserve(new_channel_ids.size());
    for (auto const& id : new_channel_ids) {
      new_stubs.push_back(stub_factory_fn_(id, true));
    }
    std::unique_lock<std::mutex> lk(mu_);
    channels_.insert(channels_.end(),
                     std::make_move_iterator(new_stubs.begin()),
                     std::make_move_iterator(new_stubs.end()));
  }

  // Calls CompletionQueuer::MakeRelativeTimer using
  // remove_channel_polling_interval with a callback that executes
  // RemoveChannel.
  void ScheduleRemoveChannel(std::unique_lock<std::mutex> const&) {
    if (remove_channel_poll_timer_.valid()) return;
    std::cout << __PRETTY_FUNCTION__ << ": set remove_channel_poll_timer"
              << std::endl;
    std::weak_ptr<DynamicChannelPool<T>> foo = this->shared_from_this();
    remove_channel_poll_timer_ =
        cq_.MakeRelativeTimer(sizing_policy_.remove_channel_polling_interval)
            .then(
                [weak = std::move(foo)](
                    future<StatusOr<std::chrono::system_clock::time_point>> f) {
                  if (f.get().ok()) {
                    if (auto self = weak.lock()) {
                      self->RemoveChannel();
                    }
                  }
                });
  }

  // Locks the mutex, reverse sorts draining_channels_, calling pop_back until
  // either draining_channels_ is empty or a channel with outstanding_rpcs is
  // encountered. Calls ScheduleRemoveChannel if draining_channels_ is
  // non-empty.
  void RemoveChannel() {
    std::unique_lock<std::mutex> lk(mu_);
    std::sort(draining_channels_.begin(), draining_channels_.end(),
              [](std::shared_ptr<ChannelUsage<T>> const& a,
                 std::shared_ptr<ChannelUsage<T>> b) {
                auto rpcs_a = a->instant_outstanding_rpcs();
                auto rpcs_b = b->instant_outstanding_rpcs();
                if (!rpcs_a.ok()) return false;
                if (!rpcs_b.ok()) return true;
                return *rpcs_a > *rpcs_b;
              });
    while (!draining_channels_.empty()) {
      auto outstanding_rpcs =
          draining_channels_.back()->instant_outstanding_rpcs();
      if (outstanding_rpcs.ok() && *outstanding_rpcs > 0) {
        ScheduleRemoveChannel(lk);
        return;
      }
      draining_channels_.pop_back();
    }
    // TODO(sdhart): If iterators becomes a member variable perhaps add logic to
    // call
    //  shrink_to_fit on iterators_ if there's a large
    // difference between iterators_.capacity and channels_.size
  }

  void EvictBadChannels(
      std::unique_lock<std::mutex> const&,
      std::vector<
          typename std::vector<std::shared_ptr<ChannelUsage<T>>>::iterator>&
          bad_channel_iters) {
    auto back_iter = channels_.rbegin();
    for (auto& bad_channel_iter : bad_channel_iters) {
      bool swapped = false;
      while (!swapped) {
        auto b = (*back_iter)->instant_outstanding_rpcs();
        if (b.ok()) {
          std::swap(*back_iter, *bad_channel_iter);
          draining_channels_.push_back(std::move(*back_iter));
          swapped = true;
        }
        ++back_iter;
      }
    }
    for (std::size_t i = 0; i < bad_channel_iters.size(); ++i) {
      channels_.pop_back();
    }
  }

  void SetResizeCooldownTimer(std::unique_lock<std::mutex> const&) {
    pool_resize_cooldown_timer_ =
        cq_.MakeRelativeTimer(sizing_policy_.pool_resize_cooldown_interval);
  }

  // Computes the average_rpcs_pre_channel across all channels in the pool,
  // excluding any channels that are awaiting removal in draining_channels_.
  // The computed average is compared to the thresholds in the sizing policy
  // and calls either ScheduleRemoveChannel or ScheduleAddChannel as
  // appropriate. If either is called the resize_cooldown_timer is also set.
  void CheckPoolChannelHealth(std::unique_lock<std::mutex> const& lk) {
    int average_rpc_per_channel =
        std::accumulate(channels_.begin(), channels_.end(), 0,
                        [](int a, auto const& b) {
                          auto rpcs_b = b->average_outstanding_rpcs();
                          return a + (rpcs_b.ok() ? *rpcs_b : 0);
                        }) /
        static_cast<int>(channels_.size());
    std::cout << __PRETTY_FUNCTION__
              << ": channels_.size()=" << channels_.size()
              << "; sizing_policy_.minimum_channel_pool_size="
              << sizing_policy_.minimum_channel_pool_size << std::endl;
    // TODO(sdhart): do we need to check if we're over max pool size here?
    if (average_rpc_per_channel <
            sizing_policy_.minimum_average_outstanding_rpcs_per_channel &&
        channels_.size() > sizing_policy_.minimum_channel_pool_size) {
      auto random_channel = std::uniform_int_distribution<std::size_t>(
          0, channels_.size() - 1)(rng_);
      std::swap(channels_[random_channel], channels_.back());
      draining_channels_.push_back(std::move(channels_.back()));
      channels_.pop_back();
      ScheduleRemoveChannel(lk);
      SetResizeCooldownTimer(lk);
    }
    // TODO(sdhart): do we need to check if we're under min pool size here?
    if (average_rpc_per_channel >
            sizing_policy_.maximum_average_outstanding_rpcs_per_channel &&
        channels_.size() < sizing_policy_.maximum_channel_pool_size) {
      // Channel/stub creation is expensive, instead of making the current RPC
      // wait on this, use an existing channel right now, and schedule a channel
      // to be added.
      ScheduleAddChannel(lk);
      SetResizeCooldownTimer(lk);
    }
  }

  mutable std::mutex mu_;
  CompletionQueue cq_;
  google::cloud::internal::DefaultPRNG rng_;
  std::shared_ptr<ConnectionRefreshState> refresh_state_;
  StubFactoryFn stub_factory_fn_;
  std::vector<std::shared_ptr<ChannelUsage<T>>> channels_;
  bigtable::experimental::DynamicChannelPoolSizingPolicy sizing_policy_;
  std::vector<std::shared_ptr<ChannelUsage<T>>> draining_channels_;
  future<void> remove_channel_poll_timer_;
  future<StatusOr<std::chrono::system_clock::time_point>>
      pool_resize_cooldown_timer_;
  std::uint32_t next_channel_id_;
  //  std::vector<
  //      typename
  //      std::vector<std::shared_ptr<ChannelUsage<T>>>::iterator>
  //      iterators_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_DYNAMIC_CHANNEL_POOL_H
