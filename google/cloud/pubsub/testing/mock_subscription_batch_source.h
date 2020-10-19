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

namespace google {
namespace cloud {
namespace pubsub_testing {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

class MockSubscriptionBatchSource
    : public pubsub_internal::SubscriptionBatchSource {
 public:
  MOCK_METHOD1(Start, void(pubsub_internal::BatchCallback));
  MOCK_METHOD0(Shutdown, void());
  MOCK_METHOD1(AckMessage, void(std::string const& ack_id));
  MOCK_METHOD1(NackMessage, void(std::string const& ack_id));
  MOCK_METHOD1(BulkNack, void(std::vector<std::string> ack_ids));
  MOCK_METHOD2(ExtendLeases, void(std::vector<std::string> ack_ids,
                                  std::chrono::seconds extension));
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TESTING_MOCK_SUBSCRIPTION_BATCH_SOURCE_H
