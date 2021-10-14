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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_MOCKS_MOCK_ACK_HANDLER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_MOCKS_MOCK_ACK_HANDLER_H

#include "google/cloud/pubsub/ack_handler.h"
#include <gmock/gmock.h>
#include <string>

namespace google {
namespace cloud {
/// A namespace for googlemock-based Cloud Pub/Sub C++ client mocks.
namespace pubsub_mocks {
/**
 * The inlined, versioned namespace for the Cloud Pubsub C++ client APIs.
 *
 * Applications may need to link multiple versions of the Cloud pubsub C++
 * client, for example, if they link a library that uses an older version of
 * the client than they do.  This namespace is inlined, so applications can use
 * `pubsub::Foo` in their source, but the symbols are versioned, i.e., the
 * symbol becomes `pubsub::v1::Foo`.
 */
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A googlemock-based mock for [pubsub::AckHandler::Impl][mocked-link]
 *
 * [mocked-link]: @ref google::cloud::pubsub::AckHandler::Impl
 *
 * @see @ref subscriber-mock for an example using this class.
 */
class MockAckHandler : public pubsub::AckHandler::Impl {
 public:
  MOCK_METHOD(void, ack, (), (override));
  MOCK_METHOD(void, nack, (), (override));
  MOCK_METHOD(std::string, ack_id, ());
  MOCK_METHOD(std::int32_t, delivery_attempt, (), (const, override));
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_mocks
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_MOCKS_MOCK_ACK_HANDLER_H
