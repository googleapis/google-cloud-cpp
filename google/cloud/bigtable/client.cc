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

#include "google/cloud/bigtable/client.h"

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

StatusOr<PreparedQuery> Client::PrepareQuery(InstanceResource instance,
                                             SqlStatement statement,
                                             Options const&) const {
  PrepareQueryParams params{std::move(instance), std::move(statement)};
  return conn_->PrepareQuery(std::move(params));
}

// Creates a PreparedQuery containing the id of the execution plan created
// by the service but returns a future.
future<StatusOr<PreparedQuery>> Client::AsyncPrepareQuery(
    InstanceResource instance, SqlStatement statement, Options const&) const {
  PrepareQueryParams params{std::move(instance), std::move(statement)};
  return conn_->AsyncPrepareQuery(std::move(params));
}

// Calls ExecuteQuery and returns a RowStream that provides a forward iterator
// for consuming query result rows. Takes the BoundQuery by rvalue reference to
// promote thread safety.
StatusOr<RowStream> Client::ExecuteQuery(BoundQuery&& bound_query,
                                         Options const&) const {
  ExecuteQueryParams params{std::move(bound_query)};
  return conn_->ExecuteQuery(params);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
