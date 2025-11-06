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

#include "google/cloud/bigtable/bound_query.h"

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

StatusOr<std::string> BoundQuery::prepared_query() const {
  return query_plan_->prepared_query();
}

StatusOr<google::bigtable::v2::PrepareQueryResponse> BoundQuery::response() {
  return query_plan_->response();
}

StatusOr<google::bigtable::v2::ResultSetMetadata> BoundQuery::metadata() const {
  return query_plan_->metadata();
}

std::unordered_map<std::string, Value> const& BoundQuery::parameters() const {
  return parameters_;
}

InstanceResource const& BoundQuery::instance() const { return instance_; }

google::bigtable::v2::ExecuteQueryRequest BoundQuery::ToRequestProto() const {
  google::bigtable::v2::ExecuteQueryRequest result;
  *result.mutable_instance_name() = instance_.FullName();
  auto prepared_query = query_plan_->prepared_query();
  if (prepared_query.ok()) {
    *result.mutable_prepared_query() = query_plan_->prepared_query().value();
  }

  google::protobuf::Map<std::string, google::bigtable::v2::Value> parameters;
  for (auto const& kv : parameters_) {
    parameters[kv.first] =
        bigtable_internal::ValueInternals::ToProto(kv.second).second;
  }
  *result.mutable_params() = parameters;
  return result;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
