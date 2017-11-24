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

#include <chrono>
#include <sstream>

namespace {

#ifndef BIGTABLE_CLIENT_DEFAULT_MAXIMUM_FAILURES
#define BIGTABLE_CLIENT_DEFAULT_MAXIMUM_FAILURES 1000
#endif  // BIGTABLE_CLIENT_DEFAULT_MAXIMUM_FAILURES

#ifndef BIGTABLE_CLIENT_DEFAULT_INITIAL_DELAY
#define BIGTABLE_CLIENT_DEFAULT_INITIAL_DELAY std::chrono::milliseconds(10)
#endif  // BIGTABLE_CLIENT_DEFAULT_INITIAL_DELAY

#ifndef BIGTABLE_CLIENT_DEFAULT_MAXIMUM_DELAY
#define BIGTABLE_CLIENT_DEFAULT_MAXIMUM_DELAY std::chrono::minutes(5)
#endif  // BIGTABLE_CLIENT_DEFAULT_MAXIMUM_DELAY

constexpr int maximum_failures = BIGTABLE_CLIENT_DEFAULT_MAXIMUM_FAILURES;
const auto initial_delay = BIGTABLE_CLIENT_DEFAULT_INITIAL_DELAY;
const auto maximum_delay = BIGTABLE_CLIENT_DEFAULT_MAXIMUM_DELAY;

}  // anonymous namespace

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {

RPCRetryPolicy::~RPCRetryPolicy() {}

std::unique_ptr<RPCRetryPolicy> DefaultRPCRetryPolicy() {
  return std::unique_ptr<RPCRetryPolicy>(new ExponentialBackoffPolicy(
      maximum_failures, initial_delay, maximum_delay));
}

std::unique_ptr<RPCRetryPolicy> ExponentialBackoffPolicy::clone() const {
  return std::unique_ptr<RPCRetryPolicy>(new ExponentialBackoffPolicy(*this));
}

bool ExponentialBackoffPolicy::on_failure(grpc::Status const& status,
                                          std::chrono::milliseconds* delay) {
  using namespace std::chrono;
  if (status.ok()) {
    *delay = duration_cast<milliseconds>(current_delay_);
    return true;
  }
  auto code = status.error_code();
  if (code != grpc::StatusCode::ABORTED and
      code != grpc::StatusCode::UNAVAILABLE and
      code != grpc::StatusCode::DEADLINE_EXCEEDED) {
    // ... this is a non-recoverable error, abort immediately ...
    return false;
  }
  if (++failure_count_ > maximum_failures_) {
    return false;
  }
  // TODO(coryan) - we need to randomize the sleep period too ...
  *delay = duration_cast<milliseconds>(current_delay_);
  current_delay_ *= 2;
  if (current_delay_ >= maximum_delay_) {
    current_delay_ = maximum_delay_;
  }
  return true;
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
