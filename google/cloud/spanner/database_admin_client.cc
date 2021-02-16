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

#include "google/cloud/spanner/database_admin_client.h"
#include <algorithm>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

namespace gcsa = ::google::spanner::admin::database::v1;

DatabaseAdminClient::DatabaseAdminClient(ConnectionOptions const& options)
    : conn_(MakeDatabaseAdminConnection(options)) {}

future<StatusOr<gcsa::Database>> DatabaseAdminClient::CreateDatabase(
    Database db, std::vector<std::string> extra_statements,
    absl::optional<KmsKeyName> encryption_key) {
  return conn_->CreateDatabase(
      {std::move(db), std::move(extra_statements), std::move(encryption_key)});
}

StatusOr<gcsa::Database> DatabaseAdminClient::GetDatabase(Database db) {
  return conn_->GetDatabase({std::move(db)});
}

StatusOr<gcsa::GetDatabaseDdlResponse> DatabaseAdminClient::GetDatabaseDdl(
    Database db) {
  return conn_->GetDatabaseDdl({std::move(db)});
}

future<StatusOr<gcsa::UpdateDatabaseDdlMetadata>>
DatabaseAdminClient::UpdateDatabase(Database db,
                                    std::vector<std::string> statements) {
  return conn_->UpdateDatabase({std::move(db), std::move(statements)});
}

ListDatabaseRange DatabaseAdminClient::ListDatabases(Instance in) {
  return conn_->ListDatabases({std::move(in)});
}

Status DatabaseAdminClient::DropDatabase(Database db) {
  return conn_->DropDatabase({std::move(db)});
}

future<StatusOr<gcsa::Database>> DatabaseAdminClient::RestoreDatabase(
    Database db, Backup const& backup,
    absl::optional<KmsKeyName> encryption_key) {
  return conn_->RestoreDatabase(
      {std::move(db), backup.FullName(), std::move(encryption_key)});
}

future<StatusOr<gcsa::Database>> DatabaseAdminClient::RestoreDatabase(
    Database db, google::spanner::admin::database::v1::Backup const& backup,
    absl::optional<KmsKeyName> encryption_key) {
  return conn_->RestoreDatabase(
      {std::move(db), backup.name(), std::move(encryption_key)});
}

StatusOr<google::iam::v1::Policy> DatabaseAdminClient::GetIamPolicy(
    Database db) {
  return conn_->GetIamPolicy({std::move(db)});
}

StatusOr<google::iam::v1::Policy> DatabaseAdminClient::SetIamPolicy(
    Database db, google::iam::v1::Policy policy) {
  return conn_->SetIamPolicy({std::move(db), std::move(policy)});
}

StatusOr<google::iam::v1::Policy> DatabaseAdminClient::SetIamPolicy(
    Database const& db, IamUpdater const& updater) {
  auto const rerun_maximum_duration = std::chrono::minutes(15);
  auto default_rerun_policy =
      LimitedTimeTransactionRerunPolicy(rerun_maximum_duration).clone();

  auto const backoff_initial_delay = std::chrono::milliseconds(1000);
  auto const backoff_maximum_delay = std::chrono::minutes(5);
  auto const backoff_scaling = 2.0;
  auto default_backoff_policy =
      ExponentialBackoffPolicy(backoff_initial_delay, backoff_maximum_delay,
                               backoff_scaling)
          .clone();

  return SetIamPolicy(db, updater, std::move(default_rerun_policy),
                      std::move(default_backoff_policy));
}

StatusOr<google::iam::v1::Policy> DatabaseAdminClient::SetIamPolicy(
    Database const& db, IamUpdater const& updater,
    std::unique_ptr<TransactionRerunPolicy> rerun_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy) {
  using RerunnablePolicy = internal::SafeTransactionRerun;

  Status last_status;
  do {
    auto current_policy = GetIamPolicy(db);
    if (!current_policy) {
      last_status = std::move(current_policy).status();
    } else {
      auto etag = current_policy->etag();
      auto desired = updater(*current_policy);
      if (!desired.has_value()) {
        return current_policy;
      }
      desired->set_etag(std::move(etag));
      auto result = SetIamPolicy(db, *std::move(desired));
      if (RerunnablePolicy::IsOk(result.status())) {
        return result;
      }
      last_status = std::move(result).status();
    }
    if (!rerun_policy->OnFailure(last_status)) break;
    std::this_thread::sleep_for(backoff_policy->OnCompletion());
  } while (!rerun_policy->IsExhausted());
  return last_status;
}

