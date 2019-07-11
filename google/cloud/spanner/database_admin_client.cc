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

DatabaseAdminClient::DatabaseAdminClient(ClientOptions const& client_options)
    : stub_(new internal::DatabaseAdminRetry(
          internal::CreateDefaultDatabaseAdminStub(client_options))) {}

future<StatusOr<gcsa::Database>> DatabaseAdminClient::CreateDatabase(
    std::string const& project_id, std::string const& instance_id,
    std::string const& database_id,
    std::vector<std::string> const& extra_statements) {
  grpc::ClientContext context;
  gcsa::CreateDatabaseRequest request;
  request.set_parent("projects/" + project_id + "/instances/" + instance_id);
  request.set_create_statement("CREATE DATABASE `" + database_id + "`");
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

Status DatabaseAdminClient::DropDatabase(std::string const& project_id,
                                         std::string const& instance_id,
                                         std::string const& database_id) {
  grpc::ClientContext context;
  google::spanner::admin::database::v1::DropDatabaseRequest request;
  request.set_database("projects/" + project_id + "/instances/" + instance_id +
                       "/databases/" + database_id);

  return stub_->DropDatabase(context, request);
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
