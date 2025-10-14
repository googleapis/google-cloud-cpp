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

#include "google/cloud/bigtable/sql_statement.h"
#include "google/cloud/internal/make_status.h"
#include <algorithm>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
PrepareQueryProto SqlStatementInternals::ToProto(bigtable::SqlStatement s) {
  ::google::bigtable::v2::PrepareQueryRequest statement_proto;
  statement_proto.set_query(std::move(s.statement_));
  statement_proto.set_instance_name(std::move(s.instance_name_));
  if (!s.params_.empty()) {
    auto& types = *statement_proto.mutable_param_types();
    for (auto& param : s.params_) {
      types[param.first] = std::move(param.second.type());
    }
  }
  return statement_proto;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal

namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::vector<std::string> SqlStatement::ParameterNames() const {
  std::vector<std::string> keys;
  keys.reserve(params_.size());
  for (auto const& p : params_) {
    keys.push_back(p.first);
  }
  return keys;
}

google::cloud::StatusOr<Value> SqlStatement::GetParameter(
    std::string const& parameter_name) const {
  auto iter = params_.find(parameter_name);
  if (iter != params_.end()) {
    // This method does not return a const value, so we copy it.
    return Value(iter->second.value());
  }
  return internal::NotFoundError("No such parameter: " + parameter_name,
                                 GCP_ERROR_INFO());
}

bool operator==(Parameter const& a, Parameter const& b) {
  return a.value() == b.value();
}

std::ostream& operator<<(std::ostream& os, Parameter const& p) {
  return os << p.value();
}

std::ostream& operator<<(std::ostream& os, SqlStatement const& stmt) {
  os << stmt.statement_;
  for (auto const& param : stmt.params_) {
    os << "\n[param]: {" << param.first << "=" << param.second << "}";
  }
  return os;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