StatusOr<google::iam::v1::TestIamPermissionsResponse>
DatabaseAdminClient::TestIamPermissions(Database db,
                                        std::vector<std::string> permissions) {
  return conn_->TestIamPermissions({std::move(db), std::move(permissions)});
}

future<StatusOr<gcsa::Backup>> DatabaseAdminClient::CreateBackup(
    Database db, std::string backup_id,
    std::chrono::system_clock::time_point expire_time) {
  auto ts = MakeTimestamp(expire_time);
  if (!ts) return make_ready_future(StatusOr<gcsa::Backup>(ts.status()));
  return CreateBackup(std::move(db), std::move(backup_id), *ts);
}

future<StatusOr<gcsa::Backup>> DatabaseAdminClient::CreateBackup(
    Database db, std::string backup_id, Timestamp expire_time,
    absl::optional<Timestamp> version_time,
    absl::optional<KmsKeyName> encryption_key) {
  auto expire_time_point =
      expire_time.get<std::chrono::system_clock::time_point>();
  if (!expire_time_point) {
    expire_time_point = std::chrono::system_clock::time_point::max();
  }
  return conn_->CreateBackup(
      {std::move(db), std::move(backup_id), *std::move(expire_time_point),
       expire_time, std::move(version_time), std::move(encryption_key)});
}

StatusOr<gcsa::Backup> DatabaseAdminClient::GetBackup(Backup const& backup) {
  return conn_->GetBackup({backup.FullName()});
}

Status DatabaseAdminClient::DeleteBackup(
    google::spanner::admin::database::v1::Backup const& backup) {
  return conn_->DeleteBackup({backup.name()});
}

Status DatabaseAdminClient::DeleteBackup(Backup const& backup) {
  return conn_->DeleteBackup({backup.FullName()});
}

ListBackupsRange DatabaseAdminClient::ListBackups(Instance in,
                                                  std::string filter) {
  return conn_->ListBackups({std::move(in), std::move(filter)});
}

StatusOr<gcsa::Backup> DatabaseAdminClient::UpdateBackupExpireTime(
    google::spanner::admin::database::v1::Backup const& backup,
    Timestamp expire_time) {
  google::spanner::admin::database::v1::UpdateBackupRequest request;
  request.mutable_backup()->set_name(backup.name());
  *request.mutable_backup()->mutable_expire_time() =
      expire_time.get<protobuf::Timestamp>().value();
  request.mutable_update_mask()->add_paths("expire_time");
  return conn_->UpdateBackup({request});
}

StatusOr<gcsa::Backup> DatabaseAdminClient::UpdateBackupExpireTime(
    Backup const& backup, Timestamp expire_time) {
  google::spanner::admin::database::v1::UpdateBackupRequest request;
  request.mutable_backup()->set_name(backup.FullName());
  *request.mutable_backup()->mutable_expire_time() =
      expire_time.get<protobuf::Timestamp>().value();
  request.mutable_update_mask()->add_paths("expire_time");
  return conn_->UpdateBackup({request});
}

StatusOr<gcsa::Backup> DatabaseAdminClient::UpdateBackupExpireTime(
    google::spanner::admin::database::v1::Backup const& backup,
    std::chrono::system_clock::time_point const& expire_time) {
  auto ts = MakeTimestamp(expire_time);
  if (!ts) return ts.status();
  return UpdateBackupExpireTime(backup, *ts);
}

StatusOr<gcsa::Backup> DatabaseAdminClient::UpdateBackupExpireTime(
    Backup const& backup,
    std::chrono::system_clock::time_point const& expire_time) {
  auto ts = MakeTimestamp(expire_time);
  if (!ts) return ts.status();
  return UpdateBackupExpireTime(backup, *ts);
}

ListBackupOperationsRange DatabaseAdminClient::ListBackupOperations(
    Instance in, std::string filter) {
  return conn_->ListBackupOperations({std::move(in), std::move(filter)});
}

ListDatabaseOperationsRange DatabaseAdminClient::ListDatabaseOperations(
    Instance in, std::string filter) {
  return conn_->ListDatabaseOperations({std::move(in), std::move(filter)});
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
