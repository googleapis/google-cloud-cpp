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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TESTING_MOCK_MESSAGE_BATCH_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TESTING_MOCK_MESSAGE_BATCH_H

#include "google/cloud/pubsub/internal/message_batch.h"
#include "google/cloud/pubsub/message.h"
#include "google/cloud/version.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_testing {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A class to mock pubsub_internal::MessageBatch
 */
class MockMessageBatch : public pubsub_internal::MessageBatch {
 public:
  ~MockMessageBatch() override = default;

  MOCK_METHOD(void, SaveMessage, (pubsub::Message), (override));
  MOCK_METHOD(void, Flush, (), (override));
  MOCK_METHOD(void, FlushCallback, (), (override));
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TESTING_MOCK_MESSAGE_BATCH_H
