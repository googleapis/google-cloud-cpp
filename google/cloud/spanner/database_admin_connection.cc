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

#include "google/cloud/spanner/database_admin_connection.h"
#include "google/cloud/spanner/internal/database_admin_retry.h"

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace gcsa = ::google::spanner::admin::database::v1;

namespace {
class DatabaseAdminConnectionImpl : public DatabaseAdminConnection {
 public:
  explicit DatabaseAdminConnectionImpl(
      std::shared_ptr<internal::DatabaseAdminStub> stub)
      : stub_(std::move(stub)) {}
  ~DatabaseAdminConnectionImpl() override = default;

  future<StatusOr<google::spanner::admin::database::v1::Database>>
  CreateDatabase(CreateDatabaseParams p) override {
    gcsa::CreateDatabaseRequest request;
    request.set_parent(p.database.instance().FullName());
    request.set_create_statement("CREATE DATABASE `" +
                                 p.database.database_id() + "`");
    for (auto& s : p.extra_statements) {
      *request.add_extra_statements() = std::move(s);
    }

    grpc::ClientContext context;
    auto operation = stub_->CreateDatabase(context, request);
    if (!operation) {
      return google::cloud::make_ready_future(
          StatusOr<gcsa::Database>(operation.status()));
    }

    return stub_->AwaitCreateDatabase(*std::move(operation));
  }

  StatusOr<google::spanner::admin::database::v1::Database> GetDatabase(
      GetDatabaseParams p) override {
    gcsa::GetDatabaseRequest request;
    request.set_name(p.database.FullName());
    grpc::ClientContext context;
    return stub_->GetDatabase(context, request);
  }

  StatusOr<google::spanner::admin::database::v1::GetDatabaseDdlResponse>
  GetDatabaseDdl(GetDatabaseDdlParams p) override {
    gcsa::GetDatabaseDdlRequest request;
    request.set_database(p.database.FullName());
    grpc::ClientContext context;
    return stub_->GetDatabaseDdl(context, request);
  }

  future<
      StatusOr<google::spanner::admin::database::v1::UpdateDatabaseDdlMetadata>>
  UpdateDatabase(UpdateDatabaseParams p) override {
    gcsa::UpdateDatabaseDdlRequest request;
    request.set_database(p.database.FullName());
    for (auto& s : p.statements) {
      *request.add_statements() = std::move(s);
    }
    grpc::ClientContext context;
    auto operation = stub_->UpdateDatabase(context, request);
    if (!operation) {
      return google::cloud::make_ready_future(
          StatusOr<gcsa::UpdateDatabaseDdlMetadata>(operation.status()));
    }

    return stub_->AwaitUpdateDatabase(*std::move(operation));
  }

  Status DropDatabase(DropDatabaseParams p) override {
    google::spanner::admin::database::v1::DropDatabaseRequest request;
    request.set_database(p.database.FullName());
    grpc::ClientContext context;
    return stub_->DropDatabase(context, request);
  }

  ListDatabaseRange ListDatabases(ListDatabasesParams p) override {
    gcsa::ListDatabasesRequest request;
    request.set_parent(p.instance.FullName());
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

 private:
  std::shared_ptr<internal::DatabaseAdminStub> stub_;
};
}  // namespace

DatabaseAdminConnection::~DatabaseAdminConnection() = default;

std::shared_ptr<DatabaseAdminConnection> MakeDatabaseAdminConnection(
    ConnectionOptions const& options) {
  return std::make_shared<DatabaseAdminConnectionImpl>(
      internal::CreateDefaultDatabaseAdminStub(options));
}

namespace internal {

std::shared_ptr<DatabaseAdminConnection> MakeDatabaseAdminConnection(
    std::shared_ptr<internal::DatabaseAdminStub> stub,
    std::unique_ptr<RetryPolicy> retry_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy) {
  stub = std::make_shared<DatabaseAdminRetry>(
      std::move(stub), *std::move(retry_policy), *std::move(backoff_policy));
  return std::make_shared<DatabaseAdminConnectionImpl>(std::move(stub));
}

std::shared_ptr<DatabaseAdminConnection> MakePlainDatabaseAdminConnection(
    std::shared_ptr<internal::DatabaseAdminStub> stub) {
  return std::make_shared<DatabaseAdminConnectionImpl>(std::move(stub));
}

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
