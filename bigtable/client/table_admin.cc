// Copyright 2017 Google Inc.
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
#include "bigtable/client/internal/throw_delegate.h"
#include <sstream>

namespace btproto = ::google::bigtable::admin::v2;

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
::google::bigtable::admin::v2::Table TableAdmin::CreateTable(
    std::string table_id, TableConfig config) {
  grpc::Status status;
  auto result =
      impl_.CreateTable(std::move(table_id), std::move(config), status);
  if (not status.ok()) {
    internal::RaiseRpcError(status, status.error_message());
  }
  return result;
}

std::vector<::google::bigtable::admin::v2::Table> TableAdmin::ListTables(
    ::google::bigtable::admin::v2::Table::View view) {
  grpc::Status status;
  auto result = impl_.ListTables(std::move(view), status);
  if (not status.ok()) {
    internal::RaiseRpcError(status, status.error_message());
  }
  return result;
}

::google::bigtable::admin::v2::Table TableAdmin::GetTable(
    std::string table_id, ::google::bigtable::admin::v2::Table::View view) {
  grpc::Status status;
  auto result = impl_.GetTable(std::move(table_id), status, std::move(view));
  if (not status.ok()) {
    internal::RaiseRpcError(status, status.error_message());
  }
  return result;
}

void TableAdmin::DeleteTable(std::string table_id) {
  grpc::Status status;
  impl_.DeleteTable(std::move(table_id), status);
  if (not status.ok()) {
    internal::RaiseRpcError(status, status.error_message());
  }
}

::google::bigtable::admin::v2::Table TableAdmin::ModifyColumnFamilies(
    std::string table_id, std::vector<ColumnFamilyModification> modifications) {
  grpc::Status status;
  auto result = impl_.ModifyColumnFamilies(std::move(table_id),
                                           std::move(modifications), status);
  if (not status.ok()) {
    internal::RaiseRpcError(status, status.error_message());
  }
  return result;
}

void TableAdmin::DropRowsByPrefix(std::string table_id,
                                  std::string row_key_prefix) {
  grpc::Status status;
  impl_.DropRowsByPrefix(std::move(table_id), std::move(row_key_prefix),
                         status);
  if (not status.ok()) {
    internal::RaiseRpcError(status, status.error_message());
  }
}

void TableAdmin::DropAllRows(std::string table_id) {
  grpc::Status status;
  impl_.DropAllRows(std::move(table_id), status);
  if (not status.ok()) {
    internal::RaiseRpcError(status, status.error_message());
  }
}

::google::bigtable::admin::v2::Snapshot TableAdmin::GetSnapshot(
    bigtable::ClusterId const& cluster_id,
    bigtable::SnapshotId const& snapshot_id) {
  grpc::Status status;
  auto result = impl_.GetSnapshot(cluster_id, snapshot_id, status);
  if (not status.ok()) {
    internal::RaiseRpcError(status, status.error_message());
  }
  return result;
}

std::string TableAdmin::GenerateConsistencyToken(std::string const& table_id) {
  grpc::Status status;
  std::string token =
      impl_.GenerateConsistencyToken(std::move(table_id), status);
  if (not status.ok()) {
    internal::RaiseRpcError(status, status.error_message());
  }
  return token;
}

bool TableAdmin::CheckConsistency(
    bigtable::TableId const& table_id,
    bigtable::ConsistencyToken const& consistency_token) {
  grpc::Status status;
  bool consistent = impl_.CheckConsistency(table_id, consistency_token, status);
  if (not status.ok()) {
    internal::RaiseRpcError(status, status.error_message());
  }
  return consistent;
}

void TableAdmin::DeleteSnapshot(bigtable::ClusterId const& cluster_id,
                                bigtable::SnapshotId const& snapshot_id) {
  grpc::Status status;
  impl_.DeleteSnapshot(cluster_id, snapshot_id, status);
  if (not status.ok()) {
    internal::RaiseRpcError(status, status.error_message());
  }
}

std::vector<btproto::Snapshot> TableAdmin::ListSnapshots(int32_t page_size,
                                                         ClusterId cluster_id) {
  grpc::Status status;
  auto result = impl_.ListSnapshots(page_size, status, cluster_id);
  if (not status.ok()) {
    internal::RaiseRpcError(status, status.error_message());
  }
  return result;
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
