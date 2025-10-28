// Copyright 2025 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_CLIENT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_CLIENT_H

#include "google/cloud/bigtable/data_connection.h"

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Connects to Cloud Bigtable's query preparation and execution APIs.
 *
 * A Bigtable query's lifecycle consists of two phases:
 * 1. Preparing a query: The service creates and caches a query execution plan.
 * 2. Executing a query: The client sends the plan ID and concrete parameters
 *    to the service, which then executes the query.
 *
 * This class provides methods for both preparing and executing SQL queries.
 *
 * @par Cost
 * Creating a `Client` object is a relatively low-cost operation. It does not
 * require connecting to the Bigtable servers. However, each `Client` object
 * holds a `std::shared_ptr<DataConnection>`, and the first RPC made on this
 * connection may incur a higher latency as the connection is established.
 * For this reason, it is recommended to reuse `Client` objects when possible.
 *
 * @par Example
 * @code
 * #include "google/cloud/bigtable/client.h"
 * #include "google/cloud/project.h"
 * #include <iostream>
 *
 * int main() {
 *   namespace cbt = google::cloud::bigtable;
 *   cbt::Client client(cbt::MakeDataConnection());
 *   cbt::InstanceResource instance(google::cloud::Project("my-project"),
 * "my-instance");
 *
 *   // Declare a parameter with a type, but no value.
 *   cbt::SqlStatement statement(
 *       "SELECT _key, CAST(family['qual'] AS STRING) AS value "
 *       "FROM my-table WHERE _key = @key",
 *       {{"key", cbt::Value(cbt::Bytes())}});
 *
 *   google::cloud::StatusOr<cbt::PreparedQuery> prepared_query =
 *       client.PrepareQuery(instance, statement);
 *   if (!prepared_query) throw std::move(prepared_query).status();
 *
 *   auto bound_query = prepared_query->BindParameters(
 *       {{"key", cbt::Value("row-key-2")}});
 *
 *   RowStream results =
 *       client.ExecuteQuery(std::move(bound_query));
 *
 *   ... // process rows
 * }
 * @endcode
 */
class Client {
 public:
  /**
   * Creates a new Client object.
   *
   * @param conn The connection object to use for all RPCs. This is typically
   *     created by `MakeDataConnection()`.
   * @param opts Unused for now
   */
  explicit Client(std::shared_ptr<DataConnection> conn, Options opts = {})
      : conn_(std::move(conn)), opts_(std::move(opts)) {}

  /**
   * Prepares a query for future execution.
   *
   * This sends the SQL statement to the service, which validates it and
   * creates an execution plan, returning a handle to this plan.
   *
   * @param instance The instance to prepare the query against.
   * @param statement The SQL statement to prepare.
   * @param opts Unused for now
   * @return A `StatusOr` containing the prepared query on success. On
   *     failure, the `Status` contains error details.
   */
  StatusOr<PreparedQuery> PrepareQuery(InstanceResource const& instance,
                                       SqlStatement const& statement,
                                       Options opts = {});

  /**
   * Asynchronously prepares a query for future execution.
   *
   * This sends the SQL statement to the service, which validates it and
   * creates an execution plan, returning a handle to this plan.
   *
   * @param instance The instance to prepare the query against.
   * @param statement The SQL statement to prepare.
   * @param opts Unused for now
   * @return A `future` that will be satisfied with a `StatusOr` containing
   *     the prepared query on success. On failure, the `Status` will contain
   *     error details.
   */
  future<StatusOr<PreparedQuery>> AsyncPrepareQuery(
      InstanceResource const& instance, SqlStatement const& statement,
      Options opts = {});

  /**
   * Executes a bound query with concrete parameters.
   *
   * This returns a `RowStream`, which is a range of `StatusOr<QueryRow>`.
   * The `BoundQuery` is passed by rvalue-reference to promote thread safety,
   * as it is not safe to use a `BoundQuery` concurrently.
   *
   * @param bound_query The bound query to execute.
   * @param opts Overrides the client-level options for this call.
   * @return A `RowStream` that can be used to iterate over the result rows.
   */
  RowStream ExecuteQuery(BoundQuery&& bound_query, Options const& opts = {});

 private:
  std::shared_ptr<DataConnection> conn_;
  Options opts_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_CLIENT_H
