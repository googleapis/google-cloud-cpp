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
#include "absl/strings/numbers.h"
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
  // On 32-bit machines the address space is quickly consumed by background
  // threads. Create just a few threads by default on such platforms. If the
  // application needs more threads, it can override this default.
  if (std::numeric_limits<std::size_t>::digits < 64) return kDefaultThreadCount;
  auto const n = std::thread::hardware_concurrency();
  return n == 0 ? kDefaultThreadCount : n;
}

Options DefaultCommonOptions(Options opts) {
  opts = internal::PopulateCommonOptions(
      std::move(opts), "", "PUBSUB_EMULATOR_HOST", "", "pubsub.googleapis.com");
  opts = internal::PopulateGrpcOptions(std::move(opts));

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
                                         std::chrono::seconds(60), 4)
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
  if (!opts.has<pubsub::CompressionAlgorithmOption>()) {
    opts.set<pubsub::CompressionAlgorithmOption>(GRPC_COMPRESS_DEFLATE);
  }
  auto e = internal::GetEnv("OTEL_SPAN_LINK_COUNT_LIMIT");
  size_t link_count;
  if (e && absl::SimpleAtoi(e.value(), &link_count)) {
    opts.set<pubsub::MaxOtelLinkCountOption>(link_count);
  } else if (!opts.has<pubsub::MaxOtelLinkCountOption>()) {
    opts.set<pubsub::MaxOtelLinkCountOption>(128);
  }

  return opts;
}

Options DefaultSubscriberOptions(Options opts) {
  return DefaultCommonOptions(DefaultSubscriberOptionsOnly(std::move(opts)));
}

Options DefaultSubscriberOptionsOnly(Options opts) {
  auto defaults =
      Options{}
          .set<pubsub::MaxDeadlineTimeOption>(seconds(0))
          .set<pubsub::MaxDeadlineExtensionOption>(seconds(600))
          .set<pubsub::MinDeadlineExtensionOption>(seconds(60))
          .set<pubsub::MaxOutstandingMessagesOption>(1000)
          .set<pubsub::MaxOutstandingBytesOption>(100 * 1024 * 1024L)
          .set<pubsub::ShutdownPollingPeriodOption>(seconds(5))
          // Subscribers are special: by default we want to retry essentially
          // forever because (a) the service will disconnect the streaming pull
          // from time to time, but that is not a "failure", (b) applications
          // can change this behavior if they need, and this is easier than some
          // hard-coded "treat these disconnects as non-failures" code.
          .set<pubsub::RetryPolicyOption>(pubsub::LimitedErrorCountRetryPolicy(
                                              (std::numeric_limits<int>::max)())
                                              .clone());
  opts = google::cloud::internal::MergeOptions(std::move(opts),
                                               std::move(defaults));

  // Enforce constraints
  if (opts.get<pubsub::MaxConcurrencyOption>() == 0) {
    opts.set<pubsub::MaxConcurrencyOption>(DefaultThreadCount());
  }

  auto& max = opts.lookup<pubsub::MaxDeadlineExtensionOption>();
  max = (std::max)((std::min)(max, seconds(600)), seconds(10));

  auto& min = opts.lookup<pubsub::MinDeadlineExtensionOption>();
  min = (std::max)((std::min)(min, max), seconds(10));

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
