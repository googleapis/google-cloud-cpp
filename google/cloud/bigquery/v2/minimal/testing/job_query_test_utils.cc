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

#include "google/cloud/bigquery/v2/minimal/testing/job_query_test_utils.h"
#include "google/cloud/bigquery/v2/minimal/internal/common_v2_resources.h"
#include "google/cloud/bigquery/v2/minimal/testing/common_v2_test_utils.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <chrono>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_testing {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::bigquery_v2_minimal_internal::ConnectionProperty;
using ::google::cloud::bigquery_v2_minimal_internal::DataFormatOptions;
using ::google::cloud::bigquery_v2_minimal_internal::PostQueryRequest;
using ::google::cloud::bigquery_v2_minimal_internal::QueryParameter;
using ::google::cloud::bigquery_v2_minimal_internal::QueryRequest;

using ::google::cloud::bigquery_v2_minimal_testing::MakeConnectionProperty;
using ::google::cloud::bigquery_v2_minimal_testing::MakeDatasetReference;
using ::google::cloud::bigquery_v2_minimal_testing::MakeQueryParameter;

QueryRequest MakeQueryRequest() {
  QueryRequest expected;
  expected.set_query("select 1;")
      .set_kind("query-kind")
      .set_parameter_mode("parameter-mode")
      .set_location("useast1")
      .set_request_id("1234")
      .set_dry_run(true)
      .set_preserve_nulls(true)
      .set_use_query_cache(true)
      .set_use_legacy_sql(true)
      .set_create_session(true)
      .set_max_results(10)
      .set_maximum_bytes_billed(100000)
      .set_timeout(std::chrono ::milliseconds(10));

  std::vector<ConnectionProperty> props;
  props.push_back(MakeConnectionProperty());

  std::vector<QueryParameter> qp;
  qp.push_back(MakeQueryParameter());

  std::map<std::string, std::string> labels;
  labels.insert({"lk1", "lv1"});
  labels.insert({"lk2", "lv2"});

  DataFormatOptions dfo;
  dfo.use_int64_timestamp = true;

  expected.set_connection_properties(props)
      .set_query_parameters(qp)
      .set_labels(labels)
      .set_format_options(dfo)
      .set_default_dataset(MakeDatasetReference());

  return expected;
}

PostQueryRequest MakePostQueryRequest() {
  PostQueryRequest expected;
  expected.set_project_id("test-project-id")
      .set_query_request(MakeQueryRequest());

  return expected;
}

void AssertEquals(QueryRequest const& lhs, QueryRequest const& rhs) {
  EXPECT_EQ(lhs.query(), rhs.query());
  EXPECT_EQ(lhs.kind(), rhs.kind());
  EXPECT_EQ(lhs.parameter_mode(), rhs.parameter_mode());
  EXPECT_EQ(lhs.location(), rhs.location());
  EXPECT_EQ(lhs.request_id(), rhs.request_id());

  EXPECT_EQ(lhs.dry_run(), rhs.dry_run());
  EXPECT_EQ(lhs.preserve_nulls(), rhs.preserve_nulls());
  EXPECT_EQ(lhs.use_query_cache(), rhs.use_query_cache());
  EXPECT_EQ(lhs.use_legacy_sql(), rhs.use_legacy_sql());
  EXPECT_EQ(lhs.create_session(), rhs.create_session());

  EXPECT_EQ(lhs.max_results(), rhs.max_results());
  EXPECT_EQ(lhs.maximum_bytes_billed(), rhs.maximum_bytes_billed());
  EXPECT_EQ(lhs.timeout(), rhs.timeout());

  EXPECT_TRUE(std::equal(lhs.connection_properties().begin(),
                         lhs.connection_properties().end(),
                         rhs.connection_properties().begin()));
  EXPECT_TRUE(std::equal(lhs.query_parameters().begin(),
                         lhs.query_parameters().end(),
                         rhs.query_parameters().begin()));
  EXPECT_TRUE(std::equal(lhs.labels().begin(), lhs.labels().end(),
                         rhs.labels().begin()));

  EXPECT_EQ(lhs.default_dataset().dataset_id, rhs.default_dataset().dataset_id);
  EXPECT_EQ(lhs.default_dataset().project_id, rhs.default_dataset().project_id);
  EXPECT_EQ(lhs.format_options().use_int64_timestamp,
            rhs.format_options().use_int64_timestamp);
}

void AssertEquals(bigquery_v2_minimal_internal::PostQueryRequest const& lhs,
                  bigquery_v2_minimal_internal::PostQueryRequest const& rhs) {
  EXPECT_EQ(lhs.project_id(), rhs.project_id());
  AssertEquals(lhs.query_request(), rhs.query_request());
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_testing
}  // namespace cloud
}  // namespace google
