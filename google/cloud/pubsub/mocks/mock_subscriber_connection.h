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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_MOCKS_MOCK_SUBSCRIBER_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_MOCKS_MOCK_SUBSCRIBER_CONNECTION_H

#include "google/cloud/pubsub/subscriber_connection.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_mocks {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

/**
 * A googlemock-based mock for `pubsub::SubscriberConnection`
 *
 * @see @ref subscriber-mock for an example using this class.
 */
class MockSubscriberConnection : public pubsub::SubscriberConnection {
 public:
  MOCK_METHOD(future<Status>, Subscribe,
              (pubsub::SubscriberConnection::SubscribeParams), (override));
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_mocks
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_MOCKS_MOCK_SUBSCRIBER_CONNECTION_H
