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
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

// StubWrapper could maybe be a Decorator on the Stub
// May have to be a Decorator on Stub to handle accounting destructor for
// streaming
template <typename T>
class StubWrapper {
 public:
  explicit StubWrapper(std::shared_ptr<T> stub)
      : stub_(std::move(stub)), outstanding_rpcs_(0) {}

  int outstanding_rpcs(std::unique_lock<std::mutex> const&) const {
    return outstanding_rpcs_;
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
  int outstanding_rpcs_;
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
    std::chrono::milliseconds resize_cooldown_interval =
        std::chrono::milliseconds(60 * 1000);

    // If the average number of outstanding RPCs is below this threshold,
    // the pool size will be decreased.
    std::size_t minimum_average_outstanding_rpcs_per_channel = 20;
    // If the average number of outstanding RPCs is above this threshold,
    // the pool size will be increased.
    std::size_t maximum_average_outstanding_rpcs_per_channel = 80;

    // When channels are removed from the pool, we have to wait until all
    // outstanding RPCs on that channel are completed before destroying it.
    std::chrono::milliseconds decommissioned_channel_polling_interval =
        std::chrono::milliseconds(30 * 1000);
  };

  static std::shared_ptr<DynamicChannelPool> Create(
      CompletionQueue cq, std::size_t initial_size, StubFactoryFn factory_fn,
      SizingPolicy sizing_policy = {}) {
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    std::vector<std::shared_ptr<StubWrapper<T>>> initial_wrapped_channels;
    for (std::size_t i = 0; i < initial_size; ++i) {
      initial_wrapped_channels.emplace_back(factory_fn());
    }
    auto pool = std::shared_ptr<DynamicChannelPool>(new DynamicChannelPool(
        std::move(cq), std::move(initial_wrapped_channels),
        std::move(factory_fn), std::move(sizing_policy)));
  }

  static std::shared_ptr<DynamicChannelPool> Create(
      CompletionQueue cq, std::vector<std::shared_ptr<T>> initial_channels,
      StubFactoryFn factory_fn, SizingPolicy sizing_policy = {}) {
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    auto pool = std::shared_ptr<DynamicChannelPool>(new DynamicChannelPool(
        std::move(cq), std::move(initial_channels), std::move(factory_fn),
        std::move(sizing_policy)));
    return pool;
  }

  // This is a snapshot aka dirty read as the size could immediately change
  // after this function returns.
  std::size_t size() const {
    std::unique_lock<std::mutex> lk(mu_);
    return channels_.size();
  }

  //  std::shared_ptr<StubWrapper<T>> GetChannel(
  //      std::unique_lock<std::mutex> const&) {
  //    // TODO: check for empty
  //    return channels_[0];
  //  }
  //
  //  std::shared_ptr<StubWrapper<T>> GetChannel(
  //      std::unique_lock<std::mutex> const&, std::size_t index) {
  //    // TODO: bounds check
  //    return channels_[index];
  //  }

  std::shared_ptr<StubWrapper<T>> GetChannelRandomTwoLeastUsed() {
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    std::unique_lock<std::mutex> lk(mu_);

    std::cout << __PRETTY_FUNCTION__ << ": channels_size()=" << channels_.size()
              << std::endl;
    // TODO: check if resize is needed.

    std::vector<std::size_t> indices(channels_.size());
    // TODO(sdhart): Maybe use iota on iterators instead of indices
    std::iota(indices.begin(), indices.end(), 0);
    std::shuffle(indices.begin(), indices.end(), rng_);

    std::shared_ptr<StubWrapper<T>> channel_1 = channels_[indices[0]];
    std::shared_ptr<StubWrapper<T>> channel_2 = channels_[indices[1]];

    return channel_1->outstanding_rpcs(lk) < channel_2->outstanding_rpcs(lk)
               ? channel_1
               : channel_2;
  }

