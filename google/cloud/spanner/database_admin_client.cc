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

// TODO(#7356): Remove this file after the deprecation period expires
#include "google/cloud/internal/disable_deprecation_warnings.inc"
#include "google/cloud/spanner/database_admin_client.h"
#include "google/cloud/spanner/timestamp.h"
#include "google/cloud/options.h"
#include <algorithm>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

namespace gcsa = ::google::spanner::admin::database;

DatabaseAdminClient::DatabaseAdminClient(ConnectionOptions const& options)
    : conn_(MakeDatabaseAdminConnection(options)) {}

future<StatusOr<gcsa::v1::Database>> DatabaseAdminClient::CreateDatabase(
    Database db, std::vector<std::string> extra_statements,
    EncryptionConfig encryption_config) {
  internal::OptionsSpan span(conn_->options());
  return conn_->CreateDatabase({std::move(db), std::move(extra_statements),
                                std::move(encryption_config)});
}

StatusOr<gcsa::v1::Database> DatabaseAdminClient::GetDatabase(Database db) {
  internal::OptionsSpan span(conn_->options());
  return conn_->GetDatabase({std::move(db)});
}

StatusOr<gcsa::v1::GetDatabaseDdlResponse> DatabaseAdminClient::GetDatabaseDdl(
    Database db) {
  internal::OptionsSpan span(conn_->options());
  return conn_->GetDatabaseDdl({std::move(db)});
}

future<StatusOr<gcsa::v1::UpdateDatabaseDdlMetadata>>
DatabaseAdminClient::UpdateDatabase(Database db,
                                    std::vector<std::string> statements) {
  internal::OptionsSpan span(conn_->options());
  return conn_->UpdateDatabase({std::move(db), std::move(statements)});
}

ListDatabaseRange DatabaseAdminClient::ListDatabases(Instance in) {
  internal::OptionsSpan span(conn_->options());
  return conn_->ListDatabases({std::move(in)});
}

Status DatabaseAdminClient::DropDatabase(Database db) {
  internal::OptionsSpan span(conn_->options());
  return conn_->DropDatabase({std::move(db)});
}

future<StatusOr<gcsa::v1::Database>> DatabaseAdminClient::RestoreDatabase(
    Database db, Backup const& backup, EncryptionConfig encryption_config) {
  internal::OptionsSpan span(conn_->options());
  return conn_->RestoreDatabase(
      {std::move(db), backup.FullName(), std::move(encryption_config)});
}

future<StatusOr<gcsa::v1::Database>> DatabaseAdminClient::RestoreDatabase(
    Database db, google::spanner::admin::database::v1::Backup const& backup,
    EncryptionConfig encryption_config) {
  internal::OptionsSpan span(conn_->options());
  return conn_->RestoreDatabase(
      {std::move(db), backup.name(), std::move(encryption_config)});
}

StatusOr<google::iam::v1::Policy> DatabaseAdminClient::GetIamPolicy(
    Database db) {
  internal::OptionsSpan span(conn_->options());
  return conn_->GetIamPolicy({std::move(db)});
}

