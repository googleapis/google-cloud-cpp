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

#include "google/cloud/pubsub/publisher.h"
#include "google/cloud/pubsub/internal/batching_publisher_connection.h"
#include "google/cloud/pubsub/internal/ordering_key_publisher_connection.h"
#include "google/cloud/pubsub/internal/rejects_with_ordering_key.h"

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {
std::shared_ptr<pubsub_internal::MessageBatcher> MakeMessageBatcher(
    pubsub::Topic topic, pubsub::PublisherOptions options,
    std::shared_ptr<pubsub::PublisherConnection> connection) {
  if (options.message_ordering()) {
    auto factory = [topic, options, connection](std::string const& key) {
      return pubsub_internal::BatchingPublisherConnection::Create(
          topic, options, key, connection);
    };
    return pubsub_internal::OrderingKeyPublisherConnection::Create(
        std::move(factory));
  }
  return pubsub_internal::RejectsWithOrderingKey::Create(
      pubsub_internal::BatchingPublisherConnection::Create(
          std::move(topic), std::move(options), {}, std::move(connection)));
}

}  // namespace

Publisher::Publisher(Topic topic, PublisherOptions options,
                     std::shared_ptr<PublisherConnection> connection)
    : batcher_(MakeMessageBatcher(std::move(topic), std::move(options),
                                  std::move(connection))) {}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
