// Copyright 2024 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TESTING_MOCK_BATCH_CALLBACK_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TESTING_MOCK_BATCH_CALLBACK_H

#include "google/cloud/pubsub/internal/batch_callback.h"
#include "google/cloud/pubsub/internal/message_callback.h"
#include "google/cloud/pubsub/version.h"
#include <gmock/gmock.h>
#include <string>

namespace google {
namespace cloud {
namespace pubsub_testing {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A class to mock pubsub_internal::BatchCallback
 */
class MockBatchCallback : public pubsub_internal::BatchCallback {
 public:
  ~MockBatchCallback() override = default;
  MOCK_METHOD(void, callback,
              (pubsub_internal::BatchCallback::StreamingPullResponse));
  MOCK_METHOD(void, user_callback,
              (pubsub_internal::MessageCallback::MessageAndHandler));
  MOCK_METHOD(void, message_callback,
              (pubsub_internal::MessageCallback::ReceivedMessage));
  MOCK_METHOD(void, StartConcurrencyControl, (std::string const&));
  MOCK_METHOD(void, EndConcurrencyControl, (std::string const&));
  MOCK_METHOD(void, AckStart, (std::string const&));
  MOCK_METHOD(void, AckEnd, (std::string const&));
  MOCK_METHOD(void, NackStart, (std::string const&));
  MOCK_METHOD(void, NackEnd, (std::string const&));
  MOCK_METHOD(void, ModackStart, (std::string const&));
  MOCK_METHOD(void, ModackEnd, (std::string const&));
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TESTING_MOCK_BATCH_CALLBACK_H
