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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_PREPARED_QUERY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_PREPARED_QUERY_H

#include "google/cloud/bigtable/bound_query.h"
#include "google/cloud/bigtable/instance_resource.h"
#include "google/cloud/bigtable/internal/query_plan.h"
#include "google/cloud/bigtable/sql_statement.h"
#include "google/cloud/bigtable/value.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/completion_queue.h"
#include <google/bigtable/v2/bigtable.pb.h>
#include <string>
#include <unordered_map>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// Represents a long-lived query execution plan.
// Query plans can expire and are refreshed as a background task.
class PreparedQuery {
 public:
  // Creates an instance of BoundQuery using the query plan ID from the
  // response.
  BoundQuery BindParameters(
      std::unordered_map<std::string, Value> params) const;

  // Accessors
  InstanceResource const& instance() const;
  SqlStatement const& sql_statement() const;

  // While we work on the Bigtable GoogleSQL functionality, we will keep this
  // constructor as public, but this will be converted to private as
  // originally intended once other classes that orchestrate PreparedQuery
  // are implemented.
  GOOGLE_CLOUD_CPP_DEPRECATED("use the other constructor")
  PreparedQuery(CompletionQueue cq, InstanceResource instance,
                SqlStatement sql_statement,
                google::bigtable::v2::PrepareQueryResponse response)
      : instance_(std::move(instance)),
        sql_statement_(std::move(sql_statement)) {
    *response.mutable_prepared_query() = sql_statement_.sql();

    // For now, the refresh function has no effect, and we simply return a new
    // prepared query response.
    query_plan_ = bigtable_internal::QueryPlan::Create(
        std::move(cq), std::move(response), [] {
          return make_ready_future(
              StatusOr<google::bigtable::v2::PrepareQueryResponse>{});
        });
  }

  PreparedQuery(InstanceResource instance, SqlStatement sql_statement,
                std::shared_ptr<bigtable_internal::QueryPlan> query_plan)
      : instance_(std::move(instance)),
        sql_statement_(std::move(sql_statement)),
        query_plan_(std::move(query_plan)) {}

 private:
  InstanceResource instance_;
  SqlStatement sql_statement_;
  std::shared_ptr<bigtable_internal::QueryPlan> query_plan_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_PREPARED_QUERY_H
