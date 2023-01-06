// Copyright 2017 Google Inc.
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

#include "google/cloud/bigtable/table_admin.h"
#include "google/cloud/bigtable/admin/bigtable_table_admin_client.h"
#include "google/cloud/bigtable/wait_for_consistency.h"
#include "google/cloud/internal/retry_policy.h"
#include "google/cloud/internal/time_utils.h"
#include <google/protobuf/duration.pb.h>
#include <sstream>

namespace btadmin = ::google::bigtable::admin::v2;

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
static_assert(std::is_copy_constructible<bigtable::TableAdmin>::value,
              "bigtable::TableAdmin must be constructible");
static_assert(std::is_copy_assignable<bigtable::TableAdmin>::value,
              "bigtable::TableAdmin must be assignable");

// NOLINTNEXTLINE(readability-identifier-naming)
constexpr TableAdmin::TableView TableAdmin::ENCRYPTION_VIEW;
// NOLINTNEXTLINE(readability-identifier-naming)
constexpr TableAdmin::TableView TableAdmin::FULL;
// NOLINTNEXTLINE(readability-identifier-naming)
constexpr TableAdmin::TableView TableAdmin::NAME_ONLY;
// NOLINTNEXTLINE(readability-identifier-naming)
constexpr TableAdmin::TableView TableAdmin::REPLICATION_VIEW;
// NOLINTNEXTLINE(readability-identifier-naming)
constexpr TableAdmin::TableView TableAdmin::SCHEMA_VIEW;
// NOLINTNEXTLINE(readability-identifier-naming)
constexpr TableAdmin::TableView TableAdmin::VIEW_UNSPECIFIED;

StatusOr<btadmin::Table> TableAdmin::CreateTable(std::string table_id,
                                                 TableConfig config) {
  google::cloud::internal::OptionsSpan span(options_);
  auto request = std::move(config).as_proto();
  request.set_parent(instance_name());
  request.set_table_id(std::move(table_id));
  return connection_->CreateTable(request);
}

StatusOr<std::vector<btadmin::Table>> TableAdmin::ListTables(
    btadmin::Table::View view) {
  google::cloud::internal::OptionsSpan span(options_);
  std::vector<btadmin::Table> result;

  btadmin::ListTablesRequest request;
  request.set_parent(instance_name());
  request.set_view(view);
  auto sr = connection_->ListTables(request);
  for (auto& t : sr) {
    if (!t) return std::move(t).status();
    result.emplace_back(*std::move(t));
  }
  return result;
}

StatusOr<btadmin::Table> TableAdmin::GetTable(std::string const& table_id,
                                              btadmin::Table::View view) {
  google::cloud::internal::OptionsSpan span(options_);
  btadmin::GetTableRequest request;
  request.set_name(TableName(table_id));
  request.set_view(view);
  return connection_->GetTable(request);
}

Status TableAdmin::DeleteTable(std::string const& table_id) {
  google::cloud::internal::OptionsSpan span(options_);
  btadmin::DeleteTableRequest request;
  request.set_name(TableName(table_id));
  return connection_->DeleteTable(request);
}

btadmin::CreateBackupRequest TableAdmin::CreateBackupParams::AsProto(
    std::string instance_name) const {
  btadmin::CreateBackupRequest proto;
  proto.set_parent(instance_name + "/clusters/" + cluster_id);
  proto.set_backup_id(backup_id);
  proto.mutable_backup()->set_source_table(std::move(instance_name) +
                                           "/tables/" + table_name);
  *proto.mutable_backup()->mutable_expire_time() =
      google::cloud::internal::ToProtoTimestamp(expire_time);
  return proto;
}

StatusOr<btadmin::Backup> TableAdmin::CreateBackup(
    CreateBackupParams const& params) {
  google::cloud::internal::OptionsSpan span(options_);
  auto request = params.AsProto(instance_name());
  return connection_->CreateBackup(request).get();
}

StatusOr<btadmin::Backup> TableAdmin::GetBackup(std::string const& cluster_id,
                                                std::string const& backup_id) {
  google::cloud::internal::OptionsSpan span(options_);
  btadmin::GetBackupRequest request;
  request.set_name(BackupName(cluster_id, backup_id));
  return connection_->GetBackup(request);
}

btadmin::UpdateBackupRequest TableAdmin::UpdateBackupParams::AsProto(
    std::string const& instance_name) const {
  btadmin::UpdateBackupRequest proto;
  proto.mutable_backup()->set_name(instance_name + "/clusters/" + cluster_id +
                                   "/backups/" + backup_name);
  *proto.mutable_backup()->mutable_expire_time() =
      google::cloud::internal::ToProtoTimestamp(expire_time);
  proto.mutable_update_mask()->add_paths("expire_time");
  return proto;
}

