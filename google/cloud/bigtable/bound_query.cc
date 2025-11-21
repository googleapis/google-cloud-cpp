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
#include "google/cloud/bigtable/internal/query_plan.h"

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

StatusOr<google::bigtable::v2::PrepareQueryResponse> BoundQuery::response() {
  return query_plan_->response();
}

google::bigtable::v2::ExecuteQueryRequest BoundQuery::ToRequestProto() const {
  google::bigtable::v2::ExecuteQueryRequest result;
  *result.mutable_instance_name() = instance_.FullName();

  google::protobuf::Map<std::string, google::bigtable::v2::Value> parameters;
  for (auto const& kv : parameters_) {
    auto type_value = bigtable_internal::ValueInternals::ToProto(kv.second);
    google::bigtable::v2::Value v = std::move(type_value.second);
    *v.mutable_type() = std::move(type_value.first);
    parameters[kv.first] = std::move(v);
  }
  *result.mutable_params() = std::move(parameters);
  return result;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
