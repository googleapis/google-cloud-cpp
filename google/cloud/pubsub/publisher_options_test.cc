// Copyright 2020 Google LLC
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

#include "google/cloud/pubsub/publisher_options.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

TEST(PublisherOptions, Setters) {
  auto const b0 = PublisherOptions{};
  EXPECT_NE(0, b0.maximum_hold_time().count());
  EXPECT_FALSE(b0.message_ordering());

  auto const b = PublisherOptions{}
                     .set_maximum_hold_time(std::chrono::seconds(12))
                     .set_maximum_batch_bytes(123)
                     .set_maximum_batch_message_count(10)
                     .enable_message_ordering();
  EXPECT_EQ(10, b.maximum_batch_message_count());
  EXPECT_EQ(123, b.maximum_batch_bytes());
  auto const expected = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::seconds(12));
  EXPECT_EQ(expected, b.maximum_hold_time());
  EXPECT_TRUE(b.message_ordering());

  auto const b1 =
      PublisherOptions{}.enable_message_ordering().disable_message_ordering();
  EXPECT_FALSE(b1.message_ordering());
}

TEST(PublisherOptions, MaximumPendingBytes) {
  auto const b0 = PublisherOptions{};
  EXPECT_NE(0, b0.maximum_pending_bytes());

  auto const b1 = PublisherOptions{}.set_maximum_pending_bytes(10000000);
  EXPECT_EQ(10000000, b1.maximum_pending_bytes());

  auto const b2 = PublisherOptions{}.set_maximum_pending_bytes(0);
  EXPECT_EQ(0, b2.maximum_pending_bytes());
}

TEST(PublisherOptions, MaximumPendingMessages) {
  auto const b0 = PublisherOptions{};
  EXPECT_NE(0, b0.maximum_pending_messages());

  auto const b1 = PublisherOptions{}.set_maximum_pending_messages(1000);
  EXPECT_EQ(1000, b1.maximum_pending_messages());

  auto const b2 = PublisherOptions{}.set_maximum_pending_messages(0);
  EXPECT_EQ(0, b2.maximum_pending_messages());
}

TEST(PublisherOptions, FullPublisherAction) {
  auto const b0 = PublisherOptions{};
  EXPECT_FALSE(b0.full_publisher_ignored());
  EXPECT_FALSE(b0.full_publisher_rejects());
  EXPECT_TRUE(b0.full_publisher_blocks());

  EXPECT_TRUE(
      PublisherOptions{}.set_full_publisher_ignored().full_publisher_ignored());
  EXPECT_TRUE(
      PublisherOptions{}.set_full_publisher_rejects().full_publisher_rejects());
  EXPECT_TRUE(
      PublisherOptions{}.set_full_publisher_blocks().full_publisher_blocks());
}

TEST(PublisherOptions, OptionsConstructor) {
  auto const b = PublisherOptions(
      Options{}
          .set<MaximumHoldTimeOption>(std::chrono::seconds(12))
          .set<MaximumBatchMessagesOption>(10)
          .set<MaximumBatchBytesOption>(123)
          .set<MaximumPendingMessagesOption>(4)
          .set<MaximumPendingBytesOption>(444)
          .set<MessageOrderingOption>(true)
          .set<FullPublisherActionOption>(FullPublisherAction::kRejects));

  auto const expected = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::seconds(12));
  EXPECT_EQ(expected, b.maximum_hold_time());
  EXPECT_EQ(10, b.maximum_batch_message_count());
  EXPECT_EQ(123, b.maximum_batch_bytes());
  EXPECT_EQ(4, b.maximum_pending_messages());
  EXPECT_EQ(444, b.maximum_pending_bytes());
  EXPECT_TRUE(b.message_ordering());
  EXPECT_TRUE(b.full_publisher_rejects());
}

TEST(PublisherOptions, MakeOptions) {
  auto b = PublisherOptions{}
               .set_maximum_hold_time(std::chrono::seconds(12))
               .set_maximum_batch_message_count(10)
               .set_maximum_batch_bytes(123)
               .set_maximum_pending_messages(4)
               .set_maximum_pending_bytes(444)
               .enable_message_ordering();

  auto opts = pubsub_internal::MakeOptions(b);
  EXPECT_EQ(std::chrono::seconds(12), opts.get<MaximumHoldTimeOption>());
  EXPECT_EQ(10, opts.get<MaximumBatchMessagesOption>());
  EXPECT_EQ(123, opts.get<MaximumBatchBytesOption>());
  EXPECT_EQ(4, opts.get<MaximumPendingMessagesOption>());
  EXPECT_EQ(444, opts.get<MaximumPendingBytesOption>());
  EXPECT_EQ(true, opts.get<MessageOrderingOption>());

  auto ignored = PublisherOptions{}.set_full_publisher_ignored();
  opts = pubsub_internal::MakeOptions(ignored);
  EXPECT_EQ(FullPublisherAction::kIgnored,
            opts.get<FullPublisherActionOption>());

  auto rejects = PublisherOptions{}.set_full_publisher_rejects();
  opts = pubsub_internal::MakeOptions(rejects);
  EXPECT_EQ(FullPublisherAction::kRejects,
            opts.get<FullPublisherActionOption>());

  auto blocks = PublisherOptions{}.set_full_publisher_blocks();
  opts = pubsub_internal::MakeOptions(blocks);
  EXPECT_EQ(FullPublisherAction::kBlocks,
            opts.get<FullPublisherActionOption>());
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
