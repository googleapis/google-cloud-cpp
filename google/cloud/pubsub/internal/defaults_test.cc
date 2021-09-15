// Copyright 2021 Google LLC
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

#include "google/cloud/pubsub/internal/defaults.h"
#include "google/cloud/pubsub/options.h"
#include <gmock/gmock.h>
#include <chrono>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ms = std::chrono::milliseconds;

TEST(OptionsTest, PublisherDefaults) {
  auto opts = DefaultPublisherOptions(Options{});
  EXPECT_EQ(ms(10), opts.get<pubsub::MaxHoldTimeOption>());
  EXPECT_EQ(100, opts.get<pubsub::MaxBatchMessagesOption>());
  EXPECT_EQ(1024 * 1024L, opts.get<pubsub::MaxBatchBytesOption>());
  EXPECT_EQ((std::numeric_limits<std::size_t>::max)(),
            opts.get<pubsub::MaxPendingBytesOption>());
  EXPECT_EQ((std::numeric_limits<std::size_t>::max)(),
            opts.get<pubsub::MaxPendingMessagesOption>());
  EXPECT_EQ(false, opts.get<pubsub::MessageOrderingOption>());
  EXPECT_EQ(pubsub::FullPublisherAction::kBlocks,
            opts.get<pubsub::FullPublisherActionOption>());
}

TEST(OptionsTest, UserSetPublisherOptions) {
  auto opts =
      DefaultPublisherOptions(Options{}
                                  .set<pubsub::MaxHoldTimeOption>(ms(100))
                                  .set<pubsub::MaxBatchMessagesOption>(1)
                                  .set<pubsub::MaxBatchBytesOption>(2)
                                  .set<pubsub::MaxPendingBytesOption>(3)
                                  .set<pubsub::MaxPendingMessagesOption>(4)
                                  .set<pubsub::MessageOrderingOption>(true)
                                  .set<pubsub::FullPublisherActionOption>(
                                      pubsub::FullPublisherAction::kIgnored));

  EXPECT_EQ(ms(100), opts.get<pubsub::MaxHoldTimeOption>());
  EXPECT_EQ(1U, opts.get<pubsub::MaxBatchMessagesOption>());
  EXPECT_EQ(2U, opts.get<pubsub::MaxBatchBytesOption>());
  EXPECT_EQ(3U, opts.get<pubsub::MaxPendingBytesOption>());
  EXPECT_EQ(4U, opts.get<pubsub::MaxPendingMessagesOption>());
  EXPECT_EQ(true, opts.get<pubsub::MessageOrderingOption>());
  EXPECT_EQ(pubsub::FullPublisherAction::kIgnored,
            opts.get<pubsub::FullPublisherActionOption>());
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
