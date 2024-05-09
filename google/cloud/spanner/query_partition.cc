// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/query_partition.h"
#include "google/cloud/internal/make_status.h"
#include <google/spanner/v1/spanner.pb.h>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// Local extension to google::spanner::v1::ExecuteSqlRequest, reserved using
// Google's conventions.
constexpr int kRouteToLeaderFieldNumber = 511037314;

QueryPartition::QueryPartition(std::string transaction_id, bool route_to_leader,
                               std::string transaction_tag,
                               std::string session_id,
                               std::string partition_token, bool data_boost,
                               SqlStatement sql_statement)
    : transaction_id_(std::move(transaction_id)),
      route_to_leader_(route_to_leader),
      transaction_tag_(std::move(transaction_tag)),
      session_id_(std::move(session_id)),
      partition_token_(std::move(partition_token)),
      data_boost_(data_boost),
      sql_statement_(std::move(sql_statement)) {}

bool operator==(QueryPartition const& a, QueryPartition const& b) {
  return a.transaction_id_ == b.transaction_id_ &&
         a.route_to_leader_ == b.route_to_leader_ &&
         a.transaction_tag_ == b.transaction_tag_ &&
         a.session_id_ == b.session_id_ &&
         a.partition_token_ == b.partition_token_ &&
         a.data_boost_ == b.data_boost_ && a.sql_statement_ == b.sql_statement_;
}

StatusOr<std::string> SerializeQueryPartition(
    QueryPartition const& query_partition) {
  google::spanner::v1::ExecuteSqlRequest proto;

  proto.set_partition_token(query_partition.partition_token());
  proto.set_session(query_partition.session_id());
  proto.mutable_transaction()->set_id(query_partition.transaction_id());
  proto.set_sql(query_partition.sql_statement_.sql());
  for (auto const& param : query_partition.sql_statement_.params()) {
    auto const& param_name = param.first;
    auto const& type_value = spanner_internal::ToProto(param.second);
    (*proto.mutable_params()->mutable_fields())[param_name] = type_value.second;
    (*proto.mutable_param_types())[param_name] = type_value.first;
  }
  if (query_partition.data_boost()) {
    proto.set_data_boost_enabled(true);
  }

  // QueryOptions are not serialized, but are instead applied on the remote
  // side during the Client::ExecuteQuery(QueryPartition, QueryOptions) call.
  // However, we do encode any transaction tag in proto.request_options.
  proto.mutable_request_options()->set_transaction_tag(
      query_partition.transaction_tag());

  // Add route_to_leader to an extension field so that we might retrieve it
  // in DeserializeQueryPartition().
  if (query_partition.route_to_leader()) {
    google::spanner::v1::ExecuteSqlRequest::GetReflection()
        ->MutableUnknownFields(&proto)
        ->AddVarint(kRouteToLeaderFieldNumber, 1);
  }

  std::string serialized_proto;
  if (proto.SerializeToString(&serialized_proto)) {
    return serialized_proto;
  }
  return internal::InvalidArgumentError("Failed to serialize QueryPartition",
                                        GCP_ERROR_INFO());
}

StatusOr<QueryPartition> DeserializeQueryPartition(
    std::string const& serialized_query_partition) {
  google::spanner::v1::ExecuteSqlRequest proto;
  if (!proto.ParseFromString(serialized_query_partition)) {
    return internal::InvalidArgumentError(
        "Failed to deserialize into QueryPartition", GCP_ERROR_INFO());
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
            spanner_internal::FromProto(param_type, std::move(param.second))));
      }
    }
  }

  bool route_to_leader = false;
  auto const& unknown_fields =
      google::spanner::v1::ExecuteSqlRequest::GetReflection()->GetUnknownFields(
          proto);
  for (int index = 0; index != unknown_fields.field_count(); ++index) {
    auto const& field = unknown_fields.field(index);
    if (field.number() == kRouteToLeaderFieldNumber) {
      route_to_leader = field.varint() != 0;
    }
  }

  QueryPartition query_partition(proto.transaction().id(), route_to_leader,
                                 proto.request_options().transaction_tag(),
                                 proto.session(), proto.partition_token(),
                                 proto.data_boost_enabled(),
                                 SqlStatement(proto.sql(), sql_parameters));
  return query_partition;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
