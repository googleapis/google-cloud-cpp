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
#include "google/cloud/bigquerycontrol/v2/job_connection_idempotency_policy.h"
#include "google/cloud/bigquerycontrol/v2/job_options.h"
#include "google/cloud/common_options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/example_driver.h"
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

// clang-format off
// main-dox-marker: bigquerycontrol_v2::JobServiceClient
// clang-format on
namespace {

auto constexpr kDefaultQuery =
    "SELECT name, state, year, sum(number) as total "
    "FROM `bigquery-public-data.usa_names.usa_1910_2013` "
    "WHERE year >= 1970 "
    "GROUP BY name, state, year "
    "ORDER by total DESC "
    "LIMIT 100";

void ExecuteQueryJob(std::vector<std::string> const& argv) {
  if (argv.size() != 2 || argv[0] == "--help") {
    throw google::cloud::testing_util::Usage{
        "execute-query-job <billing_project> <query> [<retry_delay>]"};
  }
  int retry_delay = 2;
  if (argv.size() == 3) retry_delay = std::stoi(argv[2]);

  //! [execute-query-job]
  [](std::string const& billing_project, std::string const& query,
     int retry_delay) {
    auto client = google::cloud::bigquerycontrol_v2::JobServiceClient(
        google::cloud::bigquerycontrol_v2::MakeJobServiceConnectionRest());

    google::cloud::bigquery::v2::JobConfigurationQuery query_config;
    query_config.set_query(query);
    // Using GoogleSQL allows instance and dataset specification in the FROM
    // clause.
    query_config.mutable_use_legacy_sql()->set_value(false);

    google::cloud::bigquery::v2::JobConfiguration config;
    *config.mutable_query() = query_config;
    config.mutable_labels()->emplace("type", "sample");
    google::cloud::bigquery::v2::Job job;
    *job.mutable_configuration() = config;
    google::cloud::bigquery::v2::InsertJobRequest job_request;
    job_request.set_project_id(billing_project);
    *job_request.mutable_job() = job;

    // Submit the Query Job.
    auto insert_result = client.InsertJob(job_request);
    if (!insert_result) throw insert_result.status();

    auto job_id = insert_result->job_reference().job_id();
    // If not immediately finished, poll job status until DONE.
    if (insert_result->status().state() != "DONE") {
      google::cloud::bigquery::v2::GetJobRequest get_request;
      get_request.set_project_id(billing_project);
      get_request.set_job_id(job_id);

      bool job_complete = false;
      std::vector<int> retries{retry_delay, retry_delay, retry_delay,
                               retry_delay, retry_delay};
      assert(retries.size() == 5);
      for (auto delay : retries) {
        auto get_result = client.GetJob(get_request);
        if (get_result->status().state() == "DONE") {
          job_complete = true;
          break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(delay));
      }
      if (!job_complete) throw "Query execution timed out after 5 retries.";
    }

    // Read results.
    google::cloud::bigquery::v2::GetQueryResultsRequest query_results_request;
    query_results_request.set_project_id(billing_project);
    query_results_request.set_job_id(job_id);
    query_results_request.mutable_max_results()->set_value(10);

    std::cout << query_results_request.DebugString() << "\n";

    auto query_results = client.GetQueryResults(query_results_request);
    if (!query_results) throw query_results.status();

    int num_pages = 0;
    if (query_results->job_complete().value()) {
//      std::cout << "Total rows: " << query_results->total_rows().value()
//                << "\n";
//      std::cout << "Result schema: " << query_results->schema().DebugString()
//                << "\n";
//      std::cout << "Result rows:\n";

      do {
        ++num_pages;
        //        for (google::protobuf::Struct const& row :
        //        query_results->rows()) {
        //          std::cout << row.DebugString() << "\n";
        //        }

        if (!query_results->page_token().empty()) {
          std::cout << "page_token: " << query_results->page_token() << "\n";
          query_results_request.set_page_token(query_results->page_token());
          query_results = client.GetQueryResults(query_results_request);
          if (!query_results) throw query_results.status();
        }
      } while (!query_results->page_token().empty());

      std::cout << "num_pages: " << num_pages << "\n";
    }
  }
  //! [execute-query-job]
  (argv.at(0), argv.at(1), retry_delay);
}

void ExecuteQuery(std::vector<std::string> const&) {}

void AutoRun(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;
  using ::google::cloud::internal::GetEnv;
  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({"GOOGLE_CLOUD_PROJECT"});
  auto const project = GetEnv("GOOGLE_CLOUD_PROJECT").value();

  std::cout << "\nRunning ExecuteQueryJob() example" << std::endl;
  ExecuteQueryJob({project, kDefaultQuery});

  std::cout << "\nRunning ExecuteQuery() example" << std::endl;
  ExecuteQuery({project, kDefaultQuery});
}

}  // namespace

int main(int argc, char* argv[]) {  // NOLINT(bugprone-exception-escape)
  google::cloud::testing_util::Example example({
      {"execute-query-job", ExecuteQueryJob},
      {"execute-query", ExecuteQuery},
      {"auto", AutoRun},
  });
  return example.Run(argc, argv);
}
