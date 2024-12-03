// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/emulator/cluster.h"
#include "google/cloud/internal/make_status.h"
#include "absl/strings/match.h"

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {
namespace{

namespace btadmin = google::bigtable::admin::v2;

StatusOr<btadmin::Table> ApplyView(std::string const& table_name,
                                   Table const &table,
                                   btadmin::Table_View view,
                                   btadmin::Table_View default_view) {
  if (view == btadmin::Table::VIEW_UNSPECIFIED) {
    view = default_view;
  }
  switch (view) {
    case btadmin::Table::VIEW_UNSPECIFIED:
      return internal::InternalError(
          "VIEW_UNSPECIFIED cannot be the default view");
    case btadmin::Table::NAME_ONLY: {
      btadmin::Table res;
      res.set_name(table_name);
      return res;
    }
    case btadmin::Table::SCHEMA_VIEW: {
      btadmin::Table res;
      res.set_name(table_name);
      auto before_view = table.GetSchema();
      *res.mutable_column_families() =
          std::move(*before_view.mutable_column_families());
      res.set_granularity(before_view.granularity());
      return res;
    }
    case btadmin::Table::REPLICATION_VIEW:
    case btadmin::Table::ENCRYPTION_VIEW: {
      btadmin::Table res;
      res.set_name(table_name);
      auto before_view = table.GetSchema();
      *res.mutable_cluster_states() =
          std::move(*before_view.mutable_cluster_states());
      return res;
    }
    case btadmin::Table::FULL:
      return table.GetSchema();
    default:
      return internal::UnimplementedError(
          "Unsupported view.",
          GCP_ERROR_INFO().WithMetadata("view", Table_View_Name(view)));
  }
}

}  // anonymous namespace

StatusOr<btadmin::Table> Cluster::CreateTable(std::string const& table_name,
                                              btadmin::Table schema) {
  schema.set_name(table_name);
  std::cout << "Creating table " << table_name << std::endl;
  auto to_insert = std::make_shared<Table>();
  auto status = to_insert->Construct(std::move(schema));
  if (!status.ok()) {
    return status;
  }
  {
    std::lock_guard lock(mu_);
    if (!table_by_name_.emplace(table_name, to_insert).second) {
      return internal::AlreadyExistsError(
          "Table already exists.",
          GCP_ERROR_INFO().WithMetadata("table_name", table_name));
    }
  }
  return to_insert->GetSchema();
}

StatusOr<std::vector<btadmin::Table>> Cluster::ListTables(
    std::string const& instance_name, btadmin::Table_View view) const {
  std::map<std::string, std::shared_ptr<Table>> table_by_name_copy;
  {
    std::lock_guard lock(mu_);
    table_by_name_copy = table_by_name_;
  }
  std::vector<btadmin::Table> res;
  std::string const prefix = instance_name + "/tables/";
  std::cout << "Listing tables with prefix " << prefix << std::endl;
  for (auto name_and_table_it = table_by_name_copy.upper_bound(prefix);
       name_and_table_it != table_by_name_copy.end() &&
       absl::StartsWith(name_and_table_it->first, prefix);
       ++name_and_table_it) {
    auto maybe_view =
        ApplyView(name_and_table_it->first, *name_and_table_it->second, view,
                  btadmin::Table::NAME_ONLY);
    if (!maybe_view) {
      return maybe_view.status();
    }
    res.emplace_back(*maybe_view);
  }
  return res;
}

StatusOr<btadmin::Table> Cluster::GetTable(std::string const& table_name,
                                           btadmin::Table_View view) const {
  std::shared_ptr<Table> found_table;
  {
    std::lock_guard lock(mu_);
    auto it = table_by_name_.find(table_name);
    if (it == table_by_name_.end()) {
      return NotFoundError("No such table.", GCP_ERROR_INFO().WithMetadata(
                                                 "table_name", table_name));
    }
    found_table = it->second;
  }
  return ApplyView(table_name, *found_table, view, btadmin::Table::SCHEMA_VIEW);
}

Status Cluster::DeleteTable(std::string const& table_name) {
  {
    std::lock_guard lock(mu_);
    auto it = table_by_name_.find(table_name);
    if (it == table_by_name_.end()) {
      return NotFoundError(
          "No such table.",
          GCP_ERROR_INFO().WithMetadata("table_name", table_name));
    }
    if (it->second->IsDeleteProtected()) {
      return FailedPreconditionError(
          "The table has deletion protection.",
          GCP_ERROR_INFO().WithMetadata("table_name", table_name));
    }
    table_by_name_.erase(it);
  }
  return Status();
}

Status Cluster::UpdateTable(btadmin::Table const& new_schema,
                            google::protobuf::FieldMask const& to_update) {
  auto maybe_table = FindTable(new_schema.name());
  if (!maybe_table) {
    return maybe_table.status();
  }
  (*maybe_table)->Update(new_schema, to_update);
  return Status();
}

StatusOr<google::bigtable::admin::v2::Table> Cluster::ModifyColumnFamilies(
    btadmin::ModifyColumnFamiliesRequest const& request) {
  auto maybe_table = FindTable(request.name());
  if (!maybe_table) {
    return maybe_table.status();
  }
  return (*maybe_table)->ModifyColumnFamilies(request);
}


bool Cluster::HasTable(std::string const& table_name) const {
  std::lock_guard lock(mu_);
  return table_by_name_.find(table_name) != table_by_name_.end();
}

StatusOr<std::shared_ptr<Table>> Cluster::FindTable(
    std::string const& table_name) {
  {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = table_by_name_.find(table_name);
    if (it == table_by_name_.end()) {
      return NotFoundError(
          "No such table.",
          GCP_ERROR_INFO().WithMetadata("table_name", table_name));
    }
    return it->second;
  }
}

}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
