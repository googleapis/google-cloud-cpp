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

#include "google/cloud/pubsub/subscription_options.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

TEST(SubscriptionOptionsTest, Default) {
  SubscriptionOptions const options{};
  EXPECT_LE(options.message_count_lwm(), options.message_count_hwm());
  EXPECT_LT(0, options.message_count_hwm());
  EXPECT_LE(options.message_size_lwm(), options.message_size_hwm());
  EXPECT_LT(0, options.message_size_hwm());
  EXPECT_LE(options.concurrency_lwm(), options.concurrency_hwm());
  EXPECT_LT(0, options.concurrency_hwm());
}

TEST(SubscriptionOptionsTest, SetMessageCount) {
  auto options = SubscriptionOptions{}.set_message_count_watermarks(8, 16);
  EXPECT_EQ(16, options.message_count_hwm());
  EXPECT_EQ(8, options.message_count_lwm());

  options.set_message_count_watermarks(0, 0);
  EXPECT_EQ(1, options.message_count_hwm());
  EXPECT_EQ(0, options.message_count_lwm());

  options.set_message_count_watermarks(10, 5);
  EXPECT_EQ(5, options.message_count_hwm());
  EXPECT_EQ(5, options.message_count_lwm());
}

TEST(SubscriptionOptionsTest, SetMessageSize) {
  auto options = SubscriptionOptions{}.set_message_size_watermarks(8, 16);
  EXPECT_EQ(16, options.message_size_hwm());
  EXPECT_EQ(8, options.message_size_lwm());

  options.set_message_size_watermarks(0, 0);
  EXPECT_EQ(1, options.message_size_hwm());
  EXPECT_EQ(0, options.message_size_lwm());

  options.set_message_size_watermarks(10, 5);
  EXPECT_EQ(5, options.message_size_hwm());
  EXPECT_EQ(5, options.message_size_lwm());
}

TEST(SubscriptionOptionsTest, SetConcurrency) {
  auto options = SubscriptionOptions{}.set_concurrency_watermarks(8, 16);
  EXPECT_EQ(16, options.concurrency_hwm());
  EXPECT_EQ(8, options.concurrency_lwm());

  options.set_concurrency_watermarks(0, 0);
  EXPECT_EQ(1, options.concurrency_hwm());
  EXPECT_EQ(0, options.concurrency_lwm());

  options.set_concurrency_watermarks(10, 5);
  EXPECT_EQ(5, options.concurrency_hwm());
  EXPECT_EQ(5, options.concurrency_lwm());
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
