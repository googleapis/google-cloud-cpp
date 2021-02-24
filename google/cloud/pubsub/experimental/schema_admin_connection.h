// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_EXPERIMENTAL_SCHEMA_ADMIN_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_EXPERIMENTAL_SCHEMA_ADMIN_CONNECTION_H

#include "google/cloud/pubsub/backoff_policy.h"
#include "google/cloud/pubsub/connection_options.h"
#include "google/cloud/pubsub/internal/schema_stub.h"
#include "google/cloud/pubsub/retry_policy.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/internal/pagination_range.h"
#include "google/cloud/status_or.h"
#include <google/pubsub/v1/schema.pb.h>
#include <memory>

namespace google {
namespace cloud {
namespace pubsub_experimental {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

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
 * A connection to Cloud Pub/Sub for schema-related administrative operations.
 *
 * This interface defines pure-virtual methods for each of the user-facing
 * overload sets in `SchemaAdminClient`. That is, all of `SchemaAdminClient`'s
 * overloads will forward to the one pure-virtual method declared in this
 * interface. This allows users to inject custom behavior (e.g., with a Google
 * Mock object) in a `SchemaAdminClient` object for use in their own tests.
 *
 * To create a concrete instance that connects you to the real Cloud Pub/Sub
 * service, see `MakeSchemaAdminConnection()`.
 *
 * @par The *Params nested classes
 * Applications may define classes derived from `SchemaAdminConnection`, for
 * example, because they want to mock the class. To avoid breaking all such
 * derived classes when we change the number or type of the arguments to the
 * member functions we define lightweight structures to pass the arguments.
 */
class SchemaAdminConnection {
 public:
  virtual ~SchemaAdminConnection() = 0;

  /// Defines the interface for `SchemaAdminClient::CreateSchema()`
  virtual StatusOr<google::pubsub::v1::Schema> CreateSchema(
      google::pubsub::v1::CreateSchemaRequest const&) = 0;

  /// Defines the interface for `SchemaAdminClient::GetSchema()`
  virtual StatusOr<google::pubsub::v1::Schema> GetSchema(
      google::pubsub::v1::GetSchemaRequest const&) = 0;

  /// Defines the interface for `SchemaAdminClient::ListSchemas()`
  virtual ListSchemasRange ListSchemas(
      google::pubsub::v1::ListSchemasRequest const&) = 0;

  /// Defines the interface for `SchemaAdminClient::DeleteSchema()`
  virtual Status DeleteSchema(
      google::pubsub::v1::DeleteSchemaRequest const&) = 0;

  /// Defines the interface for `SchemaAdminClient::ValidateSchema()`
  virtual StatusOr<google::pubsub::v1::ValidateSchemaResponse> ValidateSchema(
      google::pubsub::v1::ValidateSchemaRequest const&) = 0;

  /// Defines the interface for `SchemaAdminClient::ValidateMessage()`
  virtual StatusOr<google::pubsub::v1::ValidateMessageResponse> ValidateMessage(
      google::pubsub::v1::ValidateMessageRequest const&) = 0;
};

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
 * same `ConnectionOptions` parameters. However, this behavior is not guaranteed
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
 */
std::shared_ptr<SchemaAdminConnection> MakeSchemaAdminConnection(
    pubsub::ConnectionOptions const& options = pubsub::ConnectionOptions(),
    std::unique_ptr<pubsub::RetryPolicy const> retry_policy = {},
    std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy = {});

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_experimental

namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

std::shared_ptr<pubsub_experimental::SchemaAdminConnection>
MakeSchemaAdminConnection(
    pubsub::ConnectionOptions const& options, std::shared_ptr<SchemaStub> stub,
    std::unique_ptr<pubsub::RetryPolicy const> retry_policy,
    std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy);

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_EXPERIMENTAL_SCHEMA_ADMIN_CONNECTION_H
