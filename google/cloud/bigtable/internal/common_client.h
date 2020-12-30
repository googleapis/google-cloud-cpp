// Copyright 2017 Google Inc.
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_COMMON_CLIENT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_COMMON_CLIENT_H

#include "google/cloud/bigtable/client_options.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/connection_options.h"
#include "google/cloud/internal/absl_flat_hash_map_quiet.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/log.h"
#include "google/cloud/status_or.h"
#include <grpcpp/grpcpp.h>
#include <list>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

/**
 * Time after which we bail out waiting for a connection to become ready.
 *
 * This number was copied from the Java client and there doesn't seem to be a
 * well-founded reason for it to be exactly this. It should not bee too large
 * since waiting for a connection to become ready is not cancellable.
 */
std::chrono::seconds constexpr kConnectionReadyTimeout(10);

class OutstandingTimers
    : public std::enable_shared_from_this<OutstandingTimers> {
 public:
  explicit OutstandingTimers(std::shared_ptr<CompletionQueue> const& cq)
      : weak_cq_(cq) {}
  // Register a timer. It will automatically deregister on completion.
  void RegisterTimer(future<void> fut);
  // Cancel all currently registered timers and all which will be registered in
  // the future.
  void CancelAll();

 private:
  void DeregisterTimer(std::uint64_t id);
  std::mutex mu_;
  bool shutdown_ = false;           // GUARDED_BY(mu_)
  std::uint64_t id_generator_ = 0;  // GUARDED_BY(mu_)
  absl::flat_hash_map<std::uint64_t,
                      future<void>> timers_;  // GUARDED_BY(mu_)
  // Object of this class is owned by timers continuations, which means it
  // cannot have an owning reference to the `CompletionQueue` because  it would
  // otherwise create a risk of a deadlock on the completion queue destruction.
  std::weak_ptr<CompletionQueue> weak_cq_;  // GUARDED_BY(mu_)
};

/**
 * State required by timers scheduled by `CommonClient`.
 *
 * The scheduled timers might outlive `CommonClient`. They need some shared,
 * persistent state. Objects of this class implement it.
 */
class ConnectionRefreshState {
 public:
  explicit ConnectionRefreshState(
      std::shared_ptr<CompletionQueue> const& cq,
      std::chrono::milliseconds max_conn_refresh_period);
  std::chrono::milliseconds RandomizedRefreshDelay();
  OutstandingTimers& timers() { return *timers_; }

 private:
  std::mutex mu_;
  std::chrono::milliseconds max_conn_refresh_period_;
  google::cloud::internal::DefaultPRNG rng_;
  std::shared_ptr<OutstandingTimers> timers_;
};

/**
 * Schedule a chain of timers to refresh the connection.
 */
void ScheduleChannelRefresh(
    std::shared_ptr<CompletionQueue> const& cq,
    std::shared_ptr<ConnectionRefreshState> const& state,
    std::shared_ptr<grpc::Channel> const& channel);

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
 * @tparam Traits encapsulates variations between the clients.  Currently, which
 *   `*_endpoint()` member function is used.
 * @tparam Interface the gRPC object returned by `Stub()`.
 */
template <typename Traits, typename Interface>
class CommonClient {
 public:
  //@{
  /// @name Type traits.
  using StubPtr = std::shared_ptr<typename Interface::StubInterface>;
  using ChannelPtr = std::shared_ptr<grpc::Channel>;
  //@}

  explicit CommonClient(bigtable::ClientOptions options)
      : options_(std::move(options)),
        current_index_(0),
        background_threads_(
            google::cloud::internal::DefaultBackgroundThreads(1)),
        cq_(std::make_shared<CompletionQueue>(background_threads_->cq())),
        refresh_state_(std::make_shared<ConnectionRefreshState>(
            cq_, options_.max_conn_refresh_period())) {}

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

  ClientOptions& Options() { return options_; }

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

  ChannelPtr CreateChannel(std::size_t idx) {
    auto args = options_.channel_arguments();
    if (!options_.connection_pool_name().empty()) {
      args.SetString("cbt-c++/connection-pool-name",
                     options_.connection_pool_name());
    }
    args.SetInt("cbt-c++/connection-pool-id", static_cast<int>(idx));
    auto res = grpc::CreateCustomChannel(Traits::Endpoint(options_),
                                         options_.credentials(), args);
    if (options_.max_conn_refresh_period().count() == 0) {
      return res;
    }
    ScheduleChannelRefresh(cq_, refresh_state_, res);
    return res;
  }

  std::vector<std::shared_ptr<grpc::Channel>> CreateChannelPool() {
    std::vector<std::shared_ptr<grpc::Channel>> result;
    for (std::size_t i = 0; i != options_.connection_pool_size(); ++i) {
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
  std::size_t num_pending_refreshes_{};
  ClientOptions options_;
  std::vector<ChannelPtr> channels_;
  std::vector<StubPtr> stubs_;
  std::size_t current_index_;
  std::unique_ptr<BackgroundThreads> background_threads_;
  // Timers, which we schedule for refreshes, need to reference the completion
  // queue. We cannot make the completion queue's underlying implementation
  // become owned solely by the operations scheduled on it (because we risk a
  // deadlock). We solve both problems by holding only weak pointers to the
  // completion queue in the operations scheduled on it. In order to do it, we
  // need to hold one instance by a shared pointer.
  std::shared_ptr<CompletionQueue> cq_;
  std::shared_ptr<ConnectionRefreshState> refresh_state_;
};

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_COMMON_CLIENT_H
