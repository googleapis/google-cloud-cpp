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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_MOCKS_MOCK_PUBLISHER_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_MOCKS_MOCK_PUBLISHER_CONNECTION_H

#include "google/cloud/pubsub/publisher_connection.h"
#include <gmock/gmock.h>
#include <string>

namespace google {
namespace cloud {
namespace pubsub_mocks {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A googlemock-based mock for [pubsub::PublisherConnection][mocked-link]
 *
 * [mocked-link]: @ref google::cloud::pubsub::PublisherConnection
 *
 * @see @ref publisher-mock for an example using this class.
 */
class MockPublisherConnection : public pubsub::PublisherConnection {
 public:
  MOCK_METHOD(future<StatusOr<std::string>>, Publish,
              (pubsub::PublisherConnection::PublishParams), (override));
  MOCK_METHOD(void, Flush, (pubsub::PublisherConnection::FlushParams),
              (override));
  MOCK_METHOD(void, ResumePublish,
              (pubsub::PublisherConnection::ResumePublishParams), (override));
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_mocks
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_MOCKS_MOCK_PUBLISHER_CONNECTION_H
