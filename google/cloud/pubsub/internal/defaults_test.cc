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
using std::chrono::seconds;

TEST(OptionsTest, PublisherDefaults) {
  auto opts = DefaultPublisherOptions(Options{});
  EXPECT_LT(0, opts.get<pubsub::MaxHoldTimeOption>().count());
  EXPECT_LT(0L, opts.get<pubsub::MaxBatchMessagesOption>());
  EXPECT_LT(0L, opts.get<pubsub::MaxBatchBytesOption>());
  EXPECT_LT(0L, opts.get<pubsub::MaxPendingBytesOption>());
  EXPECT_LT(0L, opts.get<pubsub::MaxPendingMessagesOption>());
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

TEST(OptionsTest, SubscriberDefaults) {
  auto opts = DefaultSubscriberOptions(Options{});
  EXPECT_EQ(seconds(0), opts.get<pubsub::MaxDeadlineTimeOption>());
  EXPECT_LE(seconds(10), opts.get<pubsub::MaxDeadlineExtensionOption>());
  EXPECT_GE(seconds(600), opts.get<pubsub::MaxDeadlineExtensionOption>());
  EXPECT_LT(0, opts.get<pubsub::MaxOutstandingMessagesOption>());
  EXPECT_LT(0, opts.get<pubsub::MaxOutstandingBytesOption>());
  EXPECT_LT(0, opts.get<pubsub::MaxConcurrencyOption>());
}

TEST(OptionsTest, SubscriberConstraints) {
  auto opts = DefaultSubscriberOptions(
      Options{}
          .set<pubsub::MaxOutstandingMessagesOption>(-1)
          .set<pubsub::MaxOutstandingBytesOption>(-2)
          .set<pubsub::MaxConcurrencyOption>(0));

  EXPECT_EQ(0, opts.get<pubsub::MaxOutstandingMessagesOption>());
  EXPECT_EQ(0, opts.get<pubsub::MaxOutstandingBytesOption>());
  EXPECT_EQ(DefaultMaxConcurrency(), opts.get<pubsub::MaxConcurrencyOption>());

  opts = DefaultSubscriberOptions(
      Options{}.set<pubsub::MaxDeadlineExtensionOption>(seconds(5)));
  EXPECT_EQ(seconds(10), opts.get<pubsub::MaxDeadlineExtensionOption>());

  opts = DefaultSubscriberOptions(
      Options{}.set<pubsub::MaxDeadlineExtensionOption>(seconds(5000)));
  EXPECT_EQ(seconds(600), opts.get<pubsub::MaxDeadlineExtensionOption>());
}

TEST(OptionsTest, UserSetSubscriberOptions) {
  auto opts = DefaultSubscriberOptions(
      Options{}
          .set<pubsub::MaxDeadlineTimeOption>(seconds(2))
          .set<pubsub::MaxDeadlineExtensionOption>(seconds(30))
          .set<pubsub::MaxOutstandingMessagesOption>(4)
          .set<pubsub::MaxOutstandingBytesOption>(5)
          .set<pubsub::MaxConcurrencyOption>(6));

  EXPECT_EQ(seconds(2), opts.get<pubsub::MaxDeadlineTimeOption>());
  EXPECT_EQ(seconds(30), opts.get<pubsub::MaxDeadlineExtensionOption>());
  EXPECT_EQ(4, opts.get<pubsub::MaxOutstandingMessagesOption>());
  EXPECT_EQ(5, opts.get<pubsub::MaxOutstandingBytesOption>());
  EXPECT_EQ(6, opts.get<pubsub::MaxConcurrencyOption>());
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
