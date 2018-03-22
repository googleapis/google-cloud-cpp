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

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
