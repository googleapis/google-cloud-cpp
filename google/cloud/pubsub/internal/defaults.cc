// Copyright 2021 Google LLC
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

#include "google/cloud/pubsub/internal/defaults.h"
#include "google/cloud/pubsub/options.h"
#include "google/cloud/common_options.h"
#include "google/cloud/connection_options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/populate_common_options.h"
#include "google/cloud/internal/populate_grpc_options.h"
#include "google/cloud/internal/user_agent_prefix.h"
#include "google/cloud/options.h"
#include <chrono>
#include <limits>
#include <thread>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using std::chrono::seconds;
using ms = std::chrono::milliseconds;

std::size_t DefaultThreadCount() {
  auto constexpr kDefaultThreadCount = 4;
  auto const n = std::thread::hardware_concurrency();
  return n == 0 ? kDefaultThreadCount : n;
}

Options DefaultCommonOptions(Options opts) {
  opts = internal::PopulateCommonOptions(
      std::move(opts), "", "PUBSUB_EMULATOR_HOST", "", "pubsub.googleapis.com");
  opts = internal::PopulateGrpcOptions(std::move(opts), "PUBSUB_EMULATOR_HOST");

  if (!opts.has<GrpcNumChannelsOption>()) {
    opts.set<GrpcNumChannelsOption>(static_cast<int>(DefaultThreadCount()));
  }
  if (!opts.has<pubsub::RetryPolicyOption>()) {
    opts.set<pubsub::RetryPolicyOption>(
        pubsub::LimitedTimeRetryPolicy(std::chrono::seconds(60)).clone());
  }
  if (!opts.has<pubsub::BackoffPolicyOption>()) {
    opts.set<pubsub::BackoffPolicyOption>(
        pubsub::ExponentialBackoffPolicy(std::chrono::milliseconds(100),
                                         std::chrono::seconds(60), 1.3)
            .clone());
  }
  if (opts.get<GrpcBackgroundThreadPoolSizeOption>() == 0) {
    opts.set<GrpcBackgroundThreadPoolSizeOption>(DefaultThreadCount());
  }

  // Enforce Constraints
  auto& num_channels = opts.lookup<GrpcNumChannelsOption>();
  num_channels = (std::max)(num_channels, 1);

  return opts;
}

Options DefaultPublisherOptions(Options opts) {
  return DefaultCommonOptions(DefaultPublisherOptionsOnly(std::move(opts)));
}

Options DefaultPublisherOptionsOnly(Options opts) {
  if (!opts.has<pubsub::MaxHoldTimeOption>()) {
    opts.set<pubsub::MaxHoldTimeOption>(ms(10));
  }
  if (!opts.has<pubsub::MaxBatchMessagesOption>()) {
    opts.set<pubsub::MaxBatchMessagesOption>(100);
  }
  if (!opts.has<pubsub::MaxBatchBytesOption>()) {
    opts.set<pubsub::MaxBatchBytesOption>(1024 * 1024L);
  }
  if (!opts.has<pubsub::MaxPendingBytesOption>()) {
    opts.set<pubsub::MaxPendingBytesOption>(
        (std::numeric_limits<std::size_t>::max)());
  }
  if (!opts.has<pubsub::MaxPendingMessagesOption>()) {
    opts.set<pubsub::MaxPendingMessagesOption>(
        (std::numeric_limits<std::size_t>::max)());
  }
  if (!opts.has<pubsub::MessageOrderingOption>()) {
    opts.set<pubsub::MessageOrderingOption>(false);
  }
  if (!opts.has<pubsub::FullPublisherActionOption>()) {
    opts.set<pubsub::FullPublisherActionOption>(
        pubsub::FullPublisherAction::kBlocks);
  }

  return opts;
}

Options DefaultSubscriberOptions(Options opts) {
  return DefaultCommonOptions(DefaultSubscriberOptionsOnly(std::move(opts)));
}

Options DefaultSubscriberOptionsOnly(Options opts) {
  if (!opts.has<pubsub::MaxDeadlineTimeOption>()) {
    opts.set<pubsub::MaxDeadlineTimeOption>(seconds(0));
  }
  if (!opts.has<pubsub::MaxDeadlineExtensionOption>()) {
    opts.set<pubsub::MaxDeadlineExtensionOption>(seconds(600));
  }
  if (!opts.has<pubsub::MaxOutstandingMessagesOption>()) {
    opts.set<pubsub::MaxOutstandingMessagesOption>(1000);
  }
  if (!opts.has<pubsub::MaxOutstandingBytesOption>()) {
    opts.set<pubsub::MaxOutstandingBytesOption>(100 * 1024 * 1024L);
  }
  if (opts.get<pubsub::MaxConcurrencyOption>() == 0) {
    opts.set<pubsub::MaxConcurrencyOption>(DefaultThreadCount());
  }
  if (!opts.has<pubsub::ShutdownPollingPeriodOption>()) {
    opts.set<pubsub::ShutdownPollingPeriodOption>(seconds(5));
  }
  // Subscribers are special: by default we want to retry essentially forever
  // because (a) the service will disconnect the streaming pull from time to
  // time, but that is not a "failure", (b) applications can change this
  // behavior if they need, and this is easier than some hard-coded "treat these
  // disconnects as non-failures" code.
  if (!opts.has<pubsub::RetryPolicyOption>()) {
    opts.set<pubsub::RetryPolicyOption>(
        pubsub::LimitedErrorCountRetryPolicy((std::numeric_limits<int>::max)())
            .clone());
  }

  // Enforce constraints
  auto& extension = opts.lookup<pubsub::MaxDeadlineExtensionOption>();
  extension = (std::max)((std::min)(extension, seconds(600)), seconds(10));

  auto& messages = opts.lookup<pubsub::MaxOutstandingMessagesOption>();
  messages = std::max<std::int64_t>(0, messages);

  auto& bytes = opts.lookup<pubsub::MaxOutstandingBytesOption>();
  bytes = std::max<std::int64_t>(0, bytes);

  return opts;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
