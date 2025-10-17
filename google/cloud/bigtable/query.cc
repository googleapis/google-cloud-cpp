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

#include "google/cloud/bigtable/query.h"
#include "google/cloud/bigtable/sql_statement.h"

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::string const& BoundQuery::prepared_query() const {
  return query_plan_->prepared_query();
}

google::bigtable::v2::ResultSetMetadata const& BoundQuery::metadata() const {
  return query_plan_->metadata();
}

std::unordered_map<std::string, Value> const& BoundQuery::parameters() const {
  return parameters_;
}

InstanceResource const& BoundQuery::instance() const { return instance_; }

google::bigtable::v2::ExecuteQueryRequest BoundQuery::ToRequestProto() {
  google::bigtable::v2::ExecuteQueryRequest result;
  *result.mutable_prepared_query() = query_plan_->prepared_query();
  *result.mutable_instance_name() = instance_.FullName();
  *result.mutable_prepared_query() = query_plan_->prepared_query();

  google::protobuf::Map<std::string, google::bigtable::v2::Value> parameters;
  for (auto const& kv : parameters_) {
    parameters[kv.first] =
        bigtable_internal::ValueInternals::ToProto(kv.second).second;
  }
  *result.mutable_params() = parameters;
  return result;
}

BoundQuery PreparedQuery::BindParameters(
    std::unordered_map<std::string, Value> params) const {
  return BoundQuery(instance_, query_plan_, std::move(params));
}

InstanceResource const& PreparedQuery::instance() const { return instance_; }
SqlStatement const& PreparedQuery::sql_statement() const {
  return sql_statement_;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
