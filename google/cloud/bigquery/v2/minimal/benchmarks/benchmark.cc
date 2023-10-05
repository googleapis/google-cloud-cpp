// Copyright 2023 Google LLC
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

#include "google/cloud/bigquery/v2/minimal/benchmarks/benchmark.h"
#include "google/cloud/bigquery/v2/minimal/internal/common_v2_resources.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/options.h"
#include "google/cloud/stream_range.h"
#include "absl/time/time.h"
#include <chrono>
#include <future>
#include <iomanip>
#include <sstream>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_benchmarks {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::bigquery_v2_minimal_internal::CancelJobRequest;
using ::google::cloud::bigquery_v2_minimal_internal::Dataset;
using ::google::cloud::bigquery_v2_minimal_internal::DatasetClient;
using ::google::cloud::bigquery_v2_minimal_internal::
    DatasetConnectionPoolSizeOption;
using ::google::cloud::bigquery_v2_minimal_internal::EvaluationKind;
using ::google::cloud::bigquery_v2_minimal_internal::GetDatasetRequest;
using ::google::cloud::bigquery_v2_minimal_internal::GetJobRequest;
using ::google::cloud::bigquery_v2_minimal_internal::GetQueryResults;
using ::google::cloud::bigquery_v2_minimal_internal::GetQueryResultsRequest;
using ::google::cloud::bigquery_v2_minimal_internal::GetTableRequest;
using ::google::cloud::bigquery_v2_minimal_internal::IndexUsageMode;
using ::google::cloud::bigquery_v2_minimal_internal::InsertJobRequest;
using ::google::cloud::bigquery_v2_minimal_internal::Job;
using ::google::cloud::bigquery_v2_minimal_internal::JobClient;
using ::google::cloud::bigquery_v2_minimal_internal::KeyResultStatementKind;
using ::google::cloud::bigquery_v2_minimal_internal::ListDatasetsRequest;
using ::google::cloud::bigquery_v2_minimal_internal::ListFormatDataset;
using ::google::cloud::bigquery_v2_minimal_internal::ListFormatJob;
using ::google::cloud::bigquery_v2_minimal_internal::ListFormatTable;
using ::google::cloud::bigquery_v2_minimal_internal::ListJobsRequest;
using ::google::cloud::bigquery_v2_minimal_internal::ListProjectsRequest;
using ::google::cloud::bigquery_v2_minimal_internal::ListTablesRequest;
using ::google::cloud::bigquery_v2_minimal_internal::MakeBigQueryJobConnection;
using ::google::cloud::bigquery_v2_minimal_internal::MakeDatasetConnection;
using ::google::cloud::bigquery_v2_minimal_internal::MakeProjectConnection;
using ::google::cloud::bigquery_v2_minimal_internal::MakeTableConnection;
using ::google::cloud::bigquery_v2_minimal_internal::PostQueryRequest;
using ::google::cloud::bigquery_v2_minimal_internal::PostQueryResults;
using ::google::cloud::bigquery_v2_minimal_internal::Project;
using ::google::cloud::bigquery_v2_minimal_internal::ProjectClient;
using ::google::cloud::bigquery_v2_minimal_internal::QueryRequest;
using ::google::cloud::bigquery_v2_minimal_internal::Table;
using ::google::cloud::bigquery_v2_minimal_internal::TableClient;
using ::google::cloud::internal::MakeStreamRange;

namespace {
double const kResultPercentiles[] = {0, 50, 90, 95, 99, 99.9, 100};

std::vector<std::string> StrSplit(std::string const& str, char delimiter) {
  std::stringstream ss(str);
  std::vector<std::string> res;
  std::string token;
  while (std::getline(ss, token, delimiter)) {
    res.push_back(token);
  }
  return res;
}

std::chrono::system_clock::time_point StrToTimepoint(std::string const& str) {
  auto millis = std::stoi(str);
  std::chrono::system_clock::time_point tp =
      std::chrono::system_clock::from_time_t(0) +
      std::chrono::milliseconds(millis);
  return tp;
}

std::chrono::milliseconds ToChronoMillis(int m) {
  return std::chrono::milliseconds(m);
}

std::string GenerateJobId(std::string const& prefix = "") {
  auto constexpr kJobPrefix = "bqOdbcJob_benchmark_test_";
  std::string job_prefix = kJobPrefix;
  if (!prefix.empty()) {
    absl::StrAppend(&job_prefix, "_", prefix);
  }
  auto generator = google::cloud::internal::MakeDefaultPRNG();
  auto id = google::cloud::internal::Sample(generator, 32,
                                            "abcdefghijklmnopqrstuvwxyz");
  std::string job_id;
  absl::StrAppend(&job_id, job_prefix, "_", id);
  return job_id;
}

}  // anonymous namespace

