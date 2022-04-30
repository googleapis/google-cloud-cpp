// Copyright 2017 Google Inc.
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_COMMON_CLIENT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_COMMON_CLIENT_H

#include "google/cloud/bigtable/internal/connection_refresh_state.h"
#include "google/cloud/bigtable/internal/defaults.h"
#include "google/cloud/bigtable/options.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/connection_options.h"
#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include <grpcpp/grpcpp.h>
#include <list>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * Refactor implementation of `bigtable::{Data,Admin,InstanceAdmin}Client`.
 *
 * All the clients need to keep a collection (sometimes with a single element)
 * of channels, update the collection when needed and round-robin across the
 * channels. At least `bigtable::DataClient` needs to optimize the creation of
 * the stub objects.
 *
 * The class exposes the channels because they are needed for clients that
 * use more than one type of Stub.
 *
 * @tparam Interface the gRPC object returned by `Stub()`.
 */
template <typename Interface>
class CommonClient {
 public:
  //@{
  /// @name Type traits.
  using StubPtr = std::shared_ptr<typename Interface::StubInterface>;
  using ChannelPtr = std::shared_ptr<grpc::Channel>;
  //@}

  explicit CommonClient(Options opts)
      : opts_(std::move(opts)),
        background_threads_(
            google::cloud::internal::DefaultBackgroundThreads(1)),
        refresh_cq_(
            std::make_shared<CompletionQueue>(background_threads_->cq())),
        refresh_state_(std::make_shared<ConnectionRefreshState>(
            refresh_cq_, opts_.get<MinConnectionRefreshOption>(),
            opts_.get<MaxConnectionRefreshOption>())) {}

  ~CommonClient() {
    // This will stop the refresh of the channels.
    channels_.clear();
    // This will cancel all pending timers.
    refresh_state_->timers().CancelAll();
  }

  /**
   * Reset the channel and stub.
   *
   * This is just used for testing at the moment.  In the future, we expect that
   * the channel and stub will need to be reset under some error conditions
   * and/or when the credentials require explicit refresh.
   */
  void reset() {
    std::lock_guard<std::mutex> lk(mu_);
    stubs_.clear();
  }

  /// Return the next Stub to make a call.
  StubPtr Stub() {
    std::unique_lock<std::mutex> lk(mu_);
    CheckConnections(lk);
    auto stub = stubs_[GetIndex()];
    return stub;
  }

  /// Return the next Channel to make a call.
  ChannelPtr Channel() {
    std::unique_lock<std::mutex> lk(mu_);
    CheckConnections(lk);
    auto channel = channels_[GetIndex()];
    return channel;
  }

  google::cloud::BackgroundThreadsFactory BackgroundThreadsFactory() {
    return google::cloud::internal::MakeBackgroundThreadsFactory(opts_);
  }

 private:
  /// Make sure the connections exit, and create them if needed.
  void CheckConnections(std::unique_lock<std::mutex>& lk) {
    if (!stubs_.empty()) {
      return;
    }
    // Release the lock while making remote calls.  gRPC uses the current
    // thread to make remote connections (and probably authenticate), holding
    // a lock for long operations like that is a bad practice.  Releasing
    // the lock here can result in wasted work, but that is a smaller problem
    // than a deadlock or an unbounded priority inversion.
    // Note that only one connection per application is created by gRPC, even
    // if multiple threads are calling this function at the same time. gRPC
    // only opens one socket per destination+attributes combo, we artificially
    // introduce attributes in the implementation of CreateChannelPool() to
    // create one socket per element in the pool.
    lk.unlock();
    auto channels = CreateChannelPool();
    std::vector<StubPtr> tmp;
    std::transform(channels.begin(), channels.end(), std::back_inserter(tmp),
                   [](std::shared_ptr<grpc::Channel> ch) {
                     return Interface::NewStub(ch);
                   });
    lk.lock();
    if (stubs_.empty()) {
      channels.swap(channels_);
      tmp.swap(stubs_);
      current_index_ = 0;
    } else {
      // Some other thread created the pool and saved it in `stubs_`. The work
      // in this thread was superfluous. We release the lock while clearing the
      // channels to minimize contention.
      lk.unlock();
      tmp.clear();
      channels.clear();
      lk.lock();
    }
  }

  ChannelPtr CreateChannel(int idx) {
    auto args = google::cloud::internal::MakeChannelArguments(opts_);
    args.SetInt(GRPC_ARG_CHANNEL_ID, idx);
    auto res = grpc::CreateCustomChannel(
        opts_.get<EndpointOption>(), opts_.get<GrpcCredentialOption>(), args);
    if (opts_.get<MaxConnectionRefreshOption>().count() == 0) {
      return res;
    }
    ScheduleChannelRefresh(refresh_cq_, refresh_state_, res);
    return res;
  }

  std::vector<std::shared_ptr<grpc::Channel>> CreateChannelPool() {
    std::vector<std::shared_ptr<grpc::Channel>> result;
    for (int i = 0; i != opts_.get<GrpcNumChannelsOption>(); ++i) {
      result.emplace_back(CreateChannel(i));
    }
    return result;
  }

  /// Get the current index for round-robin over connections.
  std::size_t GetIndex() {
    std::size_t current = current_index_++;
    // Round robin through the connections.
    if (current_index_ >= stubs_.size()) {
      current_index_ = 0;
    }
    return current;
  }

  std::mutex mu_;
  std::size_t num_pending_refreshes_ = 0;
  google::cloud::Options opts_;
  std::vector<ChannelPtr> channels_;
  std::vector<StubPtr> stubs_;
  std::size_t current_index_ = 0;
  std::unique_ptr<BackgroundThreads> background_threads_;
  // Timers, which we schedule for refreshes, need to reference the completion
  // queue. We cannot make the completion queue's underlying implementation
  // become owned solely by the operations scheduled on it (because we risk a
  // deadlock). We solve both problems by holding only weak pointers to the
  // completion queue in the operations scheduled on it. In order to do it, we
  // need to hold one instance by a shared pointer.
  std::shared_ptr<CompletionQueue> refresh_cq_;
  std::shared_ptr<ConnectionRefreshState> refresh_state_;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_COMMON_CLIENT_H
