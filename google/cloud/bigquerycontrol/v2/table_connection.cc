// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Generated by the Codegen C++ plugin.
// If you make any local changes, they will be lost.
// source: google/cloud/bigquery/v2/table.proto

#include "google/cloud/bigquerycontrol/v2/table_connection.h"
#include "google/cloud/bigquerycontrol/v2/internal/table_option_defaults.h"
#include "google/cloud/bigquerycontrol/v2/internal/table_tracing_connection.h"
#include "google/cloud/bigquerycontrol/v2/table_options.h"
#include "google/cloud/background_threads.h"
#include "google/cloud/common_options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/pagination_range.h"
#include "google/cloud/internal/unified_grpc_credentials.h"
#include <memory>
#include <utility>

namespace google {
namespace cloud {
namespace bigquerycontrol_v2 {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

TableServiceConnection::~TableServiceConnection() = default;

StatusOr<google::cloud::bigquery::v2::Table> TableServiceConnection::GetTable(
    google::cloud::bigquery::v2::GetTableRequest const&) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StatusOr<google::cloud::bigquery::v2::Table>
TableServiceConnection::InsertTable(
    google::cloud::bigquery::v2::InsertTableRequest const&) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StatusOr<google::cloud::bigquery::v2::Table> TableServiceConnection::PatchTable(
    google::cloud::bigquery::v2::UpdateOrPatchTableRequest const&) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StatusOr<google::cloud::bigquery::v2::Table>
TableServiceConnection::UpdateTable(
    google::cloud::bigquery::v2::UpdateOrPatchTableRequest const&) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

Status TableServiceConnection::DeleteTable(
    google::cloud::bigquery::v2::DeleteTableRequest const&) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StreamRange<google::cloud::bigquery::v2::ListFormatTable>
TableServiceConnection::ListTables(
    google::cloud::bigquery::v2::
        ListTablesRequest) {  // NOLINT(performance-unnecessary-value-param)
  return google::cloud::internal::MakeUnimplementedPaginationRange<
      StreamRange<google::cloud::bigquery::v2::ListFormatTable>>();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquerycontrol_v2
}  // namespace cloud
}  // namespace google
