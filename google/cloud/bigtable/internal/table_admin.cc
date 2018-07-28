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

#include "google/cloud/bigtable/table_admin.h"
#include "google/cloud/bigtable/internal/unary_client_utils.h"
#include <sstream>

namespace btadmin = ::google::bigtable::admin::v2;

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace noex {

static_assert(std::is_copy_constructible<bigtable::noex::TableAdmin>::value,
              "bigtable::noex::TableAdmin must be constructible");
static_assert(std::is_copy_assignable<bigtable::noex::TableAdmin>::value,
              "bigtable::noex::TableAdmin must be assignable");

/// Shortcuts to avoid typing long names over and over.
using ClientUtils = bigtable::internal::noex::UnaryClientUtils<AdminClient>;

btadmin::Table TableAdmin::CreateTable(std::string table_id, TableConfig config,
                                       grpc::Status& status) {
  auto request = config.as_proto_move();
  request.set_parent(instance_name());
  request.set_table_id(std::move(table_id));

  // This API is not idempotent, lets call it without retry
  return ClientUtils::MakeNonIdemponentCall(
      *client_, rpc_retry_policy_->clone(), metadata_update_policy_,
      &AdminClient::CreateTable, request, "CreateTable", status);
}

std::vector<btadmin::Table> TableAdmin::ListTables(btadmin::Table::View view,
                                                   grpc::Status& status) {
  // Copy the policies in effect for the operation.
  auto rpc_policy = rpc_retry_policy_->clone();
  auto backoff_policy = rpc_backoff_policy_->clone();

  // Build the RPC request, try to minimize copying.
  std::vector<btadmin::Table> result;
  std::string page_token;
  do {
    btadmin::ListTablesRequest request;
    request.set_page_token(std::move(page_token));
    request.set_parent(instance_name());
    request.set_view(view);

    auto response = ClientUtils::MakeCall(
        *client_, *rpc_policy, *backoff_policy, metadata_update_policy_,
        &AdminClient::ListTables, request, "TableAdmin", status, true);
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

btadmin::Table TableAdmin::GetTable(std::string const& table_id,
                                    grpc::Status& status,
                                    btadmin::Table::View view) {
  btadmin::GetTableRequest request;
  request.set_name(TableName(table_id));
  request.set_view(view);

  MetadataUpdatePolicy metadata_update_policy(
      instance_name(), MetadataParamTypes::NAME, table_id);
  return ClientUtils::MakeCall(*client_, rpc_retry_policy_->clone(),
                               rpc_backoff_policy_->clone(),
                               metadata_update_policy, &AdminClient::GetTable,
                               request, "GetTable", status, true);
}

void TableAdmin::DeleteTable(std::string const& table_id,
                             grpc::Status& status) {
  btadmin::DeleteTableRequest request;
  request.set_name(TableName(table_id));
  MetadataUpdatePolicy metadata_update_policy(
      instance_name(), MetadataParamTypes::NAME, table_id);

  // This API is not idempotent, lets call it without retry
  ClientUtils::MakeNonIdemponentCall(
      *client_, rpc_retry_policy_->clone(), metadata_update_policy,
      &AdminClient::DeleteTable, request, "DeleteTable", status);
}

btadmin::Table TableAdmin::ModifyColumnFamilies(
    std::string const& table_id,
    std::vector<ColumnFamilyModification> modifications, grpc::Status& status) {
  btadmin::ModifyColumnFamiliesRequest request;
  request.set_name(TableName(table_id));
  for (auto& m : modifications) {
    *request.add_modifications() = m.as_proto_move();
  }
  MetadataUpdatePolicy metadata_update_policy(
      instance_name(), MetadataParamTypes::NAME, table_id);
  return ClientUtils::MakeNonIdemponentCall(
      *client_, rpc_retry_policy_->clone(), metadata_update_policy,
      &AdminClient::ModifyColumnFamilies, request, "ModifyColumnFamilies",
      status);
}

void TableAdmin::DropRowsByPrefix(std::string const& table_id,
                                  std::string row_key_prefix,
                                  grpc::Status& status) {
  btadmin::DropRowRangeRequest request;
  request.set_name(TableName(table_id));
  request.set_row_key_prefix(std::move(row_key_prefix));
  MetadataUpdatePolicy metadata_update_policy(
      instance_name(), MetadataParamTypes::NAME, table_id);
  ClientUtils::MakeNonIdemponentCall(
      *client_, rpc_retry_policy_->clone(), metadata_update_policy,
      &AdminClient::DropRowRange, request, "DropRowByPrefix", status);
}

void TableAdmin::DropAllRows(std::string const& table_id,
                             grpc::Status& status) {
  btadmin::DropRowRangeRequest request;
  request.set_name(TableName(table_id));
  request.set_delete_all_data_from_table(true);
  MetadataUpdatePolicy metadata_update_policy(
      instance_name(), MetadataParamTypes::NAME, table_id);
  ClientUtils::MakeNonIdemponentCall(
      *client_, rpc_retry_policy_->clone(), metadata_update_policy,
      &AdminClient::DropRowRange, request, "DropAllRows", status);
}

std::string TableAdmin::InstanceName() const {
  return "projects/" + client_->project() + "/instances/" + instance_id_;
}

btadmin::Snapshot TableAdmin::GetSnapshot(
    bigtable::ClusterId const& cluster_id,
    bigtable::SnapshotId const& snapshot_id, grpc::Status& status) {
  btadmin::GetSnapshotRequest request;
  request.set_name(SnapshotName(cluster_id, snapshot_id));

  MetadataUpdatePolicy metadata_update_policy(
      instance_name(), MetadataParamTypes::NAME, cluster_id, snapshot_id);
  return ClientUtils::MakeCall(
      *client_, rpc_retry_policy_->clone(), rpc_backoff_policy_->clone(),
      metadata_update_policy, &AdminClient::GetSnapshot, request, "GetSnapshot",
      status, true);
}

std::string TableAdmin::GenerateConsistencyToken(std::string const& table_id,
                                                 grpc::Status& status) {
  btadmin::GenerateConsistencyTokenRequest request;
  request.set_name(TableName(table_id));
  MetadataUpdatePolicy metadata_update_policy(
      instance_name(), MetadataParamTypes::NAME, table_id);

  auto response = ClientUtils::MakeCall(
      *client_, rpc_retry_policy_->clone(), rpc_backoff_policy_->clone(),
      metadata_update_policy, &AdminClient::GenerateConsistencyToken, request,
      "GenerateConsistencyToken", status, true);

  return *response.mutable_consistency_token();
}

bool TableAdmin::CheckConsistency(
    bigtable::TableId const& table_id,
    bigtable::ConsistencyToken const& consistency_token, grpc::Status& status) {
  btadmin::CheckConsistencyRequest request;
  request.set_name(TableName(table_id.get()));
  request.set_consistency_token(consistency_token.get());
  MetadataUpdatePolicy metadata_update_policy(
      instance_name(), MetadataParamTypes::NAME, table_id.get());

  auto response = ClientUtils::MakeCall(
      *client_, rpc_retry_policy_->clone(), rpc_backoff_policy_->clone(),
      metadata_update_policy, &AdminClient::CheckConsistency, request,
      "CheckConsistency", status, true);
  return response.consistent();
}

bool TableAdmin::WaitForConsistencyCheckHelper(
    bigtable::TableId const& table_id,
    bigtable::ConsistencyToken const& consistency_token, grpc::Status& status) {
  btadmin::CheckConsistencyRequest request;
  request.set_name(TableName(table_id.get()));
  request.set_consistency_token(consistency_token.get());
  MetadataUpdatePolicy metadata_update_policy(
      instance_name(), MetadataParamTypes::NAME, table_id.get());

  auto polling_policy = polling_policy_->clone();
  do {
    auto response = ClientUtils::MakeCall(
        *client_, rpc_retry_policy_->clone(), rpc_backoff_policy_->clone(),
        metadata_update_policy, &AdminClient::CheckConsistency, request,
        "CheckConsistency", status, true);

    if (status.ok()) {
      if (response.consistent()) {
        return true;
      }
    } else if (polling_policy->IsPermanentError(status)) {
      return false;
    }
  } while (not polling_policy->Exhausted());

  return false;
}

void TableAdmin::DeleteSnapshot(bigtable::ClusterId const& cluster_id,
                                bigtable::SnapshotId const& snapshot_id,
                                grpc::Status& status) {
  btadmin::DeleteSnapshotRequest request;
  request.set_name(SnapshotName(cluster_id, snapshot_id));
  MetadataUpdatePolicy metadata_update_policy(
      instance_name(), MetadataParamTypes::NAME, cluster_id, snapshot_id);

  // This API is not idempotent, lets call it without retry
  ClientUtils::MakeNonIdemponentCall(
      *client_, rpc_retry_policy_->clone(), metadata_update_policy,
      &AdminClient::DeleteSnapshot, request, "DeleteSnapshot", status);
}

void TableAdmin::ListSnapshotsImpl(
    bigtable::ClusterId const& cluster_id,
    std::function<void(google::bigtable::admin::v2::Snapshot)> const& inserter,
    grpc::Status& status) {
  // Copy the policies in effect for the operation.
  auto rpc_policy = rpc_retry_policy_->clone();
  auto backoff_policy = rpc_backoff_policy_->clone();

  MetadataUpdatePolicy metadata_update_policy(
      instance_name(), MetadataParamTypes::PARENT, cluster_id);
  std::string page_token;
  do {
    btadmin::ListSnapshotsRequest request;
    request.set_parent(ClusterName(cluster_id));
    request.set_page_size(0);
    request.set_page_token(page_token);

    auto response = ClientUtils::MakeCall(
        *client_, *rpc_policy, *backoff_policy, metadata_update_policy,
        &AdminClient::ListSnapshots, request, "ListSnapshotsImpl", status,
        true);
    if (not status.ok()) {
      break;
    }

    for (auto& x : *response.mutable_snapshots()) {
      inserter(x);
    }
    page_token = std::move(*response.mutable_next_page_token());
  } while (not page_token.empty());
}

}  // namespace noex
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