StatusOr<btadmin::Backup> TableAdmin::UpdateBackup(
    UpdateBackupParams const& params) {
  google::cloud::internal::OptionsSpan span(options_);
  btadmin::UpdateBackupRequest request = params.AsProto(instance_name());
  return connection_->UpdateBackup(request);
}

Status TableAdmin::DeleteBackup(btadmin::Backup const& backup) {
  google::cloud::internal::OptionsSpan span(options_);
  btadmin::DeleteBackupRequest request;
  request.set_name(backup.name());
  return connection_->DeleteBackup(request);
}

Status TableAdmin::DeleteBackup(std::string const& cluster_id,
                                std::string const& backup_id) {
  google::cloud::internal::OptionsSpan span(options_);
  btadmin::DeleteBackupRequest request;
  request.set_name(BackupName(cluster_id, backup_id));
  return connection_->DeleteBackup(request);
}

btadmin::ListBackupsRequest TableAdmin::ListBackupsParams::AsProto(
    std::string const& instance_name) const {
  btadmin::ListBackupsRequest proto;
  proto.set_parent(cluster_id ? instance_name + "/clusters/" + *cluster_id
                              : instance_name + "/clusters/-");
  if (filter) *proto.mutable_filter() = *filter;
  if (order_by) *proto.mutable_order_by() = *order_by;
  return proto;
}

StatusOr<std::vector<btadmin::Backup>> TableAdmin::ListBackups(
    ListBackupsParams const& params) {
  google::cloud::internal::OptionsSpan span(options_);
  std::vector<btadmin::Backup> result;

  btadmin::ListBackupsRequest request = params.AsProto(instance_name());
  auto sr = connection_->ListBackups(request);
  for (auto& b : sr) {
    if (!b) return std::move(b).status();
    result.emplace_back(*std::move(b));
  }
  return result;
}

btadmin::RestoreTableRequest TableAdmin::RestoreTableParams::AsProto(
    std::string const& instance_name) const {
  btadmin::RestoreTableRequest proto;
  proto.set_parent(instance_name);
  proto.set_table_id(table_id);
  proto.set_backup(instance_name + "/clusters/" + cluster_id + "/backups/" +
                   backup_id);
  return proto;
}

StatusOr<btadmin::Table> TableAdmin::RestoreTable(
    RestoreTableParams const& params) {
  auto p = RestoreTableFromInstanceParams{
      params.table_id, BackupName(params.cluster_id, params.backup_id)};
  return RestoreTable(std::move(p));
}

btadmin::RestoreTableRequest AsProto(
    std::string const& instance_name,
    TableAdmin::RestoreTableFromInstanceParams p) {
  btadmin::RestoreTableRequest proto;
  proto.set_parent(instance_name);
  proto.set_table_id(std::move(p.table_id));
  proto.set_backup(std::move(p.backup_name));
  return proto;
}

StatusOr<btadmin::Table> TableAdmin::RestoreTable(
    RestoreTableFromInstanceParams params) {
  google::cloud::internal::OptionsSpan span(options_);
  auto request = AsProto(instance_name(), std::move(params));
  return connection_->RestoreTable(request).get();
}

StatusOr<btadmin::Table> TableAdmin::ModifyColumnFamilies(
    std::string const& table_id,
    std::vector<ColumnFamilyModification> modifications) {
  google::cloud::internal::OptionsSpan span(options_);
  btadmin::ModifyColumnFamiliesRequest request;
  request.set_name(TableName(table_id));
  for (auto& m : modifications) {
    google::cloud::internal::OptionsSpan span(options_);
    *request.add_modifications() = std::move(m).as_proto();
  }
  return connection_->ModifyColumnFamilies(request);
}

Status TableAdmin::DropRowsByPrefix(std::string const& table_id,
                                    std::string row_key_prefix) {
  google::cloud::internal::OptionsSpan span(options_);
  btadmin::DropRowRangeRequest request;
  request.set_name(TableName(table_id));
  request.set_row_key_prefix(std::move(row_key_prefix));
  return connection_->DropRowRange(request);
}

future<StatusOr<Consistency>> TableAdmin::WaitForConsistency(
    std::string const& table_id, std::string const& consistency_token) {
  // We avoid lifetime issues due to ownership cycles, by holding the
  // `BackgroundThreads` which run the `CompletionQueue` outside of the
  // operation, in this class. If the `BackgroundThreads` running the
  // `CompletionQueue` were instead owned by the Connection, we would have an
  // ownership cycle. We have made this mistake before. See #7740 for more
  // details.
  auto client = bigtable_admin::BigtableTableAdminClient(connection_);
  return bigtable_admin::AsyncWaitForConsistency(cq_, std::move(client),
                                                 TableName(table_id),
                                                 consistency_token, options_)
      .then([](future<Status> f) -> StatusOr<Consistency> {
        auto s = f.get();
        if (!s.ok()) return s;
        return Consistency::kConsistent;
      });
}

