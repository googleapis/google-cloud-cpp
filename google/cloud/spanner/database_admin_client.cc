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
#include "google/cloud/spanner/internal/database_admin_retry.h"
#include "google/cloud/grpc_utils/grpc_error_delegate.h"
#include <algorithm>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

namespace gcsa = google::spanner::admin::database::v1;

DatabaseAdminClient::DatabaseAdminClient(ConnectionOptions const& options)
    : stub_(internal::CreateDefaultDatabaseAdminStub(options)) {}

future<StatusOr<gcsa::Database>> DatabaseAdminClient::CreateDatabase(
    Database const& db, std::vector<std::string> const& extra_statements) {
  grpc::ClientContext context;
  gcsa::CreateDatabaseRequest request;
  request.set_parent(db.instance().FullName());
  request.set_create_statement("CREATE DATABASE `" + db.database_id() + "`");
  for (auto const& s : extra_statements) {
    *request.add_extra_statements() = s;
  }
  auto operation = stub_->CreateDatabase(context, request);
  if (!operation) {
    return google::cloud::make_ready_future(
        StatusOr<gcsa::Database>(operation.status()));
  }

  return stub_->AwaitCreateDatabase(*std::move(operation));
}

StatusOr<gcsa::Database> DatabaseAdminClient::GetDatabase(Database const& db) {
  grpc::ClientContext context;
  gcsa::GetDatabaseRequest request;
  request.set_name(db.FullName());
  return stub_->GetDatabase(context, request);
}

StatusOr<gcsa::GetDatabaseDdlResponse> DatabaseAdminClient::GetDatabaseDdl(
    Database const& db) {
  grpc::ClientContext context;
  gcsa::GetDatabaseDdlRequest request;
  request.set_database(db.FullName());
  return stub_->GetDatabaseDdl(context, request);
}

future<StatusOr<gcsa::UpdateDatabaseDdlMetadata>>
DatabaseAdminClient::UpdateDatabase(
    Database const& db, std::vector<std::string> const& statements) {
  grpc::ClientContext context;
  gcsa::UpdateDatabaseDdlRequest request;
  request.set_database(db.FullName());
  for (auto const& s : statements) {
    *request.add_statements() = s;
  }
  auto operation = stub_->UpdateDatabase(context, request);
  if (!operation) {
    return google::cloud::make_ready_future(
        StatusOr<gcsa::UpdateDatabaseDdlMetadata>(operation.status()));
  }

  return stub_->AwaitUpdateDatabase(*std::move(operation));
}

ListDatabaseRange DatabaseAdminClient::ListDatabases(Instance const& in) {
  gcsa::ListDatabasesRequest request;
  request.set_parent(in.FullName());
  request.clear_page_token();
  auto stub = stub_;
  return ListDatabaseRange(
      std::move(request),
      [stub](gcsa::ListDatabasesRequest const& r) {
        grpc::ClientContext context;
        return stub->ListDatabases(context, r);
      },
      [](gcsa::ListDatabasesResponse r) {
        std::vector<gcsa::Database> result(r.databases().size());
        auto& dbs = *r.mutable_databases();
        std::move(dbs.begin(), dbs.end(), result.begin());
        return result;
      });
}

Status DatabaseAdminClient::DropDatabase(Database const& db) {
  grpc::ClientContext context;
  google::spanner::admin::database::v1::DropDatabaseRequest request;
  request.set_database(db.FullName());
  return stub_->DropDatabase(context, request);
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
