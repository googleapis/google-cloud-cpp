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

TEST(PublisherOptions, Batching) {
  auto const b0 = BatchingConfig{};
  EXPECT_EQ(0, b0.maximum_hold_time().count());

  auto const b = BatchingConfig{}
                     .set_maximum_hold_time(std::chrono::seconds(12))
                     .set_maximum_batch_bytes(123)
                     .set_maximum_message_count(10);
  EXPECT_EQ(10, b.maximum_message_count());
  EXPECT_EQ(123, b.maximum_batch_bytes());
  auto const expected = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::seconds(12));
  EXPECT_EQ(expected, b.maximum_hold_time());
}

TEST(PublisherOptions, PublisherOptions) {
  auto const o0 = PublisherOptions{};
  EXPECT_FALSE(o0.message_ordering());

  auto const o =
      PublisherOptions{}.enable_message_ordering().set_batching_config(
          BatchingConfig{}.set_maximum_hold_time(std::chrono::seconds(12)));
  EXPECT_TRUE(o.message_ordering());
  auto const expected = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::seconds(12));
  EXPECT_EQ(expected, o.batching_config().maximum_hold_time());
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
