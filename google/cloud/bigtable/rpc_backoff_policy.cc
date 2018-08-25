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

#include "google/cloud/bigtable/rpc_backoff_policy.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
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
  return std::unique_ptr<RPCBackoffPolicy>(new ExponentialBackoffPolicy(*this));
}

void ExponentialBackoffPolicy::Setup(grpc::ClientContext& /*unused*/) const {}

std::chrono::milliseconds ExponentialBackoffPolicy::OnCompletion(
    grpc::Status const& status) {
  return impl_.OnCompletion();
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
