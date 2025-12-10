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

template <typename T>
class ChannelUsageWrapper
    : public std::enable_shared_from_this<ChannelUsageWrapper<T>> {
 public:
  ChannelUsageWrapper() = default;
  explicit ChannelUsageWrapper(std::shared_ptr<T> stub)
      : stub_(std::move(stub)) {}

  // This value is a snapshot and can change immediately after the lock is
  // released.
  StatusOr<int> outstanding_rpcs() const {
    std::unique_lock<std::mutex> lk(mu_);
    if (!last_refresh_status_.ok()) return last_refresh_status_;
    return outstanding_rpcs_;
  }

  Status LastRefreshStatus() const {
    std::unique_lock<std::mutex> lk(mu_);
    return last_refresh_status_;
  }

  void SetLastRefreshStatus(Status s) {
    std::unique_lock<std::mutex> lk(mu_);
    last_refresh_status_ = std::move(s);
  }

  ChannelUsageWrapper& set_channel(std::shared_ptr<T> channel) {
    stub_ = std::move(channel);
    return *this;
  }

  std::weak_ptr<ChannelUsageWrapper<T>> MakeWeak() {
    return this->shared_from_this();
  }

  std::shared_ptr<T> AcquireStub() {
    std::unique_lock<std::mutex> lk(mu_);
    ++outstanding_rpcs_;
    return stub_;
  }

  void ReleaseStub() {
    std::unique_lock<std::mutex> lk(mu_);
    --outstanding_rpcs_;
  }

 private:
  mutable std::mutex mu_;
  std::shared_ptr<T> stub_;
  int outstanding_rpcs_ = 0;
  Status last_refresh_status_;
};

