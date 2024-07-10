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

#include "google/cloud/bigquerycontrol/v2/job_client.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/integration_test.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <chrono>
#include <thread>

namespace google {
namespace cloud {
namespace bigquerycontrol_v2 {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::AnyOf;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;

class BigQueryJobIntegrationTest
    : public ::google::cloud::testing_util::IntegrationTest {
 protected:
  void SetUp() override {
    project_id_ =
        google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
    ASSERT_FALSE(project_id_.empty());
  }
  std::string project_id_;
};

TEST_F(BigQueryJobIntegrationTest, JobCRUD) {
  namespace bigquery_proto = google::cloud::bigquery::v2;
  auto client = JobServiceClient(MakeJobServiceConnectionRest());

  bigquery_proto::JobConfigurationQuery query;
  query.set_query(
      "SELECT name, state, year, sum(number) as total "
      "FROM `bigquery-public-data.usa_names.usa_1910_2013` "
      "WHERE year >= @minimum_year "
      "GROUP BY name, state, year");

  // Use GoogleSQL dialect to enable parameterized queries.
  auto use_legacy_sql = google::protobuf::BoolValue();
  use_legacy_sql.set_value(false);
  *query.mutable_use_legacy_sql() = use_legacy_sql;

  // Specify value for named integer parameter: @minimum_year
  query.set_parameter_mode("NAMED");
  bigquery_proto::QueryParameter minimum_year_param;
  minimum_year_param.set_name("minimum_year");
  bigquery_proto::QueryParameterType minimum_year_param_type;
  minimum_year_param_type.set_type("INT64");
  *minimum_year_param.mutable_parameter_type() = minimum_year_param_type;
  bigquery_proto::QueryParameterValue minimum_year_param_value;
  auto year_value = google::protobuf::StringValue();
  year_value.set_value("1970");
  *minimum_year_param_value.mutable_value() = year_value;
  *minimum_year_param.mutable_parameter_value() = minimum_year_param_value;
  *query.add_query_parameters() = minimum_year_param;

  bigquery_proto::JobConfiguration config;
  *config.mutable_query() = query;
  bigquery_proto::Job job;
  *job.mutable_configuration() = config;
  bigquery_proto::InsertJobRequest job_request;
  job_request.set_project_id(project_id_);
  *job_request.mutable_job() = job;

  StatusOr<bigquery_proto::Job> insert_result = client.InsertJob(job_request);
  ASSERT_STATUS_OK(insert_result);
  std::string job_id = insert_result->job_reference().job_id();
  EXPECT_THAT(job_id, Not(IsEmpty()));
  EXPECT_THAT(
      insert_result->configuration().query().destination_table().project_id(),
      Not(IsEmpty()));
  EXPECT_THAT(
      insert_result->configuration().query().destination_table().dataset_id(),
      Not(IsEmpty()));
  EXPECT_THAT(
      insert_result->configuration().query().destination_table().table_id(),
      Not(IsEmpty()));

  google::cloud::bigquery::v2::ListJobsRequest list_request;
  list_request.set_project_id(project_id_);
  std::vector<bigquery_proto::JobReference> job_refs;
  std::vector<std::string> job_ids;
  for (auto job : client.ListJobs(list_request)) {
    ASSERT_STATUS_OK(job);
    job_refs.push_back(job->job_reference());
    job_ids.push_back(job->job_reference().job_id());
  }
  EXPECT_THAT(job_ids, Contains(job_id));

  google::cloud::bigquery::v2::GetJobRequest get_request;
  get_request.set_project_id(project_id_);
  get_request.set_job_id(job_id);
  bool job_complete = false;
  while (!job_complete) {
    auto get_result = client.GetJob(get_request);
    ASSERT_STATUS_OK(get_result);
    EXPECT_THAT(get_result->job_reference().job_id(), Eq(job_id));
    if (get_result->status().state() == "DONE") job_complete = true;
    std::this_thread::sleep_for(std::chrono::seconds(2));
  }

  google::cloud::bigquery::v2::DeleteJobRequest delete_request;
  delete_request.set_project_id(project_id_);
  delete_request.set_job_id(job_id);
  auto delete_result = client.DeleteJob(delete_request);
  EXPECT_STATUS_OK(delete_result);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquerycontrol_v2
}  // namespace cloud
}  // namespace google
