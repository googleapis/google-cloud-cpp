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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TESTING_MOCK_SUBSCRIPTION_BATCH_SOURCE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TESTING_MOCK_SUBSCRIPTION_BATCH_SOURCE_H

#include "google/cloud/pubsub/internal/subscription_batch_source.h"
#include "google/cloud/pubsub/version.h"
#include <gmock/gmock.h>
#include <chrono>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace pubsub_testing {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

class MockSubscriptionBatchSource
    : public pubsub_internal::SubscriptionBatchSource {
 public:
  MOCK_METHOD(void, Start, (pubsub_internal::BatchCallback), (override));
  MOCK_METHOD(void, Shutdown, (), (override));
  MOCK_METHOD(void, AckMessage, (std::string const& ack_id), (override));
  MOCK_METHOD(void, NackMessage, (std::string const& ack_id), (override));
  MOCK_METHOD(void, BulkNack, (std::vector<std::string> ack_ids), (override));
  MOCK_METHOD(void, ExtendLeases,
              (std::vector<std::string> ack_ids,
               std::chrono::seconds extension),
              (override));
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TESTING_MOCK_SUBSCRIPTION_BATCH_SOURCE_H