void Benchmark::PrintThroughputResult(std::ostream& os,
                                      std::string const& test_name,
                                      std::string const& operation,
                                      BenchmarkResult const& result) {
  std::chrono::seconds elapsed =
      std::chrono::duration_cast<std::chrono::seconds>(result.elapsed);
  auto elapsed_dbl = static_cast<double>(elapsed.count());
  auto operations_size_dbl = static_cast<double>(result.operations.size());
  auto ops_throughput = operations_size_dbl / elapsed_dbl;
  os << "# Test=" << test_name << ", " << operation
     << ", op throughput=" << ops_throughput << " ops/s" << std::endl
     << "# Test=" << test_name << ", " << operation
     << ", Total number of operations performed=" << result.operations.size()
     << std::endl
     << "# Test=" << test_name << ", " << operation
     << ", Total elapsed time=" << elapsed.count() << " seconds" << std::endl;
}

void Benchmark::PrintLatencyResult(std::ostream& os,
                                   std::string const& test_name,
                                   std::string const& operation,
                                   BenchmarkResult& result) {
  if (result.operations.empty()) {
    os << "# Test=" << test_name << ", " << operation << " no results\n";
    return;
  }
  std::sort(result.operations.begin(), result.operations.end(),
            [](OperationResult const& lhs, OperationResult const& rhs) {
              return lhs.latency < rhs.latency;
            });

  auto const nsamples = result.operations.size();
  os << "# Test=" << test_name << ", " << operation << ", Latency And Status: ";
  char const* sep = "";
  for (double p : kResultPercentiles) {
    auto index = static_cast<std::size_t>(
        std::round(static_cast<double>(nsamples - 1) * p / 100.0));
    auto i = result.operations.begin();
    std::advance(i, index);
    os << sep << "p" << std::setprecision(3) << p << "=" << std::setprecision(2)
       << FormatDuration(i->latency);
    os << ", status=" << i->status;
    sep = ", ";
  }
  os << "\n";
}

DatasetBenchmark::DatasetBenchmark(DatasetConfig config)
    : config_(std::move(config)) {
  Options options;
  if (!config_.endpoint.empty()) {
    options.set<EndpointOption>(config_.endpoint);
  }
  if (config_.connection_pool_size > 0) {
    options.set<DatasetConnectionPoolSizeOption>(config_.connection_pool_size);
  }
  dataset_client_ =
      std::make_shared<DatasetClient>(MakeDatasetConnection(options));
}

TableBenchmark::TableBenchmark(TableConfig config)
    : config_(std::move(config)) {
  Options options;
  if (!config_.endpoint.empty()) {
    options.set<EndpointOption>(config_.endpoint);
  }
  if (config_.connection_pool_size > 0) {
    options.set<DatasetConnectionPoolSizeOption>(config_.connection_pool_size);
  }
  table_client_ = std::make_shared<TableClient>(MakeTableConnection(options));
}

ProjectBenchmark::ProjectBenchmark(Config config) : config_(std::move(config)) {
  Options options;
  if (!config_.endpoint.empty()) {
    options.set<EndpointOption>(config_.endpoint);
  }
  if (config_.connection_pool_size > 0) {
    options.set<DatasetConnectionPoolSizeOption>(config_.connection_pool_size);
  }
  project_client_ =
      std::make_shared<ProjectClient>(MakeProjectConnection(options));
}

JobBenchmark::JobBenchmark(JobConfig config) : config_(std::move(config)) {
  Options options;
  if (!config_.endpoint.empty()) {
    options.set<EndpointOption>(config_.endpoint);
  }
  if (config_.connection_pool_size > 0) {
    options.set<DatasetConnectionPoolSizeOption>(config_.connection_pool_size);
  }
  job_client_ = std::make_shared<JobClient>(MakeBigQueryJobConnection(options));
}

StatusOr<Dataset> DatasetBenchmark::GetDataset() {
  GetDatasetRequest request;
  if (config_.project_id.empty()) {
    return internal::InvalidArgumentError(
        "project_id config parameter is empty.", GCP_ERROR_INFO());
  }
  if (config_.dataset_id.empty()) {
    return internal::InvalidArgumentError(
        "dataset_id config parameter is empty.", GCP_ERROR_INFO());
  }
  request.set_project_id(config_.project_id);
  request.set_dataset_id(config_.dataset_id);

  return dataset_client_->GetDataset(request);
}

