// Copyright 2020 Google LLC
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

#include "google/cloud/pubsub/testing/test_retry_policies.h"
#include "google/cloud/pubsub/options.h"
#include <chrono>
#include <memory>

namespace google {
namespace cloud {
namespace pubsub_testing {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

Options MakeTestOptions(Options opts) {
  if (!opts.has<pubsub::RetryPolicyOption>()) {
    opts.set<pubsub::RetryPolicyOption>(
        std::make_shared<pubsub::LimitedErrorCountRetryPolicy>(3));
  }
  if (!opts.has<pubsub::BackoffPolicyOption>()) {
    opts.set<pubsub::BackoffPolicyOption>(
        std::make_shared<pubsub::ExponentialBackoffPolicy>(
            std::chrono::microseconds(1), std::chrono::microseconds(1), 2.0));
  }
  if (!opts.has<pubsub::MaxOtelLinkCountOption>()) {
    opts.set<pubsub::MaxOtelLinkCountOption>(128);
  }
  return opts;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_testing
}  // namespace cloud
}  // namespace google
