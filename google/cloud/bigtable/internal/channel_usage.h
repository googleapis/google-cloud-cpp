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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_CHANNEL_USAGE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_CHANNEL_USAGE_H

#include "google/cloud/internal/clock.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <memory>
#include <mutex>
#include <utility>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// This class wraps a `T`, typically a BigtableStub, and tracks the number of
// outstanding RPCs by taking measurements when the wrapped stub is acquired
// and released.
template <typename T>
class ChannelUsage : public std::enable_shared_from_this<ChannelUsage<T>> {
 public:
  ChannelUsage() = default;
  explicit ChannelUsage(std::shared_ptr<T> stub) : stub_(std::move(stub)) {}

  // This constructor is only used in testing.
  ChannelUsage(std::shared_ptr<T> stub, int initial_outstanding_rpcs,
               Status last_refresh_status = {})
      : stub_(std::move(stub)),
        outstanding_rpcs_(initial_outstanding_rpcs),
        last_refresh_status_(std::move(last_refresh_status)) {}

  StatusOr<int> instant_outstanding_rpcs() {
    std::scoped_lock lk(mu_);
    if (!last_refresh_status_.ok()) return last_refresh_status_;
    return outstanding_rpcs_;
  }

  ChannelUsage& set_last_refresh_status(Status s) {
    std::scoped_lock lk(mu_);
    last_refresh_status_ = std::move(s);
    return *this;
  }

  // A channel can only be set if the current value is nullptr. This mutator
  // exists only so that we can obtain a std::weak_ptr to the ChannelUsage
  // object that will eventually hold the channel.
  ChannelUsage& set_stub(std::shared_ptr<T> stub) {
    std::scoped_lock lk(mu_);
    if (!stub_) stub_ = std::move(stub);
    return *this;
  }

  std::weak_ptr<ChannelUsage<T>> MakeWeak() { return this->shared_from_this(); }

  std::shared_ptr<T> AcquireStub() {
    std::scoped_lock lk(mu_);
    ++outstanding_rpcs_;
    return stub_;
  }

  void ReleaseStub() {
    std::scoped_lock lk(mu_);
    --outstanding_rpcs_;
  }

 private:
  mutable std::mutex mu_;
  std::shared_ptr<T> stub_;
  int outstanding_rpcs_ = 0;
  Status last_refresh_status_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_CHANNEL_USAGE_H
