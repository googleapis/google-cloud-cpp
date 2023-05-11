// Copyright 2023 Google LLC
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

#include "google/cloud/bigquery/v2/minimal/internal/table_connection.h"
#include "google/cloud/bigquery/v2/minimal/internal/table_rest_connection_impl.h"
#include "google/cloud/bigquery/v2/minimal/internal/table_rest_stub_factory.h"
#include "google/cloud/common_options.h"
#include "google/cloud/credentials.h"
#include <memory>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

TableConnection::~TableConnection() = default;

StatusOr<Table> TableConnection::GetTable(GetTableRequest const&) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StreamRange<ListFormatTable> TableConnection::ListTables(
    ListTablesRequest const&) {
  return google::cloud::internal::MakeStreamRange<ListFormatTable>(
      []() -> absl::variant<Status, ListFormatTable> {
        return Status(StatusCode::kUnimplemented, "not implemented");
      });
}

std::shared_ptr<TableConnection> MakeTableConnection(Options options) {
  internal::CheckExpectedOptions<CommonOptionList, UnifiedCredentialsOptionList,
                                 TablePolicyOptionList>(options, __func__);
  options = TableDefaultOptions(std::move(options));

  auto table_rest_stub = CreateDefaultTableRestStub(options);

  return std::make_shared<TableRestConnectionImpl>(std::move(table_rest_stub),
                                                   std::move(options));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