template <typename T>
class DynamicChannelPool
    : public std::enable_shared_from_this<DynamicChannelPool<T>> {
 public:
  using StubFactoryFn =
      std::function<std::shared_ptr<ChannelUsageWrapper<T>>(std::uint32_t id)>;
  struct SizingPolicy {
    // To avoid channel churn, the pool will not add or remove channels more
    // frequently that this period.
    std::chrono::milliseconds pool_resize_cooldown_interval =
        std::chrono::milliseconds(60 * 1000);

    struct DiscreteChannels {
      int number;
    };
    struct PercentageOfPoolSize {
      double percentage;
    };
    absl::variant<DiscreteChannels, PercentageOfPoolSize>
        channels_to_add_per_resize = DiscreteChannels{1};

    // If the average number of outstanding RPCs is below this threshold,
    // the pool size will be decreased.
    int minimum_average_outstanding_rpcs_per_channel = 20;
    // If the average number of outstanding RPCs is above this threshold,
    // the pool size will be increased.
    int maximum_average_outstanding_rpcs_per_channel = 80;

    // When channels are removed from the pool, we have to wait until all
    // outstanding RPCs on that channel are completed before destroying it.
    std::chrono::milliseconds remove_channel_polling_interval =
        std::chrono::milliseconds(30 * 1000);

    // Limits how large the pool can grow.
    std::size_t maximum_channel_pool_size;

    // This is set to the initial channel pool size.
    std::size_t minimum_channel_pool_size;
  };

  static std::shared_ptr<DynamicChannelPool> Create(
      CompletionQueue cq,
      std::vector<std::shared_ptr<ChannelUsageWrapper<T>>> initial_channels,
      std::shared_ptr<ConnectionRefreshState> refresh_state,
      StubFactoryFn stub_factory_fn, SizingPolicy sizing_policy = {}) {
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
    if (pool_resize_cooldown_timer_ && remove_channel_poll_timer_.valid()) {
      pool_resize_cooldown_timer_->cancel();
    }
  }

  // This is a snapshot aka dirty read as the size could immediately change
  // after this function returns.
  std::size_t size() const {
    std::unique_lock<std::mutex> lk(mu_);
    return channels_.size();
  }

  std::shared_ptr<ChannelUsageWrapper<T>> GetChannelRandomTwoLeastUsed() {
    std::unique_lock<std::mutex> lk(mu_);
    std::cout << __PRETTY_FUNCTION__ << ": channels_size()=" << channels_.size()
              << std::endl;
    //    for (auto const& c : channels_) {
    //      std::cout << __PRETTY_FUNCTION__ << ": channel=" << c.get() <<
    //      std::endl;
    //    }

    if (!pool_resize_cooldown_timer_) {
      CheckPoolChannelHealth(lk);
    } else if (pool_resize_cooldown_timer_->is_ready()) {
      (void)pool_resize_cooldown_timer_->get();
      pool_resize_cooldown_timer_ = absl::nullopt;
      CheckPoolChannelHealth(lk);
    }

    //    std::cout << __PRETTY_FUNCTION__ << ": finished
    //    CheckPoolChannelHealth"
    //              << std::endl;
    std::vector<
        typename std::vector<std::shared_ptr<ChannelUsageWrapper<T>>>::iterator>
        iterators(channels_.size());

    std::iota(iterators.begin(), iterators.end(), channels_.begin());
    std::shuffle(iterators.begin(), iterators.end(), rng_);

    //    typename std::vector<typename std::vector<
    //        std::shared_ptr<ChannelUsageWrapper<T>>>::iterator>::iterator
    auto shuffle_iter = iterators.begin();
    //    typename
    //    std::vector<std::shared_ptr<ChannelUsageWrapper<T>>>::iterator
    auto channel_1 = *shuffle_iter;
    std::shared_ptr<ChannelUsageWrapper<T>> c = *channel_1;
    //    std::cout << __PRETTY_FUNCTION__
    //              << ": check channel 1=" << c.get() << std::endl;
    auto channel_1_rpcs = shuffle_iter != iterators.end()
                              ? (*channel_1)->outstanding_rpcs()
                              : Status{StatusCode::kNotFound, ""};
    ++shuffle_iter;
    //    typename
    //    std::vector<std::shared_ptr<ChannelUsageWrapper<T>>>::iterator
    auto channel_2 = *shuffle_iter;
    // We want to snapshot these outstanding_rpcs values.
    //    std::cout << __PRETTY_FUNCTION__
    //              << ": check channel 2=" << (channel_2)->get() << std::endl;
    auto channel_2_rpcs = shuffle_iter != iterators.end()
                              ? (*channel_2)->outstanding_rpcs()
                              : Status{StatusCode::kNotFound, ""};
    // This is the ideal (and most common ) case so we try it first.
    //    std::cout << __PRETTY_FUNCTION__ << ": compare channel rpcs" <<
    //    std::endl;
    if (channel_1_rpcs.ok() && channel_2_rpcs.ok()) {
      std::cout << __PRETTY_FUNCTION__ << ": 2 ok channels, returning smaller"
                << std::endl;
      return *channel_1_rpcs < *channel_2_rpcs ? *channel_1 : *channel_2;
    }

    // We have one or more bad channels. Spending time finding a good channel
    // will be cheaper than trying to use a bad channel in the long run.
    std::vector<
        typename std::vector<std::shared_ptr<ChannelUsageWrapper<T>>>::iterator>
        bad_channels;
    while (!channel_1_rpcs.ok() && shuffle_iter != iterators.end()) {
      bad_channels.push_back(channel_1);
      ++shuffle_iter;
      channel_1 = *shuffle_iter;
      channel_1_rpcs = shuffle_iter != iterators.end()
                           ? (*channel_1)->outstanding_rpcs()
                           : Status{StatusCode::kNotFound, ""};
    }

    while (!channel_2_rpcs.ok() && shuffle_iter != iterators.end()) {
      bad_channels.push_back(channel_2);
      ++shuffle_iter;
      channel_2 = *shuffle_iter;
      channel_2_rpcs = shuffle_iter != iterators.end()
                           ? (*channel_2)->outstanding_rpcs()
                           : Status{StatusCode::kNotFound, ""};
    }

    if (channel_1_rpcs.ok() && channel_2_rpcs.ok()) {
      std::cout << __PRETTY_FUNCTION__ << ": 2 ok channels" << std::endl;
      return *channel_1_rpcs < *channel_2_rpcs ? *channel_1 : *channel_2;
    }
    if (channel_1_rpcs.ok()) {
      std::cout << __PRETTY_FUNCTION__ << ": ONLY channel_1 ok" << std::endl;
      return *channel_1;
    }
    if (channel_2_rpcs.ok()) {
      std::cout << __PRETTY_FUNCTION__ << ": ONLY channel_2 ok" << std::endl;
      return *channel_2;
    }

    // TODO(sdhart): we have no usable channels in the entire pool; this is bad.
    std::cout << __PRETTY_FUNCTION__ << ": NO USABLE CHANNELS" << std::endl;
    return nullptr;
  }

 private:
  DynamicChannelPool(CompletionQueue cq,
                     std::vector<std::shared_ptr<ChannelUsageWrapper<T>>>
                         initial_wrapped_channels,
                     std::shared_ptr<ConnectionRefreshState> refresh_state,
                     StubFactoryFn stub_factory_fn, SizingPolicy sizing_policy)
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
    std::size_t operator()(typename SizingPolicy::DiscreteChannels const& c) {
      return c.number;
    }

    std::size_t operator()(
        typename SizingPolicy::PercentageOfPoolSize const& c) {
      return static_cast<std::size_t>(
          std::floor(static_cast<double>(pool_size) * c.percentage));
    }
  };

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

  void AddChannel(std::vector<int> const& new_channel_ids) {
    std::vector<std::shared_ptr<ChannelUsageWrapper<T>>> new_stubs;
    new_stubs.reserve(new_channel_ids.size());
    for (auto const& id : new_channel_ids) {
      new_stubs.push_back(stub_factory_fn_(id));
      //          std::make_shared<ChannelUsageWrapper<T>>(stub_factory_fn_(id)));
    }
    std::unique_lock<std::mutex> lk(mu_);
    channels_.insert(channels_.end(),
                     std::make_move_iterator(new_stubs.begin()),
                     std::make_move_iterator(new_stubs.end()));
  }

  void ScheduleRemoveChannel(std::unique_lock<std::mutex> const&) {
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

  void RemoveChannel() {
    std::unique_lock<std::mutex> lk(mu_);
    std::sort(draining_channels_.begin(), draining_channels_.end(),
              [](std::shared_ptr<ChannelUsageWrapper<T>> const& a,
                 std::shared_ptr<ChannelUsageWrapper<T>> b) {
                auto rpcs_a = a->outstanding_rpcs();
                auto rpcs_b = b->outstanding_rpcs();
                if (!rpcs_a.ok()) return false;
                if (!rpcs_b.ok()) return true;
                return *rpcs_a > *rpcs_b;
              });
    while (!draining_channels_.empty()) {
      auto outstanding_rpcs = draining_channels_.back()->outstanding_rpcs();
      if (outstanding_rpcs.ok() && *outstanding_rpcs != 0) {
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

  void SetResizeCooldownTimer(std::unique_lock<std::mutex> const&) {
    pool_resize_cooldown_timer_ =
        cq_.MakeRelativeTimer(sizing_policy_.pool_resize_cooldown_interval);
  }

  void CheckPoolChannelHealth(std::unique_lock<std::mutex> const& lk) {
    int average_rpc_per_channel =
        std::accumulate(channels_.begin(), channels_.end(), 0,
                        [](int a, auto const& b) {
                          auto rpcs_b = b->outstanding_rpcs();
                          return a + (rpcs_b.ok() ? *rpcs_b : 0);
                        }) /
        static_cast<int>(channels_.size());
    std::cout << __PRETTY_FUNCTION__
              << ": channels_.size()=" << channels_.size()
              << "; sizing_policy_.minimum_channel_pool_size="
              << sizing_policy_.minimum_channel_pool_size << std::endl;
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
  std::vector<std::shared_ptr<ChannelUsageWrapper<T>>> channels_;
  SizingPolicy sizing_policy_;
  std::vector<std::shared_ptr<ChannelUsageWrapper<T>>> draining_channels_;
  future<void> remove_channel_poll_timer_;
  absl::optional<future<StatusOr<std::chrono::system_clock::time_point>>>
      pool_resize_cooldown_timer_ = absl::nullopt;
  std::uint32_t next_channel_id_;
  //  std::vector<
  //      typename
  //      std::vector<std::shared_ptr<ChannelUsageWrapper<T>>>::iterator>
  //      iterators_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_DYNAMIC_CHANNEL_POOL_H
