// Copyright 2018 Google LLC
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

#include "google/cloud/bigtable/internal/async_retry_multi_page.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

MultipagePollingPolicy::MultipagePollingPolicy(
    std::unique_ptr<RPCRetryPolicy> retry,
    std::unique_ptr<RPCBackoffPolicy> backoff)
    : last_was_success_(true),
      rpc_retry_policy_(std::move(retry)),
      rpc_backoff_policy_(backoff->clone()),
      rpc_backoff_policy_prototype_(std::move(backoff)) {}

std::unique_ptr<PollingPolicy> MultipagePollingPolicy::clone() {
  return std::unique_ptr<PollingPolicy>(new MultipagePollingPolicy(
      rpc_retry_policy_->clone(), rpc_backoff_policy_->clone()));
}

bool MultipagePollingPolicy::IsPermanentError(grpc::Status const& status) {
  return RPCRetryPolicy::IsPermanentFailure(status);
}

bool MultipagePollingPolicy::OnFailure(grpc::Status const& status) {
  if (status.ok()) {
    last_was_success_ = true;
    rpc_backoff_policy_ = rpc_backoff_policy_prototype_->clone();
  } else {
    last_was_success_ = false;
  }
  return rpc_retry_policy_->OnFailure(status);
}

bool MultipagePollingPolicy::Exhausted() {
  return not rpc_retry_policy_->OnFailure(grpc::Status::OK);
}

std::chrono::milliseconds MultipagePollingPolicy::WaitPeriod() {
  if (last_was_success_) {
    return std::chrono::milliseconds(0);
  }
  return rpc_backoff_policy_->OnCompletion(grpc::Status::OK);
}

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
