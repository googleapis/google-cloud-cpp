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
#include "google/cloud/grpc_utils/grpc_error_delegate.h"
#include <algorithm>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

namespace gcsa = google::spanner::admin::database::v1;

DatabaseAdminClient::DatabaseAdminClient(ConnectionOptions const& options)
    : conn_(MakeDatabaseAdminConnection(options)) {}

future<StatusOr<gcsa::Database>> DatabaseAdminClient::CreateDatabase(
    Database db, std::vector<std::string> extra_statements) {
  return conn_->CreateDatabase({std::move(db), std::move(extra_statements)});
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

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
