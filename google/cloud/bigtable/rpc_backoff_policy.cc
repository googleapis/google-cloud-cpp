// Copyright 2017 Google LLC
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

#include "google/cloud/bigtable/rpc_backoff_policy.h"

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
std::unique_ptr<RPCBackoffPolicy> DefaultRPCBackoffPolicy(
    internal::RPCPolicyParameters defaults) {
  return std::unique_ptr<RPCBackoffPolicy>(new ExponentialBackoffPolicy(
      defaults.initial_delay, defaults.maximum_delay));
}

ExponentialBackoffPolicy::ExponentialBackoffPolicy(
    internal::RPCPolicyParameters defaults)
    : ExponentialBackoffPolicy(defaults.initial_delay, defaults.maximum_delay) {
}

std::unique_ptr<RPCBackoffPolicy> ExponentialBackoffPolicy::clone() const {
  return std::unique_ptr<RPCBackoffPolicy>(
      new ExponentialBackoffPolicy(initial_delay_, maximum_delay_));
}

void ExponentialBackoffPolicy::Setup(grpc::ClientContext&) const {}

std::chrono::milliseconds ExponentialBackoffPolicy::OnCompletion(
    google::cloud::Status const&) {
  return impl_.OnCompletion();
}

std::chrono::milliseconds ExponentialBackoffPolicy::OnCompletion(
    grpc::Status const&) {
  return impl_.OnCompletion();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::unique_ptr<internal::BackoffPolicy> MakeCommonBackoffPolicy(
    std::unique_ptr<bigtable::RPCBackoffPolicy> policy) {
  class CommonBackoffPolicy : public internal::BackoffPolicy {
   public:
    explicit CommonBackoffPolicy(
        std::unique_ptr<bigtable::RPCBackoffPolicy> impl)
        : impl_(std::move(impl)) {}
    ~CommonBackoffPolicy() override = default;

    std::unique_ptr<internal::BackoffPolicy> clone() const override {
      return std::make_unique<CommonBackoffPolicy>(impl_->clone());
    }
    std::chrono::milliseconds OnCompletion() override {
      return impl_->OnCompletion();
    }

   private:
    std::unique_ptr<bigtable::RPCBackoffPolicy> impl_;
  };

  return std::make_unique<CommonBackoffPolicy>(std::move(policy));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
