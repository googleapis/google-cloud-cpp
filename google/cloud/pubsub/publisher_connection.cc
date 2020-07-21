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

#include "google/cloud/pubsub/publisher_connection.h"
#include "google/cloud/pubsub/internal/batching_publisher_connection.h"
#include "google/cloud/pubsub/internal/publisher_stub.h"
#include <memory>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

PublisherConnection::~PublisherConnection() = default;

std::shared_ptr<PublisherConnection> MakePublisherConnection(
    Topic topic, PublisherOptions options,
    ConnectionOptions const& connection_options) {
  auto stub = pubsub_internal::CreateDefaultPublisherStub(connection_options,
                                                          /*channel_id=*/0);
  return pubsub_internal::MakePublisherConnection(
      std::move(topic), std::move(options), std::move(stub),
      connection_options);
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub

namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

std::shared_ptr<pubsub::PublisherConnection> MakePublisherConnection(
    pubsub::Topic topic, pubsub::PublisherOptions options,
    std::shared_ptr<PublisherStub> stub,
    pubsub::ConnectionOptions const& connection_options) {
  return BatchingPublisherConnection::Create(
      std::move(topic), options.batching_config(), std::move(stub),
      connection_options);
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
