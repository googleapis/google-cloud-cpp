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

#include "google/cloud/bigtable/rpc_retry_policy.h"
#include "google/cloud/grpc_utils/grpc_error_delegate.h"
#include <sstream>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
std::unique_ptr<RPCRetryPolicy> DefaultRPCRetryPolicy(
    internal::RPCPolicyParameters defaults) {
  return std::unique_ptr<RPCRetryPolicy>(
      new LimitedTimeRetryPolicy(defaults.maximum_retry_period));
}

std::unique_ptr<RPCRetryPolicy> LimitedErrorCountRetryPolicy::clone() const {
  return std::unique_ptr<RPCRetryPolicy>(
      new LimitedErrorCountRetryPolicy(*this));
}

void LimitedErrorCountRetryPolicy::Setup(grpc::ClientContext&) const {}

bool LimitedErrorCountRetryPolicy::OnFailure(
    google::cloud::Status const& status) {
  return impl_.OnFailure(status);
}

bool LimitedErrorCountRetryPolicy::OnFailure(grpc::Status const& status) {
  return impl_.OnFailure(grpc_utils::MakeStatusFromRpcError(status));
}

LimitedTimeRetryPolicy::LimitedTimeRetryPolicy(
    internal::RPCPolicyParameters defaults)
    : impl_(defaults.maximum_retry_period) {}

std::unique_ptr<RPCRetryPolicy> LimitedTimeRetryPolicy::clone() const {
  return std::unique_ptr<RPCRetryPolicy>(new LimitedTimeRetryPolicy(*this));
}

void LimitedTimeRetryPolicy::Setup(grpc::ClientContext& context) const {
  if (context.deadline() >= impl_.deadline()) {
    context.set_deadline(impl_.deadline());
  }
}

bool LimitedTimeRetryPolicy::OnFailure(google::cloud::Status const& status) {
  return impl_.OnFailure(status);
}

bool LimitedTimeRetryPolicy::OnFailure(grpc::Status const& status) {
  return impl_.OnFailure(grpc_utils::MakeStatusFromRpcError(status));
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
