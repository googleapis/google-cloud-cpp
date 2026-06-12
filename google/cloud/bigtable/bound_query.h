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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_BOUND_QUERY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_BOUND_QUERY_H

#include "google/cloud/bigtable/instance_resource.h"
#include "google/cloud/bigtable/value.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/completion_queue.h"
#include "google/bigtable/v2/bigtable.pb.h"
#include <string>
#include <unordered_map>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
class DataConnectionImpl;
class QueryPlan;
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal

namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Move only type representing a PreparedQuery with parameter values.
 * Created by calling PreparedQuery::BindParameters.
 */
class BoundQuery {
 public:
  // Copy and move.
  BoundQuery(BoundQuery const&) = delete;
  BoundQuery(BoundQuery&&) = default;
  BoundQuery& operator=(BoundQuery const&) = delete;
  BoundQuery& operator=(BoundQuery&&) = default;

  // Accessors
  InstanceResource const& instance() const { return instance_; }
  std::unordered_map<std::string, Value> const& parameters() const {
    return parameters_;
  }

  // This data may change if a Query Plan Refresh is performed. If the original
  // response data is needed for your application, consider copying the response
  // data immediately after a successful Client::PrepareQuery.
  StatusOr<google::bigtable::v2::PrepareQueryResponse> response();

  google::bigtable::v2::ExecuteQueryRequest ToRequestProto() const;

 private:
  friend class PreparedQuery;
  friend class bigtable_internal::DataConnectionImpl;

  BoundQuery(InstanceResource instance,
             std::shared_ptr<bigtable_internal::QueryPlan> query_plan,
             std::unordered_map<std::string, Value> parameters)
      : instance_(std::move(instance)),
        query_plan_(std::move(query_plan)),
        parameters_(std::move(parameters)) {}

  InstanceResource instance_;
  // Copy of the query_plan_ contained by the PreparedQuery that created
  // this BoundQuery.
  std::shared_ptr<bigtable_internal::QueryPlan> query_plan_;
  std::unordered_map<std::string, Value> parameters_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_BOUND_QUERY_H
