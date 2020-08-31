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
  MOCK_METHOD0(Shutdown, void());
  MOCK_METHOD2(AckMessage,
               future<Status>(std::string const& ack_id, std::size_t size));
  MOCK_METHOD2(NackMessage,
               future<Status>(std::string const& ack_id, std::size_t size));
  MOCK_METHOD2(BulkNack, future<Status>(std::vector<std::string> ack_ids,
                                        std::size_t total_size));
  MOCK_METHOD1(
      Pull, future<StatusOr<google::pubsub::v1::PullResponse>>(std::int32_t));
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TESTING_MOCK_SUBSCRIPTION_BATCH_SOURCE_H
