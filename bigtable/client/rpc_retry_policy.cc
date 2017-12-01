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

#include "bigtable/client/rpc_retry_policy.h"

#include <sstream>

namespace {
// Define the defaults using a pre-processor macro, this allows the application
// developers to change the defaults for their application by compiling with
// different values.
#ifndef BIGTABLE_CLIENT_DEFAULT_MAXIMUM_RETRY_PERIOD
#define BIGTABLE_CLIENT_DEFAULT_MAXIMUM_RETRY_PERIOD std::chrono::hours(1)
#endif  // BIGTABLE_CLIENT_DEFAULT_MAXIMUM_RETRY_PERIOD

const auto maximum_retry_period = BIGTABLE_CLIENT_DEFAULT_MAXIMUM_RETRY_PERIOD;

}  // anonymous namespace

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
std::unique_ptr<RPCRetryPolicy> DefaultRPCRetryPolicy() {
  return std::unique_ptr<RPCRetryPolicy>(
      new LimitedTimeRetryPolicy(maximum_retry_period));
}

std::unique_ptr<RPCRetryPolicy> LimitedErrorCountRetryPolicy::clone() const {
  return std::unique_ptr<RPCRetryPolicy>(
      new LimitedErrorCountRetryPolicy(*this));
}

void LimitedErrorCountRetryPolicy::setup(
    grpc::ClientContext& /*unused*/) const {}

bool LimitedErrorCountRetryPolicy::on_failure(grpc::Status const& status) {
  using namespace std::chrono;
  if (not can_retry(status.error_code())) {
    return false;
  }
  return ++failure_count_ <= maximum_failures_;
}

bool LimitedErrorCountRetryPolicy::can_retry(grpc::StatusCode code) const {
  return IsRetryableStatusCode(code);
}

std::unique_ptr<RPCRetryPolicy> LimitedTimeRetryPolicy::clone() const {
  return std::unique_ptr<RPCRetryPolicy>(
      new LimitedTimeRetryPolicy(maximum_duration_));
}

void LimitedTimeRetryPolicy::setup(grpc::ClientContext& context) const {
  if (context.deadline() >= deadline_) {
    context.set_deadline(deadline_);
  }
}

bool LimitedTimeRetryPolicy::on_failure(grpc::Status const& status) {
  using namespace std::chrono;
  if (not can_retry(status.error_code())) {
    return false;
  }
  return std::chrono::system_clock::now() < deadline_;
}

bool LimitedTimeRetryPolicy::can_retry(grpc::StatusCode code) const {
  return IsRetryableStatusCode(code);
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
