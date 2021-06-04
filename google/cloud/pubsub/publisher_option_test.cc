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

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
