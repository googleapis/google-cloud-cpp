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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_PUBLISHER_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_PUBLISHER_CONNECTION_H

#include "google/cloud/pubsub/publisher_connection.h"
#include "google/cloud/pubsublite/admin_connection.h"
#include "google/cloud/pubsublite/topic.h"
#include "google/cloud/options.h"
#include "google/cloud/status_or.h"

namespace google {
namespace cloud {
namespace pubsublite {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Creates a new `PublisherConnection` object to work with `Publisher`.
 *
 * @param admin_connection a connection to the Pub/Sub Lite Admin API. This is
 *     needed to query the number of partitions in the topic.
 * @param topic the Cloud Pub/Sub Lite topic used by the returned
 *     `PublisherConnection`.
 * @param opts The options to use for this call. Expected options are any of
 *     the types in the following option lists and in
 * google/cloud/pubsublite/options.h.
 *       - `google::cloud::CommonOptionList`
 *       - `google::cloud::GrpcOptionList`
 */
StatusOr<std::unique_ptr<google::cloud::pubsub::PublisherConnection>>
MakePublisherConnection(
    std::shared_ptr<AdminServiceConnection> admin_connection, Topic topic,
    Options opts);

/**
 * Creates a new `PublisherConnection` object to work with `Publisher`.
 *
 * @param topic the Cloud Pub/Sub Lite topic used by the returned
 *     `PublisherConnection`.
 * @param opts The options to use for this call. Expected options are any of
 *     the types in the following option lists and in
 * google/cloud/pubsublite/options.h.
 *       - `google::cloud::CommonOptionList`
 *       - `google::cloud::GrpcOptionList`
 *
 * @deprecated Use the overload consuming an `AdminServiceConnection`.
 */
// clang-format off
[[deprecated("Use the overload consuming an AdminServiceConnection")]]
StatusOr<std::unique_ptr<google::cloud::pubsub::PublisherConnection>>
// clang-format on
MakePublisherConnection(Topic topic, Options opts);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_PUBLISHER_CONNECTION_H
