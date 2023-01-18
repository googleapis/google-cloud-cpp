// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SCHEMA_ADMIN_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SCHEMA_ADMIN_CONNECTION_H

#include "google/cloud/pubsub/backoff_policy.h"
#include "google/cloud/pubsub/connection_options.h"
#include "google/cloud/pubsub/retry_policy.h"
#include "google/cloud/pubsub/schema_connection.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/internal/non_constructible.h"
#include "google/cloud/internal/pagination_range.h"
#include <initializer_list>
#include <memory>

namespace google {
namespace cloud {
namespace pubsub {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * An input range to stream Cloud Pub/Sub schemas.
 *
 * This type models an [input range][cppref-input-range] of
 * `google::pubsub::v1::Schema` objects. Applications can make a
 * single pass through the results.
 *
 * [cppref-input-range]: https://en.cppreference.com/w/cpp/ranges/input_range
 */
using ListSchemasRange =
    google::cloud::internal::PaginationRange<google::pubsub::v1::Schema>;

/**
 * Backwards compatibility alias.
 *
 * The `SchemaAdminConnection` class has been replaced to take advantage of
 * the C++ code generator. The API is identical, just the name is different.
 */
using SchemaAdminConnection GOOGLE_CLOUD_CPP_DEPRECATED(
    "use `SchemaServiceConnection` instead") = SchemaServiceConnection;

/**
 * Creates a new `SchemaAdminConnection` object to work with
 * `SchemaAdminClient`.
 *
 * @note This function exists solely for backwards compatibility. It prevents
 *     existing code that calls `MakeSchemaAdminConnection({})` from breaking,
 *     due to ambiguity.
 *
 * @deprecated Please use `MakeSchemaAdminConnection()` instead.
 */
GOOGLE_CLOUD_CPP_DEPRECATED("use `MakeSchemaServiceConnection()` instead")
std::shared_ptr<SchemaServiceConnection> MakeSchemaAdminConnection(
    std::initializer_list<internal::NonConstructible>);

/**
 * Creates a new `SchemaAdminConnection` object to work with
 * `SchemaAdminClient`.
 *
 * The `SchemaAdminConnection` class is provided for applications wanting to
 * mock the `SchemaAdminClient` behavior in their tests. It is not intended for
 * direct use.
 *
 * @par Performance
 * Creating a new `SchemaAdminConnection` is relatively expensive. This
 * typically initiates connections to the service, and therefore these objects
 * should be shared and reused when possible. Note that gRPC reuses existing OS
 * resources (sockets) whenever possible, so applications may experience better
 * performance on the second (and subsequent) calls to this function with the
 * same `Options` from `GrpcOptionList` and `CommonOptionList`. However, this
 * behavior is not guaranteed and applications should not rely on it.
 *
 * @see `SchemaAdminClient`
 *
 * @param opts The options to use for this call. Expected options are any of
 *     the types in the following option lists.
 *       - `google::cloud::CommonOptionList`
 *       - `google::cloud::GrpcOptionList`
 *       - `google::cloud::pubsub::PolicyOptionList`
 */
GOOGLE_CLOUD_CPP_DEPRECATED("use `MakeSchemaServiceConnection()` instead")
std::shared_ptr<SchemaServiceConnection> MakeSchemaAdminConnection(
    Options opts = {});

/**
 * Creates a new `SchemaAdminConnection` object to work with
 * `SchemaAdminClient`.
 *
 * The `SchemaAdminConnection` class is provided for applications wanting to
 * mock the `SchemaAdminClient` behavior in their tests. It is not intended for
 * direct use.
 *
 * @par Performance
 * Creating a new `SchemaAdminConnection` is relatively expensive. This
 * typically initiate connections to the service, and therefore these objects
 * should be shared and reused when possible. Note that gRPC reuses existing OS
 * resources (sockets) whenever possible, so applications may experience better
 * performance on the second (and subsequent) calls to this function with the
 * identical values for @p options. However, this behavior is not guaranteed
 * and applications should not rely on it.
 *
 * @see `SchemaAdminClient`
 *
 * @param options (optional) configure the `PublisherConnection` created by
 *     this function.
 * @param retry_policy control for how long (or how many times) are retryable
 *     RPCs attempted.
 * @param backoff_policy controls the backoff behavior between retry attempts,
 *     typically some form of exponential backoff with jitter.
 *
 * @deprecated Please use the `MakeSchemaAdminConnection` function that accepts
 *     `google::cloud::Options` instead.
 */
GOOGLE_CLOUD_CPP_DEPRECATED("use `MakeSchemaServiceConnection()` instead")
std::shared_ptr<SchemaServiceConnection> MakeSchemaAdminConnection(
    pubsub::ConnectionOptions const& options,
    std::unique_ptr<pubsub::RetryPolicy const> retry_policy = {},
    std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy = {});

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SCHEMA_ADMIN_CONNECTION_H
