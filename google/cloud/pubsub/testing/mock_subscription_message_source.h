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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TESTING_MOCK_SUBSCRIPTION_MESSAGE_SOURCE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TESTING_MOCK_SUBSCRIPTION_MESSAGE_SOURCE_H

#include "google/cloud/pubsub/internal/subscription_message_source.h"
#include "google/cloud/pubsub/version.h"
#include <gmock/gmock.h>
#include <string>

namespace google {
namespace cloud {
namespace pubsub_testing {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

class MockSubscriptionMessageSource
    : public pubsub_internal::SubscriptionMessageSource {
 public:
  MOCK_METHOD(void, Start, (pubsub_internal::MessageCallback), (override));
  MOCK_METHOD(void, Shutdown, (), (override));
  MOCK_METHOD(void, Read, (std::size_t max_callbacks), (override));
  MOCK_METHOD(void, AckMessage, (std::string const& ack_id), (override));
  MOCK_METHOD(void, NackMessage, (std::string const& ack_id), (override));
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TESTING_MOCK_SUBSCRIPTION_MESSAGE_SOURCE_H