StatusOr<google::iam::v1::Policy> DatabaseAdminClient::SetIamPolicy(
    Database db, google::iam::v1::Policy policy) {
  internal::OptionsSpan span(conn_->options());
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
  internal::OptionsSpan span(conn_->options());
  using RerunnablePolicy = spanner_internal::SafeTransactionRerun;

  Status last_status;
  do {
    auto current_policy = conn_->GetIamPolicy({db});
    if (!current_policy) {
      last_status = std::move(current_policy).status();
    } else {
      auto etag = current_policy->etag();
      auto desired = updater(*current_policy);
      if (!desired.has_value()) {
        return current_policy;
      }
      desired->set_etag(std::move(etag));
      auto result = conn_->SetIamPolicy({db, *std::move(desired)});
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
  internal::OptionsSpan span(conn_->options());
  return conn_->TestIamPermissions({std::move(db), std::move(permissions)});
}

future<StatusOr<gcsa::v1::Backup>> DatabaseAdminClient::CreateBackup(
    Database db, std::string backup_id, Timestamp expire_time,
    absl::optional<Timestamp> version_time,
    EncryptionConfig encryption_config) {
  internal::OptionsSpan span(conn_->options());
  auto expire_time_point =
      expire_time.get<std::chrono::system_clock::time_point>();
  if (!expire_time_point) {
    expire_time_point = std::chrono::system_clock::time_point::max();
  }
  return conn_->CreateBackup(
      {std::move(db), std::move(backup_id), *std::move(expire_time_point),
       expire_time, std::move(version_time), std::move(encryption_config)});
}

future<StatusOr<gcsa::v1::Backup>> DatabaseAdminClient::CreateBackup(
    Database db, std::string backup_id,
    std::chrono::system_clock::time_point expire_time) {
  internal::OptionsSpan span(conn_->options());
  auto ts = MakeTimestamp(expire_time);
  if (!ts) return make_ready_future(StatusOr<gcsa::v1::Backup>(ts.status()));
  return CreateBackup(std::move(db), std::move(backup_id), *ts);
}

StatusOr<gcsa::v1::Backup> DatabaseAdminClient::GetBackup(
    Backup const& backup) {
  internal::OptionsSpan span(conn_->options());
  return conn_->GetBackup({backup.FullName()});
}

Status DatabaseAdminClient::DeleteBackup(
    google::spanner::admin::database::v1::Backup const& backup) {
  internal::OptionsSpan span(conn_->options());
  return conn_->DeleteBackup({backup.name()});
}

Status DatabaseAdminClient::DeleteBackup(Backup const& backup) {
  internal::OptionsSpan span(conn_->options());
  return conn_->DeleteBackup({backup.FullName()});
}

ListBackupsRange DatabaseAdminClient::ListBackups(Instance in,
                                                  std::string filter) {
  internal::OptionsSpan span(conn_->options());
  return conn_->ListBackups({std::move(in), std::move(filter)});
}

StatusOr<gcsa::v1::Backup> DatabaseAdminClient::UpdateBackupExpireTime(
    google::spanner::admin::database::v1::Backup const& backup,
    Timestamp expire_time) {
  internal::OptionsSpan span(conn_->options());
  google::spanner::admin::database::v1::UpdateBackupRequest request;
  request.mutable_backup()->set_name(backup.name());
  *request.mutable_backup()->mutable_expire_time() =
      expire_time.get<protobuf::Timestamp>().value();
  request.mutable_update_mask()->add_paths("expire_time");
  return conn_->UpdateBackup({request});
}

StatusOr<gcsa::v1::Backup> DatabaseAdminClient::UpdateBackupExpireTime(
    Backup const& backup, Timestamp expire_time) {
  internal::OptionsSpan span(conn_->options());
  google::spanner::admin::database::v1::UpdateBackupRequest request;
  request.mutable_backup()->set_name(backup.FullName());
  *request.mutable_backup()->mutable_expire_time() =
      expire_time.get<protobuf::Timestamp>().value();
  request.mutable_update_mask()->add_paths("expire_time");
  return conn_->UpdateBackup({request});
}

StatusOr<gcsa::v1::Backup> DatabaseAdminClient::UpdateBackupExpireTime(
    google::spanner::admin::database::v1::Backup const& backup,
    std::chrono::system_clock::time_point const& expire_time) {
  auto ts = MakeTimestamp(expire_time);
  if (!ts) return ts.status();
  return UpdateBackupExpireTime(backup, *ts);
}

StatusOr<gcsa::v1::Backup> DatabaseAdminClient::UpdateBackupExpireTime(
    Backup const& backup,
    std::chrono::system_clock::time_point const& expire_time) {
  auto ts = MakeTimestamp(expire_time);
  if (!ts) return ts.status();
  return UpdateBackupExpireTime(backup, *ts);
}

ListBackupOperationsRange DatabaseAdminClient::ListBackupOperations(
    Instance in, std::string filter) {
  internal::OptionsSpan span(conn_->options());
  return conn_->ListBackupOperations({std::move(in), std::move(filter)});
}

ListDatabaseOperationsRange DatabaseAdminClient::ListDatabaseOperations(
    Instance in, std::string filter) {
  internal::OptionsSpan span(conn_->options());
  return conn_->ListDatabaseOperations({std::move(in), std::move(filter)});
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
