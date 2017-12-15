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

#include "bigtable/admin/table_admin.h"

#include <sstream>
#include <thread>

#include <absl/strings/str_cat.h>

namespace btproto = ::google::bigtable::admin::v2;

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
::google::bigtable::admin::v2::Table TableAdmin::CreateTable(
    std::string table_id, TableConfig config) {
  // Copy the policies in effect for the operation.
  auto rpc_policy = rpc_retry_policy_->clone();
  auto backoff_policy = rpc_backoff_policy_->clone();

  auto request = config.as_proto_move();
  request.set_parent(instance_name());
  request.set_table_id(std::move(table_id));

  return call_with_retry(
      &btproto::BigtableTableAdmin::StubInterface::CreateTable, request,
      absl::StrCat("CreateTable(", request.table_id(), ")"));
}

std::vector<::google::bigtable::admin::v2::Table> TableAdmin::ListTables(
    ::google::bigtable::admin::v2::Table::View view) {
  // Copy the policies in effect for the operation.
  auto rpc_policy = rpc_retry_policy_->clone();
  auto backoff_policy = rpc_backoff_policy_->clone();

  // Build the RPC request, try to minimize copying.
  std::vector<btproto::Table> result;
  std::string page_token;
  while (true) {
    btproto::ListTablesRequest request;
    request.set_page_token(std::move(page_token));
    request.set_parent(instance_name());
    request.set_view(view);

    btproto::ListTablesResponse response;
    grpc::ClientContext client_context;
    rpc_policy->setup(client_context);
    backoff_policy->setup(client_context);
    grpc::Status status =
        client_->table_admin().ListTables(&client_context, request, &response);
    client_->on_completion(status);
    if (status.ok()) {
      for (auto& table : response.tables()) {
        result.emplace_back(std::move(table));
      }
      if (response.next_page_token().empty()) {
        break;
      }
      page_token = std::move(*response.mutable_next_page_token());
      continue;
    }
    page_token = std::move(*request.mutable_page_token());
    if (not rpc_policy->on_failure(status)) {
      RaiseError(status, "ListTables()");
    }
    auto delay = backoff_policy->on_completion(status);
    std::this_thread::sleep_for(delay);
  }
  return result;
}

::google::bigtable::admin::v2::Table TableAdmin::GetTable(
    std::string table_id, ::google::bigtable::admin::v2::Table::View view) {
  btproto::GetTableRequest request;
  request.set_name(absl::StrCat(instance_name(), "/tables/", table_id));
  request.set_view(view);

  return call_with_retry(&btproto::BigtableTableAdmin::StubInterface::GetTable,
                         request,
                         absl::StrCat("GetTable(", request.name(), ")"));
}

std::string TableAdmin::CreateInstanceName() const {
  return absl::StrCat("projects/", client_->project(), "/instances/",
                      instance_id_);
}

void TableAdmin::RaiseError(grpc::Status const& status,
                            absl::string_view error_message) const {
  std::ostringstream os;
  os << "TableAdmin(" << instance_name() << ") unrecoverable error or too many "
     << " errors in " << error_message << ": " << status.error_message() << " ["
     << status.error_code() << "] " << status.error_details();
  // TODO(#35) - implement non-throwing version of this class.
  throw std::runtime_error(os.str());
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
