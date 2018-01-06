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

#include <grpc++/grpc++.h>
#include "bigtable/client/client_options.h"

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {
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
  CommonClient(bigtable::ClientOptions options)
      : options_(std::move(options)) {}

  using StubPtr = std::shared_ptr<typename Interface::StubInterface>;

  /**
   * Reset the channel and stub.
   *
   * This is just used for testing at the moment.  In the future, we expect that
   * the channel and stub will need to be reset under some error conditions
   * and/or when the credentials require explicit refresh.
   */
  void reset() {
    stub_.reset();
    channel_.reset();
  }

  StubPtr Stub() {
    if (not stub_) {
      auto channel = grpc::CreateCustomChannel(Traits::endpoint(options_),
                                               options_.credentials(),
                                               options_.channel_arguments());
      stub_ = Interface::NewStub(channel);
      channel_ = channel;
    }
    return stub_;
  }

 private:
  ClientOptions options_;
  std::shared_ptr<grpc::Channel> channel_;
  StubPtr stub_;
};

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_INTERNAL_COMMON_CLIENT_H_