Status TableAdmin::DropAllRows(std::string const& table_id) {
  google::cloud::internal::OptionsSpan span(options_);
  btadmin::DropRowRangeRequest request;
  request.set_name(TableName(table_id));
  request.set_delete_all_data_from_table(true);
  return connection_->DropRowRange(request);
}

StatusOr<std::string> TableAdmin::GenerateConsistencyToken(
    std::string const& table_id) {
  google::cloud::internal::OptionsSpan span(options_);
  btadmin::GenerateConsistencyTokenRequest request;
  request.set_name(TableName(table_id));
  auto sor = connection_->GenerateConsistencyToken(request);
  if (!sor) return std::move(sor).status();
  return std::move(*sor->mutable_consistency_token());
}

StatusOr<Consistency> TableAdmin::CheckConsistency(
    std::string const& table_id, std::string const& consistency_token) {
  google::cloud::internal::OptionsSpan span(options_);
  btadmin::CheckConsistencyRequest request;
  request.set_name(TableName(table_id));
  request.set_consistency_token(consistency_token);
  auto sor = connection_->CheckConsistency(request);
  if (!sor) return std::move(sor).status();
  return sor->consistent() ? Consistency::kConsistent
                           : Consistency::kInconsistent;
}

StatusOr<google::iam::v1::Policy> TableAdmin::GetIamPolicy(
    std::string const& table_id) {
  return GetIamPolicyImpl(TableName(table_id));
}

StatusOr<google::iam::v1::Policy> TableAdmin::GetIamPolicy(
    std::string const& cluster_id, std::string const& backup_id) {
  return GetIamPolicyImpl(BackupName(cluster_id, backup_id));
}

StatusOr<google::iam::v1::Policy> TableAdmin::GetIamPolicyImpl(
    std::string resource) {
  google::cloud::internal::OptionsSpan span(options_);
  ::google::iam::v1::GetIamPolicyRequest request;
  request.set_resource(std::move(resource));
  return connection_->GetIamPolicy(request);
}

StatusOr<google::iam::v1::Policy> TableAdmin::SetIamPolicy(
    std::string const& table_id, google::iam::v1::Policy const& iam_policy) {
  return SetIamPolicyImpl(TableName(table_id), iam_policy);
}

StatusOr<google::iam::v1::Policy> TableAdmin::SetIamPolicy(
    std::string const& cluster_id, std::string const& backup_id,
    google::iam::v1::Policy const& iam_policy) {
  return SetIamPolicyImpl(BackupName(cluster_id, backup_id), iam_policy);
}

StatusOr<google::iam::v1::Policy> TableAdmin::SetIamPolicyImpl(
    std::string resource, google::iam::v1::Policy const& iam_policy) {
  google::cloud::internal::OptionsSpan span(options_);
  ::google::iam::v1::SetIamPolicyRequest request;
  request.set_resource(std::move(resource));
  *request.mutable_policy() = iam_policy;
  return connection_->SetIamPolicy(request);
}

StatusOr<std::vector<std::string>> TableAdmin::TestIamPermissions(
    std::string const& table_id, std::vector<std::string> const& permissions) {
  return TestIamPermissionsImpl(TableName(table_id), permissions);
}

StatusOr<std::vector<std::string>> TableAdmin::TestIamPermissions(
    std::string const& cluster_id, std::string const& backup_id,
    std::vector<std::string> const& permissions) {
  return TestIamPermissionsImpl(BackupName(cluster_id, backup_id), permissions);
}

StatusOr<std::vector<std::string>> TableAdmin::TestIamPermissionsImpl(
    std::string resource, std::vector<std::string> const& permissions) {
  google::cloud::internal::OptionsSpan span(options_);
  ::google::iam::v1::TestIamPermissionsRequest request;
  request.set_resource(std::move(resource));
  for (auto const& permission : permissions) {
    request.add_permissions(permission);
  }
  auto sor = connection_->TestIamPermissions(request);
  if (!sor) return std::move(sor).status();
  auto response = *std::move(sor);
  std::vector<std::string> result;
  auto& ps = *response.mutable_permissions();
  std::move(ps.begin(), ps.end(), std::back_inserter(result));
  return result;
}

std::string TableAdmin::InstanceName() const {
  return google::cloud::bigtable::InstanceName(project_id_, instance_id_);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
