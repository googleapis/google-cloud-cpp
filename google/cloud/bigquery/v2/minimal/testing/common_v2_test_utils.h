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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_TESTING_COMMON_V2_TEST_UTILS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_TESTING_COMMON_V2_TEST_UTILS_H

#include "google/cloud/bigquery/v2/minimal/internal/common_v2_resources.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/rest_context.h"

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_testing {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

bigquery_v2_minimal_internal::QueryParameterType MakeQueryParameterType();
bigquery_v2_minimal_internal::QueryParameterValue MakeQueryParameterValue();
bigquery_v2_minimal_internal::QueryParameter MakeQueryParameter();
bigquery_v2_minimal_internal::SystemVariables MakeSystemVariables();
bigquery_v2_minimal_internal::DatasetReference MakeDatasetReference();
bigquery_v2_minimal_internal::ConnectionProperty MakeConnectionProperty();
bigquery_v2_minimal_internal::RowData MakeRowData();

void AssertParamValueEquals(
    bigquery_v2_minimal_internal::QueryParameterValue& expected,
    bigquery_v2_minimal_internal::QueryParameterValue& actual);
void AssertParamTypeEquals(
    bigquery_v2_minimal_internal::QueryParameterType& expected,
    bigquery_v2_minimal_internal::QueryParameterType& actual);
void AssertEquals(bigquery_v2_minimal_internal::SystemVariables& expected,
                  bigquery_v2_minimal_internal::SystemVariables& actual);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_TESTING_COMMON_V2_TEST_UTILS_H
