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
#include <sstream>

namespace {
// Define the defaults using a pre-processor macro, this allows the application
// developers to change the defaults for their application by compiling with
// different values.
#ifndef BIGTABLE_CLIENT_DEFAULT_MAXIMUM_RETRY_PERIOD
#define BIGTABLE_CLIENT_DEFAULT_MAXIMUM_RETRY_PERIOD std::chrono::hours(1)
#endif  // BIGTABLE_CLIENT_DEFAULT_MAXIMUM_RETRY_PERIOD

auto const MAXIMUM_RETRY_PERIOD = BIGTABLE_CLIENT_DEFAULT_MAXIMUM_RETRY_PERIOD;

}  // anonymous namespace

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
std::unique_ptr<RPCRetryPolicy> DefaultRPCRetryPolicy() {
  return std::unique_ptr<RPCRetryPolicy>(new LimitedTimeRetryPolicy);
}

std::unique_ptr<RPCRetryPolicy> LimitedErrorCountRetryPolicy::clone() const {
  return std::unique_ptr<RPCRetryPolicy>(
      new LimitedErrorCountRetryPolicy(*this));
}

void LimitedErrorCountRetryPolicy::Setup(
    grpc::ClientContext& /*unused*/) const {}

bool LimitedErrorCountRetryPolicy::OnFailure(grpc::Status const& status) {
  return impl_.OnFailure(status);
}

LimitedTimeRetryPolicy::LimitedTimeRetryPolicy()
    : impl_(MAXIMUM_RETRY_PERIOD) {}

std::unique_ptr<RPCRetryPolicy> LimitedTimeRetryPolicy::clone() const {
  return std::unique_ptr<RPCRetryPolicy>(new LimitedTimeRetryPolicy(*this));
}

void LimitedTimeRetryPolicy::Setup(grpc::ClientContext& context) const {
  if (context.deadline() >= impl_.deadline()) {
    context.set_deadline(impl_.deadline());
  }
}

bool LimitedTimeRetryPolicy::OnFailure(grpc::Status const& status) {
  return impl_.OnFailure(status);
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
