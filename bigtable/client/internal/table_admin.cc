// Copyright 2018 Google Inc.
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

#include "bigtable/client/table_admin.h"
#include <sstream>

namespace btproto = ::google::bigtable::admin::v2;

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace noex {

::google::bigtable::admin::v2::Table TableAdmin::CreateTable(
    std::string table_id, TableConfig config, grpc::Status& status) {
  auto request = config.as_proto_move();
  request.set_parent(instance_name());
  request.set_table_id(std::move(table_id));

  auto error_message = "CreateTable(" + request.table_id() + ")";

  // This API is not idempotent, lets call it without retry
  return RpcUtils::CallWithoutRetry(
      *client_, rpc_retry_policy_->clone(), metadata_update_policy_,
      &StubType::CreateTable, request, error_message.c_str(), status);
}

std::vector<::google::bigtable::admin::v2::Table> TableAdmin::ListTables(
    ::google::bigtable::admin::v2::Table::View view, grpc::Status& status) {
  // Copy the policies in effect for the operation.
  auto rpc_policy = rpc_retry_policy_->clone();
  auto backoff_policy = rpc_backoff_policy_->clone();

  std::string msg = "TableAdmin(" + instance_name() + ")::ListTables()";

  // Build the RPC request, try to minimize copying.
  std::vector<btproto::Table> result;
  std::string page_token;
  do {
    btproto::ListTablesRequest request;
    request.set_page_token(std::move(page_token));
    request.set_parent(instance_name());
    request.set_view(view);

    auto response = RpcUtils::CallWithRetryBorrow(
        *client_, *rpc_policy, *backoff_policy, metadata_update_policy_,
        &StubType::ListTables, request, msg.c_str(), status);
    if (not status.ok()) {
      return result;
    }

    for (auto& x : *response.mutable_tables()) {
      result.emplace_back(std::move(x));
    }
    page_token = std::move(*response.mutable_next_page_token());
  } while (not page_token.empty());
  return result;
}

::google::bigtable::admin::v2::Table TableAdmin::GetTable(
    std::string table_id, grpc::Status& status,
    ::google::bigtable::admin::v2::Table::View view) {
  btproto::GetTableRequest request;
  request.set_name(TableName(table_id));
  request.set_view(view);

  MetadataUpdatePolicy metadata_update_policy(
      instance_name(), MetadataParamTypes::NAME, table_id);
  auto error_message = "GetTable(" + request.name() + ")";
  return RpcUtils::CallWithRetry(*client_, rpc_retry_policy_->clone(),
                                 rpc_backoff_policy_->clone(),
                                 metadata_update_policy, &StubType::GetTable,
                                 request, error_message.c_str(), status);
}

void TableAdmin::DeleteTable(std::string table_id, grpc::Status& status) {
  btproto::DeleteTableRequest request;
  request.set_name(TableName(table_id));
  MetadataUpdatePolicy metadata_update_policy(
      instance_name(), MetadataParamTypes::NAME, table_id);

  // This API is not idempotent, lets call it without retry
  auto error_message = "DeleteTable(" + request.name() + ")";
  RpcUtils::CallWithoutRetry(*client_, rpc_retry_policy_->clone(),
                             metadata_update_policy, &StubType::DeleteTable,
                             request, error_message.c_str(), status);
}

::google::bigtable::admin::v2::Table TableAdmin::ModifyColumnFamilies(
    std::string table_id, std::vector<ColumnFamilyModification> modifications,
    grpc::Status& status) {
  btproto::ModifyColumnFamiliesRequest request;
  request.set_name(TableName(table_id));
  for (auto& m : modifications) {
    *request.add_modifications() = m.as_proto_move();
  }
  MetadataUpdatePolicy metadata_update_policy(
      instance_name(), MetadataParamTypes::NAME, table_id);
  auto error_message = "ModifyColumnFamilies(" + request.name() + ")";
  return RpcUtils::CallWithoutRetry(
      *client_, rpc_retry_policy_->clone(), metadata_update_policy,
      &StubType::ModifyColumnFamilies, request, error_message.c_str(), status);
}

void TableAdmin::DropRowsByPrefix(std::string table_id,
                                  std::string row_key_prefix,
                                  grpc::Status& status) {
  btproto::DropRowRangeRequest request;
  request.set_name(TableName(table_id));
  request.set_row_key_prefix(std::move(row_key_prefix));
  MetadataUpdatePolicy metadata_update_policy(
      instance_name(), MetadataParamTypes::NAME, table_id);
  auto error_message = "DropRowsByPrefix(" + request.name() + ")";
  RpcUtils::CallWithoutRetry(*client_, rpc_retry_policy_->clone(),
                             metadata_update_policy, &StubType::DropRowRange,
                             request, error_message.c_str(), status);
}

void TableAdmin::DropAllRows(std::string table_id, grpc::Status& status) {
  btproto::DropRowRangeRequest request;
  request.set_name(TableName(table_id));
  request.set_delete_all_data_from_table(true);
  MetadataUpdatePolicy metadata_update_policy(
      instance_name(), MetadataParamTypes::NAME, table_id);
  auto error_message = "DropAllRows(" + request.name() + ")";
  RpcUtils::CallWithoutRetry(*client_, rpc_retry_policy_->clone(),
                             metadata_update_policy, &StubType::DropRowRange,
                             request, error_message.c_str(), status);
}

std::string TableAdmin::InstanceName() const {
  return "projects/" + client_->project() + "/instances/" + instance_id_;
}
}  // namespace noex
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
