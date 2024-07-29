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

#include "google/cloud/bigquerycontrol/v2/job_client.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/example_driver.h"
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

auto constexpr kDefaultQuery =
    "SELECT name, state, year, sum(number) as total "
    "FROM `bigquery-public-data.usa_names.usa_1910_2013` "
    "WHERE year >= 1970 "
    "GROUP BY name, state, year "
    "ORDER by total DESC "
    "LIMIT 10";

void ExecuteQueryJob(std::vector<std::string> const& argv) {
  if (argv.size() != 2 || argv[0] == "--help") {
    throw google::cloud::testing_util::Usage{
        "execute-query-job <billing_project> <query> [<backoff>]"};
  }

  auto const backoff =
      std::chrono::seconds(argv.size() == 3 ? std::stoi(argv[2]) : 2);
  //! [execute-query-job]
  namespace bigquery_v2_proto = google::cloud::bigquery::v2;
  [](std::string const& billing_project, std::string const& query,
     std::chrono::seconds backoff) {
    auto client = google::cloud::bigquerycontrol_v2::JobServiceClient(
        google::cloud::bigquerycontrol_v2::MakeJobServiceConnectionRest());

    bigquery_v2_proto::JobConfigurationQuery query_config;
    // Using GoogleSQL allows instance and dataset specification in the FROM
    // clause as part of the table name.
    query_config.mutable_use_legacy_sql()->set_value(false);
    query_config.set_query(query);

    bigquery_v2_proto::JobConfiguration config;
    *config.mutable_query() = query_config;
    config.mutable_labels()->emplace("type", "sample");
    bigquery_v2_proto::Job job;
    *job.mutable_configuration() = config;
    bigquery_v2_proto::InsertJobRequest job_request;
    job_request.set_project_id(billing_project);
    *job_request.mutable_job() = job;

    // Submit the Query Job.
    auto insert_result = client.InsertJob(job_request);
    if (!insert_result) throw std::move(insert_result).status();

    auto job_id = insert_result->job_reference().job_id();
    // If not immediately finished, poll job status until DONE.
    if (insert_result->status().state() != "DONE") {
      bigquery_v2_proto::GetJobRequest get_request;
      get_request.set_project_id(billing_project);
      get_request.set_job_id(job_id);

      bool job_complete = false;
      for (auto b : {backoff, backoff, backoff, backoff, backoff}) {
        auto get_result = client.GetJob(get_request);
        if (get_result->status().state() == "DONE") {
          job_complete = true;
          break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(b));
      }
      if (!job_complete)
        throw std::runtime_error("Query execution timed out after 5 retries.");
    }

    // Read query results using this library. This rpc returns the result rows
    // as google.protobuf.Struct types and uses REST/JSON as the transport. For
    // a faster, more efficient, mechanism to retrieve query results, the
    // BigQuery Storage Read service should be used to read from the destination
    // table
    // (https://github.com/GoogleCloudPlatform/cpp-samples/tree/main/bigquery/read).
    bigquery_v2_proto::GetQueryResultsRequest query_results_request;
    query_results_request.set_project_id(billing_project);
    query_results_request.set_job_id(job_id);
    // This restricts the number of results per page to a ridiculously small
    // value so that we can demonstrate paging.
    query_results_request.mutable_max_results()->set_value(5);

    int num_pages = 0;
    bool finished = false;
    while (!finished) {
      auto query_results = client.GetQueryResults(query_results_request);
      if (!query_results) throw std::move(query_results).status();

      // Only print summary once.
      if (num_pages == 0) {
        std::cout << "Total rows: " << query_results->total_rows().value()
                  << "\n";
        std::cout << "Result schema: " << query_results->schema().DebugString()
                  << "\n";
        std::cout << "Result rows:\n";
      }

      std::cout << "Page: " << std::to_string(num_pages++) << "\n";
      for (google::protobuf::Struct const& row : query_results->rows()) {
        std::cout << row.DebugString() << "\n";
      }

      if (!query_results->page_token().empty()) {
        query_results_request.set_page_token(query_results->page_token());
        query_results = client.GetQueryResults(query_results_request);
        if (!query_results) throw std::move(query_results).status();
      } else {
        finished = true;
      }
    }
  }
  //! [execute-query-job]
  (argv.at(0), argv.at(1), backoff);
}

void AutoRun(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;
  using ::google::cloud::internal::GetEnv;
  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({"GOOGLE_CLOUD_PROJECT"});
  auto const project = GetEnv("GOOGLE_CLOUD_PROJECT").value();

  std::cout << "\nRunning ExecuteQueryJob() example" << std::endl;
  ExecuteQueryJob({project, kDefaultQuery});
}

}  // namespace

int main(int argc, char* argv[]) {  // NOLINT(bugprone-exception-escape)
  google::cloud::testing_util::Example example({
      {"execute-query-job", ExecuteQueryJob},
      {"auto", AutoRun},
  });
  return example.Run(argc, argv);
}
