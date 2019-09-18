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

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
