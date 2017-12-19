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

#include <thread>

#include <absl/strings/str_cat.h>

namespace btproto = ::google::bigtable::admin::v2;

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
::google::bigtable::admin::v2::Table TableAdmin::CreateTable(
    std::string table_id, std::map<std::string, GcRule> column_families,
    std::vector<std::string> splits,
    ::google::bigtable::admin::v2::Table::TimestampGranularity granularity) {
  // Copy the policies in effect for the operation.
  auto rpc_policy = rpc_retry_policy_->clone();
  auto backoff_policy = rpc_backoff_policy_->clone();

  btproto::CreateTableRequest request;
  request.set_parent(instance_name());
  request.set_table_id(std::move(table_id));
  auto& table = *request.mutable_table();
  table.set_granularity(granularity);
  auto& families = *table.mutable_column_families();
  for (auto& kv : column_families) {
    *families[kv.first].mutable_gc_rule() = kv.second.as_proto_move();
  }
  for (auto& split : splits) {
    request.add_initial_splits()->set_key(std::move(split));
  }

  btproto::Table response;
  while (true) {
    grpc::ClientContext client_context;
    rpc_policy->setup(client_context);
    backoff_policy->setup(client_context);
    grpc::Status status =
        client_->table_admin().CreateTable(&client_context, request, &response);
    client_->on_completion(status);
    if (status.ok()) {
      break;
    }
    if (not rpc_policy->on_failure(status)) {
      throw std::runtime_error("could not create table");
    }
    auto delay = backoff_policy->on_completion(status);
    std::this_thread::sleep_for(delay);
  }
  return response;
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
      // TODO(#35) - implement non-throwing version of this class.
      throw std::runtime_error("could not fetch all tables");
    }
    auto delay = backoff_policy->on_completion(status);
    std::this_thread::sleep_for(delay);
  }
  return result;
}

std::string TableAdmin::CreateInstanceName() const {
  return absl::StrCat("projects/", client_->project(), "/instances/",
                      instance_id_);
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