StatusOr<Table> TableBenchmark::GetTable() {
  GetTableRequest request;
  if (config_.project_id.empty()) {
    return internal::InvalidArgumentError(
        "project_id config parameter is empty.", GCP_ERROR_INFO());
  }
  if (config_.dataset_id.empty()) {
    return internal::InvalidArgumentError(
        "dataset_id config parameter is empty.", GCP_ERROR_INFO());
  }
  if (config_.table_id.empty()) {
    return internal::InvalidArgumentError("table_id config parameter is empty.",
                                          GCP_ERROR_INFO());
  }
  request.set_project_id(config_.project_id);
  request.set_dataset_id(config_.dataset_id);
  request.set_table_id(config_.table_id);

  // Optional request parameters.
  if (!config_.selected_fields.empty()) {
    auto selected_fields = StrSplit(config_.selected_fields, ',');
    request.set_selected_fields(selected_fields);
  }
  if (!config_.view.value.empty()) {
    request.set_view(config_.view);
  }
  return table_client_->GetTable(request);
}

StatusOr<Job> JobBenchmark::GetJob() {
  GetJobRequest request;
  if (config_.project_id.empty()) {
    return internal::InvalidArgumentError(
        "project_id config parameter is empty.", GCP_ERROR_INFO());
  }
  if (config_.job_id.empty()) {
    return internal::InvalidArgumentError("job_id config parameter is empty.",
                                          GCP_ERROR_INFO());
  }
  request.set_project_id(config_.project_id);
  request.set_job_id(config_.job_id);

  // Optional parameters.
  if (!config_.location.empty()) {
    request.set_location(config_.location);
  }
  return job_client_->GetJob(request);
}

StatusOr<Job> JobBenchmark::CancelJob() {
  CancelJobRequest request;
  if (config_.project_id.empty()) {
    return internal::InvalidArgumentError(
        "project_id config parameter is empty.", GCP_ERROR_INFO());
  }
  if (config_.job_id.empty()) {
    return internal::InvalidArgumentError("job_id config parameter is empty.",
                                          GCP_ERROR_INFO());
  }
  request.set_project_id(config_.project_id);
  request.set_job_id(config_.job_id);

  // Optional parameters.
  if (!config_.location.empty()) {
    request.set_location(config_.location);
  }

  return job_client_->CancelJob(request);
}

StreamRange<ListFormatDataset> DatasetBenchmark::ListDatasets() {
  ListDatasetsRequest request;
  if (config_.project_id.empty()) {
    return MakeStreamRange<ListFormatDataset>({});
  }
  request.set_project_id(config_.project_id);
  if (config_.all) {
    request.set_all_datasets(config_.all);
  }
  if (!config_.filter.empty()) {
    request.set_filter(config_.filter);
  }
  if (!config_.page_token.empty()) {
    request.set_page_token(config_.page_token);
  }
  if (config_.max_results > 0) {
    request.set_max_results(config_.max_results);
  }

  return dataset_client_->ListDatasets(request);
}

StreamRange<ListFormatTable> TableBenchmark::ListTables() {
  ListTablesRequest request;
  if (config_.project_id.empty()) {
    return MakeStreamRange<ListFormatTable>({});
  }
  if (config_.dataset_id.empty()) {
    return MakeStreamRange<ListFormatTable>({});
  }
  request.set_project_id(config_.project_id);
  request.set_dataset_id(config_.dataset_id);

  // Optional parameters.
  if (!config_.page_token.empty()) {
    request.set_page_token(config_.page_token);
  }
  if (config_.max_results > 0) {
    request.set_max_results(config_.max_results);
  }

  return table_client_->ListTables(request);
}

StreamRange<Project> ProjectBenchmark::ListProjects() {
  ListProjectsRequest request;

  // Optional parameters.
  if (!config_.page_token.empty()) {
    request.set_page_token(config_.page_token);
  }
  if (config_.max_results > 0) {
    request.set_max_results(config_.max_results);
  }

  return project_client_->ListProjects(request);
}

StreamRange<ListFormatJob> JobBenchmark::ListJobs() {
  ListJobsRequest request;
  if (config_.project_id.empty()) {
    return MakeStreamRange<ListFormatJob>({});
  }
  request.set_project_id(config_.project_id);
  if (config_.all_users) {
    request.set_all_users(config_.all_users);
  }
  if (!config_.min_creation_time.empty()) {
    request.set_min_creation_time(StrToTimepoint(config_.min_creation_time));
  }
  if (!config_.max_creation_time.empty()) {
    request.set_min_creation_time(StrToTimepoint(config_.max_creation_time));
  }
  if (!config_.page_token.empty()) {
    request.set_page_token(config_.page_token);
  }
  if (config_.max_results > 0) {
    request.set_max_results(config_.max_results);
  }
  if (!config_.state_filter.value.empty()) {
    request.set_state_filter(config_.state_filter);
  }
  if (!config_.projection.value.empty()) {
    request.set_projection(config_.projection);
  }
  if (!config_.parent_job_id.empty()) {
    request.set_parent_job_id(config_.parent_job_id);
  }

  return job_client_->ListJobs(request);
}