 private:
  DynamicChannelPool(
      CompletionQueue cq,
      std::vector<std::shared_ptr<StubWrapper<T>>> initial_wrapped_channels,
      StubFactoryFn factory_fn, SizingPolicy sizing_policy)
      : cq_(std::move(cq)),
        factory_fn_(std::move(factory_fn)),
        channels_(std::move(initial_wrapped_channels)),
        sizing_policy_(std::move(sizing_policy)),
        next_channel_id_(channels_.size()) {}

  DynamicChannelPool(CompletionQueue cq,
                     std::vector<std::shared_ptr<T>> initial_channels,
                     StubFactoryFn factory_fn, SizingPolicy sizing_policy)
      : cq_(std::move(cq)),
        factory_fn_(std::move(factory_fn)),
        channels_(),
        sizing_policy_(std::move(sizing_policy)),
        next_channel_id_(initial_channels.size()) {
    std::cout << __PRETTY_FUNCTION__ << ": wrap initial_channels" << std::endl;
    channels_.reserve(initial_channels.size());
    for (auto& channel : initial_channels) {
      channels_.push_back(std::make_shared<StubWrapper<T>>(std::move(channel)));
    }
    //    for (auto i = 0; i < channels_.size(); ++i) {
    //        std::cout << __PRETTY_FUNCTION__ << ": channels_[" << i <<
    //        "].get()=" << channels_[i].get() << std::endl;
    //    }
  }

  void ScheduleAddChannel() {}
  void AddChannel() {}

  void ScheduleRemoveChannel() {
    //    std::weak_ptr<DynamicChannelPool<T>> weak = this->shared_from_this();
    decommission_timer_ =
        cq_.MakeRelativeTimer(
               sizing_policy_.decommissioned_channel_polling_interval)
            .then([weak = std::weak_ptr<DynamicChannelPool<T>>(
                       this->shared_from_this())]() {
              if (weak.lock()) {
                weak->RemoveChannel();
              }
            });
  }

  void RemoveChannel() {
    std::unique_lock<std::mutex> lk(mu_);
    std::sort(decommissioned_channels_.begin(), decommissioned_channels_.end(),
              [](std::shared_ptr<StubWrapper<T>> const& a,
                 std::shared_ptr<StubWrapper<T>> b) {
                return a->outstanding_rpcs() > b->outstanding_rpcs();
              });
    while (!decommissioned_channels_.empty()) {
      if (decommissioned_channels_.back()->outstanding_rpcs() != 0) {
        ScheduleRemoveChannel();
        return;
      }
      decommissioned_channels_.pop_back();
    }
  }

  void CheckPoolChannelHealth(std::unique_lock<std::mutex> const&) {
    auto average_rpc_per_channel =
        std::accumulate(channels_.begin(), channels_.end(),
                        [](std::shared_ptr<internal::StubWrapper<T>> const& s) {
                          return s->outstanding_rpcs();
                        }) /
        channels_.size();
    if (average_rpc_per_channel <
        sizing_policy_.minimum_average_outstanding_rpcs_per_channel) {
      // TODO(sdhart): Is there a downside to always removing the most recently
      // created channel?
      decommissioned_channels_.push_back(std::move(channels_.back()));
      channels_.pop_back();
      ScheduleRemoveChannel();
    }
    if (average_rpc_per_channel >
        sizing_policy_.maximum_average_outstanding_rpcs_per_channel) {
      // Channel/stub creation is expensive, instead of making the current RPC
      // wait on this, use an existing channel right now, and schedule a channel
      // to be added.
      ScheduleAddChannel();
    }
  }

  mutable std::mutex mu_;
  CompletionQueue cq_;
  google::cloud::internal::DefaultPRNG rng_;
  StubFactoryFn factory_fn_;
  std::vector<std::shared_ptr<StubWrapper<T>>> channels_;
  SizingPolicy sizing_policy_;
  std::vector<std::shared_ptr<StubWrapper<T>>> decommissioned_channels_;
  future<StatusOr<std::chrono::system_clock::time_point>> decommission_timer_;
  int next_channel_id_;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CHANNEL_POOL_H
