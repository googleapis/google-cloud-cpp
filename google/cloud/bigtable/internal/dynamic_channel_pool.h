// Copyright 2026 Google LLC
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

#include "google/cloud/bigtable/instance_resource.h"
#include "google/cloud/bigtable/internal/channel_usage.h"
#include "google/cloud/bigtable/internal/connection_refresh_state.h"
#include "google/cloud/bigtable/internal/stub_manager.h"
#include "google/cloud/bigtable/options.h"
#include "google/cloud/completion_queue.h"
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

//
// This class manages a pool of Stubs wrapped in a ChannelUsage object, and
// selects one for use using a "Random Two Least Used" strategy.
//
// Based on usage data from the ChannelUsage object, the pool will add and
// remove ChannelUsage<T> objects per the configuration present in the
// DynamicChannelPoolSizingPolicyOption.
//
template <typename T>
class DynamicChannelPool
    : public std::enable_shared_from_this<DynamicChannelPool<T>> {
 public:
  // This function should only return an error status if priming is attempted
  // and it is unsuccessful.
  using StubFactoryFn =
      std::function<StatusOr<std::shared_ptr<ChannelUsage<T>>>(
          std::uint32_t id, std::string const& instance_name,
          StubManager::Priming priming)>;

  static std::shared_ptr<DynamicChannelPool> Create(
      std::string const& instance_name, CompletionQueue cq,
      std::vector<std::shared_ptr<ChannelUsage<T>>> initial_channels,
      std::shared_ptr<ConnectionRefreshState> refresh_state,
      StubFactoryFn stub_factory_fn,
      bigtable::experimental::DynamicChannelPoolSizingPolicy sizing_policy =
          {}) {
    auto pool = std::shared_ptr<DynamicChannelPool>(new DynamicChannelPool(
        std::move(instance_name), std::move(cq), std::move(initial_channels),
        std::move(refresh_state), std::move(stub_factory_fn),
        std::move(sizing_policy)));
    return pool;
  }

  ~DynamicChannelPool() {
    std::scoped_lock lk(mu_);
    // Eventually the channel refresh chain will terminate after this class is
    // destroyed. But only after the timer futures expire on the CompletionQueue
    // performing this work. We might as well cancel those timer futures now.
    refresh_state_->timers().CancelAll();
    if (remove_channel_poll_timer_.valid()) remove_channel_poll_timer_.cancel();
    if (pool_size_decrease_cooldown_timer_.valid()) {
      pool_size_decrease_cooldown_timer_.cancel();
    }
  }

  // This is a snapshot aka dirty read as the size could immediately change
  // after this function returns.
  std::size_t size() const {
    std::scoped_lock lk(mu_);
    return channels_.size();
  }

  // This is a snapshot aka dirty read as the size could immediately change
  // after this function returns.
  bool empty() const {
    std::scoped_lock lk(mu_);
    return channels_.empty();
  }

  // Calls CheckPoolChannelHealth before picking a channel.
  //
  // Pick two random channels from channels_ and return the channel with the
  // lower number of outstanding_rpcs. This is the "quick" path.
  //
  // If one or both of the random channels have been marked unhealthy after a
  // refresh, continue choosing random channels to find a pair of healthy
  // channels to compare. Any channels found to be unhealthy are moved from
  // channels_ to draining_channels_ and ScheduleRemoveChannels is called.
  //
  // If there is only one health channel in the pool, use it.
  //
  // If there are no healthy channels in channels_, create a new channel and
  // use that one. Also call ScheduleAddChannels to replenish channels_.
  std::shared_ptr<ChannelUsage<T>> GetChannelRandomTwoLeastUsed() {
    std::scoped_lock lk(mu_);
    CheckPoolChannelHealth(lk);

    ChannelSelectionData d;
    d.iterators.reserve(channels_.size());
    for (auto iter = channels_.begin(); iter != channels_.end(); ++iter) {
      d.iterators.push_back(iter);
    }
    std::shuffle(d.iterators.begin(), d.iterators.end(), rng_);
    d.shuffle_iter = d.iterators.begin();

    if (d.shuffle_iter != d.iterators.end()) {
      d.channel_1_iter = *d.shuffle_iter;
      d.channel_1_rpcs = (*d.channel_1_iter)->instant_outstanding_rpcs();
      ++d.shuffle_iter;
    }

    if (d.shuffle_iter != d.iterators.end()) {
      d.channel_2_iter = *d.shuffle_iter;
      d.channel_2_rpcs = (*d.channel_2_iter)->instant_outstanding_rpcs();
    }

    // This is the most common case so we try it first.
    if (d.channel_1_rpcs.ok() && d.channel_2_rpcs.ok()) {
      return *d.channel_1_rpcs < *d.channel_2_rpcs ? *d.channel_1_iter
                                                   : *d.channel_2_iter;
    }
    if (d.iterators.size() == 1 && d.channel_1_rpcs.ok()) {
      // Pool contains exactly 1 good channel.
      return *d.channel_1_iter;
    }
    if (d.iterators.empty()) {
      // Pool is empty, create a channel immediately and return it. While the
      // return value is a StatusOr<T>, it will only ever contain an error if
      // priming is attempted.
      channels_.push_back(stub_factory_fn_(next_channel_id_++, instance_name_,
                                           StubManager::Priming::kNoPriming)
                              .value());
      return channels_.front();
    }
    return HandleBadChannels(lk, d);
  }

 private:
  friend class DynamicChannelPoolTestWrapper;

  DynamicChannelPool(
      std::string const& instance_name, CompletionQueue cq,
      std::vector<std::shared_ptr<ChannelUsage<T>>> initial_wrapped_channels,
      std::shared_ptr<ConnectionRefreshState> refresh_state,
      StubFactoryFn stub_factory_fn,
      bigtable::experimental::DynamicChannelPoolSizingPolicy sizing_policy)
      : instance_name_(std::move(instance_name)),
        cq_(std::move(cq)),
        refresh_state_(std::move(refresh_state)),
        stub_factory_fn_(std::move(stub_factory_fn)),
        channels_(std::move(initial_wrapped_channels)),
        sizing_policy_(std::move(sizing_policy)),
        next_channel_id_(static_cast<std::uint32_t>(channels_.size())) {
    std::scoped_lock lk(mu_);
    SetSizeDecreaseCooldownTimer(lk);
  }

  struct ChannelSelectionData {
    using ChannelSelect =
        typename std::vector<std::shared_ptr<ChannelUsage<T>>>::iterator;
    std::vector<ChannelSelect> iterators;
    ChannelSelect channel_1_iter;
    ChannelSelect channel_2_iter;
    StatusOr<int> channel_1_rpcs = Status{StatusCode::kNotFound, ""};
    StatusOr<int> channel_2_rpcs = Status{StatusCode::kNotFound, ""};
    typename std::vector<ChannelSelect>::iterator shuffle_iter;

    static void FindGoodChannel(
        std::vector<ChannelSelect>& iterators, ChannelSelect& iter,
        StatusOr<int>& rpcs,
        typename std::vector<ChannelSelect>::iterator& shuffle_iter,
        std::vector<ChannelSelect>& bad_channel_iters) {
      if (!rpcs.ok()) {
        bad_channel_iters.push_back(iter);
        while (shuffle_iter != iterators.end() && !rpcs.ok()) {
          iter = *shuffle_iter;
          rpcs = (*iter)->instant_outstanding_rpcs();
          if (!rpcs.ok()) bad_channel_iters.push_back(iter);
          ++shuffle_iter;
        }
      }
    }
  };

  // We have one or more bad channels. Spending time finding a good channel
  // will be cheaper than trying to use a bad channel in the long run.
  std::shared_ptr<ChannelUsage<T>> HandleBadChannels(
      std::scoped_lock<std::mutex> const& lk, ChannelSelectionData& d) {
    std::vector<typename ChannelSelectionData::ChannelSelect> bad_channel_iters;
    if (d.shuffle_iter != d.iterators.end()) ++d.shuffle_iter;
    ChannelSelectionData::FindGoodChannel(d.iterators, d.channel_1_iter,
                                          d.channel_1_rpcs, d.shuffle_iter,
                                          bad_channel_iters);
    ChannelSelectionData::FindGoodChannel(d.iterators, d.channel_2_iter,
                                          d.channel_2_rpcs, d.shuffle_iter,
                                          bad_channel_iters);

    std::shared_ptr<ChannelUsage<T>> channel;
    if (d.channel_1_rpcs.ok() || d.channel_2_rpcs.ok()) {
      if (d.channel_1_rpcs.ok() && d.channel_2_rpcs.ok()) {
        channel = *d.channel_1_rpcs < *d.channel_2_rpcs ? *d.channel_1_iter
                                                        : *d.channel_2_iter;
      } else if (d.channel_1_rpcs.ok()) {
        channel = *d.channel_1_iter;
      } else if (d.channel_2_rpcs.ok()) {
        channel = *d.channel_2_iter;
      }
      // Wait until we no longer need valid iterators to call EvictBadChannels.
      EvictBadChannels(lk, bad_channel_iters);
    } else {
      // Call EvictBadChannels before we channels_.push_back to avoid
      // invalidating bad_channel_iters if there is a realloc of the vector.
      EvictBadChannels(lk, bad_channel_iters);
      // We have no usable channels in the entire pool; this is bad.
      // Create a channel immediately to unblock application. While the
      // return value is a StatusOr<T>, it will only ever contain an error if
      // priming is attempted.
      channels_.push_back(stub_factory_fn_(next_channel_id_++, instance_name_,
                                           StubManager::Priming::kNoPriming)
                              .value());
      std::swap(channels_.front(), channels_.back());
      channel = channels_.front();
    }
    ScheduleRemoveChannels(lk);
    return channel;
  }

  struct ChannelAddVisitor {
    std::size_t pool_size;
    explicit ChannelAddVisitor(std::size_t pool_size) : pool_size(pool_size) {}
    std::size_t operator()(
        typename bigtable::experimental::DynamicChannelPoolSizingPolicy::
            DiscreteChannels const& c) const {
      return c.number;
    }
    std::size_t operator()(
        typename bigtable::experimental::DynamicChannelPoolSizingPolicy::
            PercentageOfPoolSize const& c) const {
      return static_cast<std::size_t>(
          std::floor(static_cast<double>(pool_size) * c.percentage));
    }
  };

  // Determines the number of channels to add and reserves the channel ids to
  // be used. Lastly, it calls CompletionQueue::RunAsync with a callback that
  // executes AddChannels with the reserved ids.
  void ScheduleAddChannels(
      std::scoped_lock<std::mutex> const&,
      std::function<void(std::vector<int> const&)> const& test_fn = nullptr) {
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
    num_pending_channels_ += num_channels_to_add;
    std::vector<int> new_channel_ids;
    new_channel_ids.reserve(num_channels_to_add);
    for (std::size_t i = 0; i < num_channels_to_add; ++i) {
      new_channel_ids.push_back(next_channel_id_++);
    }

    if (test_fn) test_fn(new_channel_ids);

    std::weak_ptr<DynamicChannelPool<T>> weak_self = this->shared_from_this();
    cq_.RunAsync([new_channel_ids = std::move(new_channel_ids),
                  weak = std::move(weak_self)]() {
      if (auto self = weak.lock()) {
        self->AddChannels(new_channel_ids);
      }
    });
  }

  // Creates the new channels using the stub_factory_fn and only after that
  // locks the mutex to add the new channels.
  void AddChannels(std::vector<int> const& new_channel_ids) {
    std::vector<std::shared_ptr<ChannelUsage<T>>> new_stubs;
    new_stubs.reserve(new_channel_ids.size());
    for (auto const& id : new_channel_ids) {
      auto new_stub = stub_factory_fn_(
          id, instance_name_, StubManager::Priming::kSynchronousPriming);
      if (new_stub.ok()) new_stubs.push_back(*std::move(new_stub));
    }
    std::scoped_lock lk(mu_);
    num_pending_channels_ -= new_channel_ids.size();
    channels_.insert(channels_.end(),
                     std::make_move_iterator(new_stubs.begin()),
                     std::make_move_iterator(new_stubs.end()));
  }

  // Calls CompletionQueuer::MakeRelativeTimer using
  // remove_channel_polling_interval with a callback that executes
  // RemoveChannels.
  void ScheduleRemoveChannels(std::scoped_lock<std::mutex> const&) {
    if (remove_channel_poll_timer_.valid()) return;
    std::weak_ptr<DynamicChannelPool<T>> foo = this->shared_from_this();
    remove_channel_poll_timer_ =
        cq_.MakeRelativeTimer(sizing_policy_.remove_channel_polling_interval)
            .then(
                [weak = std::move(foo)](
                    future<StatusOr<std::chrono::system_clock::time_point>> f) {
                  if (f.get().ok()) {
                    if (auto self = weak.lock()) {
                      self->RemoveChannels();
                    }
                  }
                });
  }

  // Locks the mutex, reverse sorts draining_channels_, calling pop_back until
  // either draining_channels_ is empty or a channel with outstanding_rpcs is
  // encountered. Calls ScheduleRemoveChannels if draining_channels_ is
  // non-empty.
  void RemoveChannels() {
    std::scoped_lock lk(mu_);
    std::sort(draining_channels_.begin(), draining_channels_.end(),
              [](std::shared_ptr<ChannelUsage<T>> const& a,
                 std::shared_ptr<ChannelUsage<T>> const& b) {
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
        ScheduleRemoveChannels(lk);
        return;
      }

      draining_channels_.pop_back();
    }
  }

  void EvictBadChannels(
      std::scoped_lock<std::mutex> const&,
      std::vector<
          typename std::vector<std::shared_ptr<ChannelUsage<T>>>::iterator>&
          bad_channel_iters) {
    auto back_iter = channels_.rbegin();
    for (auto& bad_channel_iter : bad_channel_iters) {
      bool swapped = false;
      while (!swapped && back_iter != channels_.rend()) {
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

  void SetSizeDecreaseCooldownTimer(std::scoped_lock<std::mutex> const&) {
    pool_size_decrease_cooldown_timer_ = cq_.MakeRelativeTimer(
        sizing_policy_.pool_size_decrease_cooldown_interval);
  }

  // Computes the average RPCs per channel across all channels in the pool,
  // by summing the outstanding_rpc from each channel and dividing by the
  // number of active channels plus the num_pending_channels_.
  // Any channels that are awaiting removal in draining_channels_ are excluded
  // from this calculation.
  // The computed average is compared to the thresholds in the sizing policy
  // and calls either ScheduleRemoveChannel or ScheduleAddChannel as
  // appropriate. If ScheduleRemoveChannel is called the resize_cooldown_timer
  // is also set.
  void CheckPoolChannelHealth(std::scoped_lock<std::mutex> const& lk) {
    int average_rpcs_per_channel =
        channels_.empty()
            ? 0
            : std::accumulate(channels_.begin(), channels_.end(), 0,
                              [](int a, auto const& b) {
                                auto rpcs_b = b->instant_outstanding_rpcs();
                                return a + (rpcs_b.ok() ? *rpcs_b : 0);
                              }) /
                  static_cast<int>(channels_.size() + num_pending_channels_);
    if (channels_.size() < sizing_policy_.minimum_channel_pool_size ||
        (average_rpcs_per_channel >
             sizing_policy_.maximum_average_outstanding_rpcs_per_channel &&
         channels_.size() < sizing_policy_.maximum_channel_pool_size)) {
      // Channel/stub creation is expensive, instead of making the current RPC
      // wait on this, use an existing channel right now, and schedule a channel
      // to be added.
      ScheduleAddChannels(lk);
      return;
    }

    if ((!pool_size_decrease_cooldown_timer_.valid() ||
         pool_size_decrease_cooldown_timer_.is_ready()) &&
        average_rpcs_per_channel <
            sizing_policy_.minimum_average_outstanding_rpcs_per_channel &&
        channels_.size() > sizing_policy_.minimum_channel_pool_size) {
      if (pool_size_decrease_cooldown_timer_.is_ready()) {
        pool_size_decrease_cooldown_timer_.get();
      }
      auto random_channel = std::uniform_int_distribution<std::size_t>(
          0, channels_.size() - 1)(rng_);
      std::swap(channels_[random_channel], channels_.back());
      draining_channels_.push_back(std::move(channels_.back()));
      channels_.pop_back();
      ScheduleRemoveChannels(lk);
      SetSizeDecreaseCooldownTimer(lk);
    }
  }

  mutable std::mutex mu_;
  std::string instance_name_;
  CompletionQueue cq_;
  google::cloud::internal::DefaultPRNG rng_;
  std::shared_ptr<ConnectionRefreshState> refresh_state_;
  StubFactoryFn stub_factory_fn_;
  std::vector<std::shared_ptr<ChannelUsage<T>>> channels_;
  std::size_t num_pending_channels_ = 0;
  bigtable::experimental::DynamicChannelPoolSizingPolicy sizing_policy_;
  std::vector<std::shared_ptr<ChannelUsage<T>>> draining_channels_;
  future<void> remove_channel_poll_timer_;
  future<StatusOr<std::chrono::system_clock::time_point>>
      pool_size_decrease_cooldown_timer_;
  std::uint32_t next_channel_id_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_DYNAMIC_CHANNEL_POOL_H
