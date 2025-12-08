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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CHANNEL_POOL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CHANNEL_POOL_H

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
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

template <typename T>
class StubUsageWrapper
    : public std::enable_shared_from_this<StubUsageWrapper<T>> {
 public:
  explicit StubUsageWrapper(std::shared_ptr<T> stub) : stub_(std::move(stub)) {}

  // This value is a snapshot and can change immediately after the lock is
  // released.
  int outstanding_rpcs() const {
    std::unique_lock<std::mutex> lk(mu_);
    return outstanding_rpcs_;
  }

  std::weak_ptr<StubUsageWrapper<T>> MakeWeak() {
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
};

template <typename T>
class StaticChannelPool {
 public:
  explicit StaticChannelPool(std::size_t size);

  std::shared_ptr<T> GetChannel();
  std::shared_ptr<T> GetChannel(std::size_t index);

 private:
  std::vector<T> channels_;
};

template <typename T>
class DynamicChannelPool
    : public std::enable_shared_from_this<DynamicChannelPool<T>> {
 public:
  using StubFactoryFn = std::function<std::shared_ptr<T>(int id)>;
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
  };

  static std::shared_ptr<DynamicChannelPool> Create(
      CompletionQueue cq, std::size_t initial_size,
      StubFactoryFn stub_factory_fn, SizingPolicy sizing_policy = {}) {
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    std::vector<std::shared_ptr<StubUsageWrapper<T>>> initial_wrapped_channels;
    for (std::size_t i = 0; i < initial_size; ++i) {
      initial_wrapped_channels.emplace_back(stub_factory_fn());
    }
    auto pool = std::shared_ptr<DynamicChannelPool>(new DynamicChannelPool(
        std::move(cq), std::move(initial_wrapped_channels),
        std::move(stub_factory_fn), std::move(sizing_policy)));
  }

  static std::shared_ptr<DynamicChannelPool> Create(
      CompletionQueue cq, std::vector<std::shared_ptr<T>> initial_channels,
      StubFactoryFn stub_factory_fn, SizingPolicy sizing_policy = {}) {
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    auto pool = std::shared_ptr<DynamicChannelPool>(new DynamicChannelPool(
        std::move(cq), std::move(initial_channels), std::move(stub_factory_fn),
        std::move(sizing_policy)));
    return pool;
  }

  // This is a snapshot aka dirty read as the size could immediately change
  // after this function returns.
  std::size_t size() const {
    std::unique_lock<std::mutex> lk(mu_);
    return channels_.size();
  }

  //  std::shared_ptr<StubUsageWrapper<T>> GetChannel(
  //      std::unique_lock<std::mutex> const&) {
  //    // TODO: check for empty
  //    return channels_[0];
  //  }
  //
  //  std::shared_ptr<StubUsageWrapper<T>> GetChannel(
  //      std::unique_lock<std::mutex> const&, std::size_t index) {
  //    // TODO: bounds check
  //    return channels_[index];
  //  }

  std::shared_ptr<StubUsageWrapper<T>> GetChannelRandomTwoLeastUsed() {
    std::unique_lock<std::mutex> lk(mu_);
    std::cout << __PRETTY_FUNCTION__ << ": channels_size()=" << channels_.size()
              << std::endl;

    if (!pool_resize_cooldown_timer_) {
      CheckPoolChannelHealth(lk);
    } else if (pool_resize_cooldown_timer_->is_ready()) {
      (void)pool_resize_cooldown_timer_->get();
      pool_resize_cooldown_timer_ = absl::nullopt;
      CheckPoolChannelHealth(lk);
    }

    if (channels_.size() == 1) {
      return channels_.front();
    }

    std::shared_ptr<StubUsageWrapper<T>> channel_1;
    std::shared_ptr<StubUsageWrapper<T>> channel_2;
    if (channels_.size() == 2) {
      channel_1 = channels_[0];
      channel_2 = channels_[1];
    } else {
      std::vector<std::size_t> indices(channels_.size());
      // TODO(sdhart): Maybe use iota on iterators instead of indices
      std::iota(indices.begin(), indices.end(), 0);
      std::shuffle(indices.begin(), indices.end(), rng_);

      channel_1 = channels_[indices[0]];
      channel_2 = channels_[indices[1]];
    }
    return channel_1->outstanding_rpcs() < channel_2->outstanding_rpcs()
               ? channel_1
               : channel_2;
  }

 private:
  DynamicChannelPool(CompletionQueue cq,
                     std::vector<std::shared_ptr<StubUsageWrapper<T>>>
                         initial_wrapped_channels,
                     StubFactoryFn stub_factory_fn, SizingPolicy sizing_policy)
      : cq_(std::move(cq)),
        stub_factory_fn_(std::move(stub_factory_fn)),
        channels_(std::move(initial_wrapped_channels)),
        sizing_policy_(std::move(sizing_policy)),
        next_channel_id_(channels_.size()) {}

  DynamicChannelPool(CompletionQueue cq,
                     std::vector<std::shared_ptr<T>> initial_channels,
                     StubFactoryFn stub_factory_fn, SizingPolicy sizing_policy)
      : cq_(std::move(cq)),
        stub_factory_fn_(std::move(stub_factory_fn)),
        channels_(),
        sizing_policy_(std::move(sizing_policy)),
        next_channel_id_(static_cast<int>(initial_channels.size())) {
    std::cout << __PRETTY_FUNCTION__ << ": wrap initial_channels" << std::endl;
    channels_.reserve(initial_channels.size());
    for (auto& channel : initial_channels) {
      channels_.push_back(
          std::make_shared<StubUsageWrapper<T>>(std::move(channel)));
    }
  }

  struct ChannelAddVisitor {
    std::size_t pool_size;
    explicit ChannelAddVisitor(std::size_t pool_size) : pool_size(pool_size) {}
    int operator()(typename SizingPolicy::DiscreteChannels const& c) {
      return c.number;
    }

    int operator()(typename SizingPolicy::PercentageOfPoolSize const& c) {
      return static_cast<int>(
          std::floor(static_cast<double>(pool_size) * c.percentage));
    }
  };

  void ScheduleAddChannel(std::unique_lock<std::mutex> const&) {
    auto num_channels_to_add =
        absl::visit(ChannelAddVisitor(channels_.size()),
                    sizing_policy_.channels_to_add_per_resize);
    std::vector<int> new_channel_ids;
    new_channel_ids.reserve(num_channels_to_add);
    for (int i = 0; i < num_channels_to_add; ++i) {
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
    std::vector<std::shared_ptr<StubUsageWrapper<T>>> new_stubs;
    new_stubs.reserve(new_channel_ids.size());
    for (auto const& id : new_channel_ids) {
      new_stubs.push_back(
          std::make_shared<StubUsageWrapper<T>>(stub_factory_fn_(id)));
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
              [](std::shared_ptr<StubUsageWrapper<T>> const& a,
                 std::shared_ptr<StubUsageWrapper<T>> b) {
                return a->outstanding_rpcs() > b->outstanding_rpcs();
              });
    while (!draining_channels_.empty()) {
      if (draining_channels_.back()->outstanding_rpcs() != 0) {
        ScheduleRemoveChannel(lk);
        return;
      }
      draining_channels_.pop_back();
    }
  }

  void SetResizeCooldownTimer(std::unique_lock<std::mutex> const&) {
    pool_resize_cooldown_timer_ =
        cq_.MakeRelativeTimer(sizing_policy_.pool_resize_cooldown_interval);
  }

  void CheckPoolChannelHealth(std::unique_lock<std::mutex> const& lk) {
    int average_rpc_per_channel =
        std::accumulate(
            channels_.begin(), channels_.end(), 0,
            [](int a, auto const& b) { return a + b->outstanding_rpcs(); }) /
        static_cast<int>(channels_.size());
    if (average_rpc_per_channel <
        sizing_policy_.minimum_average_outstanding_rpcs_per_channel) {
      auto random_channel = std::uniform_int_distribution<std::size_t>(
          0, channels_.size() - 1)(rng_);
      std::swap(channels_[random_channel], channels_.back());
      draining_channels_.push_back(std::move(channels_.back()));
      channels_.pop_back();
      ScheduleRemoveChannel(lk);
      SetResizeCooldownTimer(lk);
    }
    if (average_rpc_per_channel >
        sizing_policy_.maximum_average_outstanding_rpcs_per_channel) {
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
  StubFactoryFn stub_factory_fn_;
  std::vector<std::shared_ptr<StubUsageWrapper<T>>> channels_;
  SizingPolicy sizing_policy_;
  std::vector<std::shared_ptr<StubUsageWrapper<T>>> draining_channels_;
  future<void> remove_channel_poll_timer_;
  absl::optional<future<StatusOr<std::chrono::system_clock::time_point>>>
      pool_resize_cooldown_timer_ = absl::nullopt;
  int next_channel_id_;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CHANNEL_POOL_H
