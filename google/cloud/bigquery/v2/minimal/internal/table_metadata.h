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

// Internal interface for Bigquery V2 Job resource.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_TABLE_METADATA_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_TABLE_METADATA_H

#include "google/cloud/bigquery/v2/minimal/internal/table_rest_stub.h"
#include "google/cloud/internal/rest_context.h"
#include "google/cloud/version.h"

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class TableMetadata : public TableRestStub {
 public:
  ~TableMetadata() override = default;
  explicit TableMetadata(std::shared_ptr<TableRestStub> child);

  StatusOr<GetTableResponse> GetTable(rest_internal::RestContext& context,
                                      GetTableRequest const& request) override;
  StatusOr<ListTablesResponse> ListTables(
      rest_internal::RestContext& context,
      ListTablesRequest const& request) override;

 private:
  void SetMetadata(rest_internal::RestContext& context,
                   std::vector<std::string> const& params = {});

  std::shared_ptr<TableRestStub> child_;
  std::string api_client_header_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_TABLE_METADATA_H
