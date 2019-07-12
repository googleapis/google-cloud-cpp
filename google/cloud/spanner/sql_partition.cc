// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/sql_partition.h"
#include <google/protobuf/map.h>
#include <google/spanner/v1/spanner.pb.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

SqlPartition::SqlPartition(std::string transaction_id, std::string session_id,
                           std::string partition_token,
                           SqlStatement sql_statement)
    : transaction_id_(std::move(transaction_id)),
      session_id_(std::move(session_id)),
      partition_token_(std::move(partition_token)),
      sql_statement_(std::move(sql_statement)) {}

StatusOr<std::string> SerializeSqlPartition(SqlPartition const& sql_partition) {
  google::spanner::v1::ExecuteSqlRequest proto;
  proto.set_partition_token(sql_partition.partition_token());
  proto.set_session(sql_partition.session_id());
  proto.mutable_transaction()->set_id(sql_partition.transaction_id());
  proto.set_sql(sql_partition.sql_statement_.sql());

  for (auto const& param : sql_partition.sql_statement_.params()) {
    auto const& param_name = param.first;
    auto const& type_value = internal::ToProto(param.second);
    (*proto.mutable_params()->mutable_fields())[param_name] = type_value.second;
    (*proto.mutable_param_types())[param_name] = type_value.first;
  }
  std::string serialized_proto;
  if (proto.SerializeToString(&serialized_proto)) {
    return serialized_proto;
  }
  return Status(StatusCode::kInvalidArgument,
                "Failed to serialize SqlPartition");
}

StatusOr<SqlPartition> DeserializeSqlPartition(
    std::string const& serialized_sql_partition) {
  google::spanner::v1::ExecuteSqlRequest proto;
  if (!proto.ParseFromString(serialized_sql_partition)) {
    return Status(StatusCode::kInvalidArgument,
                  "Failed to deserialize into SqlPartition");
  }

  SqlStatement::ParamType sql_parameters;
  if (proto.has_params()) {
    auto const& param_types = proto.param_types();
    for (auto& param : *(proto.mutable_params()->mutable_fields())) {
      auto const& param_name = param.first;
      auto iter = param_types.find(param_name);
      if (iter != param_types.end()) {
        auto const& param_type = iter->second;
        sql_parameters.insert(std::make_pair(
            param_name,
            internal::FromProto(param_type, std::move(param.second))));
      }
    }
  }

  SqlPartition sql_partition(proto.transaction().id(), proto.session(),
                             proto.partition_token(),
                             SqlStatement(proto.sql(), sql_parameters));
  return sql_partition;
}

namespace internal {
SqlPartition MakeSqlPartition(std::string const& transaction_id,
                              std::string const& session_id,
                              std::string const& partition_token,
                              SqlStatement const& sql_statement) {
  return SqlPartition(transaction_id, session_id, partition_token,
                      sql_statement);
}

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
