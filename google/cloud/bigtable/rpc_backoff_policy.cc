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

namespace {
// Define the defaults using a pre-processor macro, this allows the application
// developers to change the defaults for their application by compiling with
// different values.
#ifndef BIGTABLE_CLIENT_DEFAULT_INITIAL_DELAY
#define BIGTABLE_CLIENT_DEFAULT_INITIAL_DELAY std::chrono::milliseconds(10)
#endif  // BIGTABLE_CLIENT_DEFAULT_INITIAL_DELAY

#ifndef BIGTABLE_CLIENT_DEFAULT_MAXIMUM_DELAY
#define BIGTABLE_CLIENT_DEFAULT_MAXIMUM_DELAY std::chrono::minutes(5)
#endif  // BIGTABLE_CLIENT_DEFAULT_MAXIMUM_DELAY

auto const DEFAULT_INITIAL_DELAY = BIGTABLE_CLIENT_DEFAULT_INITIAL_DELAY;
auto const DEFAULT_MAXIMUM_DELAY = BIGTABLE_CLIENT_DEFAULT_MAXIMUM_DELAY;
}  // anonymous namespace

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
std::unique_ptr<RPCBackoffPolicy> DefaultRPCBackoffPolicy() {
  return std::unique_ptr<RPCBackoffPolicy>(new ExponentialBackoffPolicy(
      DEFAULT_INITIAL_DELAY, DEFAULT_MAXIMUM_DELAY));
}

ExponentialBackoffPolicy::ExponentialBackoffPolicy()
    : ExponentialBackoffPolicy(DEFAULT_INITIAL_DELAY, DEFAULT_MAXIMUM_DELAY) {}

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
