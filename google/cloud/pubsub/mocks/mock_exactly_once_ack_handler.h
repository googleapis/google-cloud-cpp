// Copyright 2022 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_MOCKS_MOCK_EXACTLY_ONCE_ACK_HANDLER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_MOCKS_MOCK_EXACTLY_ONCE_ACK_HANDLER_H

#include "google/cloud/pubsub/exactly_once_ack_handler.h"
#include <gmock/gmock.h>
#include <string>

namespace google {
namespace cloud {
namespace pubsub_mocks {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A googlemock-based mock for
 * [pubsub::ExactlyOnceAckHandler::Impl][mocked-link]
 *
 * [mocked-link]: @ref
 * google::cloud::pubsub_internal::ExactlyOnceAckHandler::Impl
 *
 * @see @ref subscriber-mock for an example using this class.
 */
class MockExactlyOnceAckHandler : public pubsub::ExactlyOnceAckHandler::Impl {
 public:
  MOCK_METHOD(future<Status>, ack, (), (override));
  MOCK_METHOD(future<Status>, nack, (), (override));
  MOCK_METHOD(std::string, ack_id, ());
  MOCK_METHOD(std::int32_t, delivery_attempt, (), (const, override));
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_mocks
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_MOCKS_MOCK_EXACTLY_ONCE_ACK_HANDLER_H
