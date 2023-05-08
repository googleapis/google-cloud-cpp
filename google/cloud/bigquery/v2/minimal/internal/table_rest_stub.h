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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_TABLE_REST_STUB_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_TABLE_REST_STUB_H

#include "google/cloud/bigquery/v2/minimal/internal/table_request.h"
#include "google/cloud/bigquery/v2/minimal/internal/table_response.h"
#include "google/cloud/internal/rest_client.h"
#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <memory>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class TableRestStub {
 public:
  virtual ~TableRestStub() = 0;

  virtual StatusOr<GetTableResponse> GetTable(
      rest_internal::RestContext& rest_context,
      GetTableRequest const& request) = 0;
  virtual StatusOr<ListTablesResponse> ListTables(
      rest_internal::RestContext& rest_context,
      ListTablesRequest const& request) = 0;
};

class DefaultTableRestStub : public TableRestStub {
 public:
  explicit DefaultTableRestStub(
      std::unique_ptr<rest_internal::RestClient> rest_stub)
      : rest_stub_(std::move(rest_stub)) {}

  StatusOr<GetTableResponse> GetTable(rest_internal::RestContext& rest_context,
                                      GetTableRequest const& request) override;
  StatusOr<ListTablesResponse> ListTables(
      rest_internal::RestContext& rest_context,
      ListTablesRequest const& request) override;

 private:
  std::unique_ptr<rest_internal::RestClient> rest_stub_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_TABLE_REST_STUB_H