StatusOr<Job> JobBenchmark::InsertJob() {
  InsertJobRequest request;
  if (config_.project_id.empty()) {
    return internal::InvalidArgumentError(
        "project_id config parameter is empty.", GCP_ERROR_INFO());
  }
  request.set_project_id(config_.project_id);

  // Set request body.
  Job job;
  std::string request_body;
  std::string job_id;
  if (config_.dry_run) {
    request_body = JobConfig::GetInsertJobDryRunRequestBody();
    job_id = GenerateJobId("dry-run");
  } else {
    request_body = JobConfig::GetInsertJobRequestBody();
    job_id = GenerateJobId("real-run");
  }
  auto json = nlohmann::json::parse(request_body, nullptr, false);
  if (!json.is_object()) {
    return internal::InternalError(
        "Invalid JSON: Unable to parse request body: " + request_body,
        GCP_ERROR_INFO());
  }
  from_json(json, job);

  // Set Job Id;
  job.job_reference.job_id = job_id;

  // Make sure some of the required enum fields are not empty.
  if (job.configuration.query.script_options.key_result_statement.value
          .empty()) {
    job.configuration.query.script_options.key_result_statement =
        KeyResultStatementKind::UnSpecified();
  }
  if (job.statistics.job_query_stats.search_statistics.index_usage_mode.value
          .empty()) {
    job.statistics.job_query_stats.search_statistics.index_usage_mode =
        IndexUsageMode::UnSpecified();
  }
  if (job.statistics.script_statistics.evaluation_kind.value.empty()) {
    job.statistics.script_statistics.evaluation_kind =
        EvaluationKind::UnSpecified();
  }
  request.set_job(job);
  // Remove Json fields that shouldn't be part of the InsertJob payload for
  // this test case.
  request.set_json_filter_keys(
      {"statistics", "status", "labels", "destinationTable",
       "maximumBytesBilled", "userDefinedFunctionResources", "defaultDataset",
       "schemaUpdateOptions", "timePartitioning", "rangePartitioning",
       "clustering", "destinationEncryptionConfiguration", "scriptOptions",
       "connectionProperties", "systemVariables", "structTypes",
       "structValues"});
  return job_client_->InsertJob(request);
}

StatusOr<PostQueryResults> JobBenchmark::Query() {
  PostQueryRequest request;
  if (config_.project_id.empty()) {
    return internal::InvalidArgumentError(
        "project_id config parameter is empty.", GCP_ERROR_INFO());
  }
  request.set_project_id(config_.project_id);

  // Set request body.
  QueryRequest qr;
  std::string request_body;
  if (config_.query_create_replace) {
    if (config_.dry_run) {
      request_body = JobConfig::GetQueryCreateReplaceDryRunRequestBody();
    } else {
      request_body = JobConfig::GetQueryCreateReplaceRequestBody();
    }
  }
  if (config_.query_drop) {
    if (config_.dry_run) {
      request_body = JobConfig::GetQueryDropDryRunRequestBody();
    } else {
      request_body = JobConfig::GetQueryDropRequestBody();
    }
  }
  auto json = nlohmann::json::parse(request_body, nullptr, false);
  if (!json.is_object()) {
    return internal::InternalError(
        "Invalid JSON: Unable to parse request body: " + request_body,
        GCP_ERROR_INFO());
  }
  from_json(json, qr);
  request.set_query_request(qr);
  // Remove Json fields that shouldn't be part of the Query payload for
  // this test case.
  request.set_json_filter_keys({"preserveNulls", "labels", "requestId",
                                "queryParameters", "defaultDataset",
                                "maximumBytesBilled", "formatOptions",
                                "connectionProperties"});
  return job_client_->Query(request);
}

StatusOr<GetQueryResults> JobBenchmark::QueryResults() {
  GetQueryResultsRequest request;
  if (config_.project_id.empty()) {
    return internal::InvalidArgumentError(
        "project_id config parameter is empty.", GCP_ERROR_INFO());
  }
  if (config_.job_id.empty()) {
    return internal::InvalidArgumentError("job_id config parameter is empty.",
                                          GCP_ERROR_INFO());
  }
  request.set_project_id(config_.project_id);
  request.set_job_id(config_.job_id);

  // Optional parameters.
  if (config_.max_results > 0) {
    request.set_max_results(static_cast<std::uint32_t>(config_.max_results));
  }
  if (!config_.page_token.empty()) {
    request.set_page_token(config_.page_token);
  }
  if (!config_.location.empty()) {
    request.set_location(config_.location);
  }
  if (config_.start_index >= 0) {
    request.set_start_index(config_.start_index);
  }
  if (config_.timeout_ms > 0) {
    request.set_timeout(ToChronoMillis(config_.timeout_ms));
  }

  return job_client_->QueryResults(request);
}

std::ostream& operator<<(std::ostream& os, FormatDuration d) {
  return os << absl::FormatDuration(absl::FromChrono(d.ns));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_benchmarks
}  // namespace cloud
}  // namespace google
