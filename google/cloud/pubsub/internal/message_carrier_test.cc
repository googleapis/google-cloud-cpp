// Copyright 2023 Google LLC
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

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#include "google/cloud/pubsub/internal/message_carrier.h"
#include "google/cloud/pubsub/message.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::Contains;
using ::testing::IsNull;
using ::testing::Pair;

TEST(MessageCarrierTest, SetAttribute) {
  auto message = pubsub::MessageBuilder().Build();
  MessageCarrier message_carrier(message);

  message_carrier.Set("key", "test-value");

  EXPECT_THAT(message.attributes(),
              Contains(Pair("googclient_key", "test-value")));
}

TEST(MessageCarrierTest, GetAttribute) {
  auto message =
      pubsub::MessageBuilder().SetAttribute("googclient_key", "value").Build();
  MessageCarrier message_carrier(message);

  auto value = message_carrier.Get("key");

  EXPECT_EQ(std::string(value.data(), value.size()), "value");
}

TEST(MessageCarrierTest, GetAttributeIgnoresKeyWithoutPrefix) {
  auto message = pubsub::MessageBuilder().SetAttribute("key", "value").Build();
  MessageCarrier message_carrier(message);

  auto value = message_carrier.Get("key");

  EXPECT_THAT(value.data(), IsNull());
}

TEST(MessageCarrierTest, GetAttributeNotFound) {
  auto message = pubsub::MessageBuilder().Build();
  MessageCarrier message_carrier(message);

  auto value = message_carrier.Get("key1");

  EXPECT_THAT(value.data(), IsNull());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
