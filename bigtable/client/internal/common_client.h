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

#ifndef GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_INTERNAL_COMMON_CLIENT_H_
#define GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_INTERNAL_COMMON_CLIENT_H_

#include "bigtable/client/client_options.h"
#include <grpc++/grpc++.h>

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

/// Create a pool of grpc::Channel objects based on the client options.
std::vector<std::shared_ptr<grpc::Channel>> CreateChannelPool(
    std::string const& endpoint, bigtable::ClientOptions const& options);

/**
 * Refactor implementation of `bigtable::AdminClient` and `bigtable::DataClient`
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
  //@}

  CommonClient(bigtable::ClientOptions options)
      : options_(std::move(options)), current_stub_index_(0) {}

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

  StubPtr Stub() {
    std::unique_lock<std::mutex> lk(mu_);
    if (stubs_.empty()) {
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
      auto channels = CreateChannelPool(Traits::Endpoint(options_), options_);
      std::vector<StubPtr> tmp;
      std::transform(channels.begin(), channels.end(), std::back_inserter(tmp),
                     [](std::shared_ptr<grpc::Channel> ch) {
                       return Interface::NewStub(ch);
                     });
      lk.lock();
      if (stubs_.empty()) {
        tmp.swap(stubs_);
        current_stub_index_ = 0;
      }
    }
    auto stub = stubs_[current_stub_index_];
    // Round robin through the connections.
    if (++current_stub_index_ >= stubs_.size()) {
      current_stub_index_ = 0;
    }
    return stub;
  }

 private:
  std::mutex mu_;
  ClientOptions options_;
  std::vector<StubPtr> stubs_;
  std::size_t current_stub_index_;
};

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_INTERNAL_COMMON_CLIENT_H_
