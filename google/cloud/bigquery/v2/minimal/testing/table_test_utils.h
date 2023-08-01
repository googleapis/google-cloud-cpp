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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_TESTING_TABLE_TEST_UTILS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_TESTING_TABLE_TEST_UTILS_H

#include "google/cloud/bigquery/v2/minimal/internal/table.h"
#include "google/cloud/bigquery/v2/minimal/internal/table_request.h"
#include "google/cloud/log.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_testing {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

bigquery_v2_minimal_internal::ListFormatTable MakeListFormatTable();
bigquery_v2_minimal_internal::Table MakeTable();
void AssertEquals(bigquery_v2_minimal_internal::Table const& lhs,
                  bigquery_v2_minimal_internal::Table const& rhs);
void AssertEquals(bigquery_v2_minimal_internal::ListFormatTable const& lhs,
                  bigquery_v2_minimal_internal::ListFormatTable const& rhs);
std::string MakeTableJsonText();
std::string MakeListFormatTableJsonText();
std::string MakeListTablesResponseJsonText();
std::string MakeListTablesResponseNoPageTokenJsonText();

bigquery_v2_minimal_internal::GetTableRequest MakeGetTableRequest();
bigquery_v2_minimal_internal::ListTablesRequest MakeListTablesRequest();

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_TESTING_TABLE_TEST_UTILS_H
