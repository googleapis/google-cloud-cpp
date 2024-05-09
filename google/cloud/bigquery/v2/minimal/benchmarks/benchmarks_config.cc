// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigquery/v2/minimal/benchmarks/benchmarks_config.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/build_info.h"
#include "google/cloud/internal/compiler_info.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/random.h"
#include "absl/strings/match.h"
#include <chrono>
#include <functional>
#include <sstream>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_benchmarks {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::StatusCode;
using ::google::cloud::bigquery_v2_minimal_internal::Projection;
using ::google::cloud::bigquery_v2_minimal_internal::StateFilter;
using ::google::cloud::bigquery_v2_minimal_internal::TableMetadataView;

namespace {

auto invalid_argument = [](std::string msg) {
  return internal::InvalidArgumentError(std::move(msg), GCP_ERROR_INFO());
};

auto status_ok = google::cloud::Status(StatusCode::kOk, "");

std::string insert_job_dr_request_body =
    R"({"jobReference":{"projectId":"bigquery-devtools-drivers")"
    R"(,"location":"US")"
    R"(})"
    R"(,"configuration":{"dryRun":true)"
    R"(,"query":{"query":"insert into ODBCTESTDATASET.ODBCTESTTABLE_INSERT VALUES\u0028\u003f\u0029")"
    R"(,"useQueryCache":true,"useLegacySql":false,"createSession":false,"parameterMode":"POSITIONAL"}}})";

std::string insert_job_request_body =
    R"({"jobReference":{"projectId":"bigquery-devtools-drivers")"
    R"(,"location":"US")"
    R"(})"
    R"(,"configuration":{"dryRun":false)"
    R"(,"query":{"query":"insert into ODBCTESTDATASET.ODBCTESTTABLE_INSERT VALUES\u0028\u003f\u0029")"
    R"(,"useQueryCache":true,"useLegacySql":false)"
    R"(,"createSession":false,"parameterMode":"POSITIONAL")"
    R"(,"queryParameters":[{"parameterType":{"type":"STRING"},"parameterValue":{"value":"testdata"}}]}}})";

std::string query_create_replace_dr_request_body =
    R"({"kind":"job")"
    R"(,"query":"create or replace table ODBCTESTDATASET.ODBCTESTTABLE_QUERY \u0028name STRING\u0029")"
    R"(,"dryRun":true,"maxResults":100000,"useLegacySql":false)"
    R"(,"location":"US")"
    R"(,"timeoutMs":10000,"useQueryCache":true,"createSession":false,"parameterMode":"POSITIONAL"})";

std::string query_create_replace_request_body =
    R"({"kind":"job")"
    R"(,"query":"create or replace table ODBCTESTDATASET.ODBCTESTTABLE_QUERY \u0028name STRING\u0029")"
    R"(,"dryRun":false,"maxResults":100000,"useLegacySql":false)"
    R"(,"location":"US")"
    R"(,"timeoutMs":10000,"useQueryCache":true,"createSession":false})";

std::string query_drop_dr_request_body =
    R"({"kind":"job")"
    R"(,"query":"drop table if exists ODBCTESTDATASET.ODBCTESTTABLE_QUERY")"
    R"(,"dryRun":true,"maxResults":100000)"
    R"(,"location":"US")"
    R"(,"useLegacySql":false,"timeoutMs":10000)"
    R"(,"useQueryCache":true,"createSession":false,"parameterMode":"POSITIONAL"})";

std::string query_drop_request_body =
    R"({"kind":"job")"
    R"(,"query":"drop table if exists ODBCTESTDATASET.ODBCTESTTABLE_QUERY")"
    R"(,"dryRun":false,"maxResults":100000)"
    R"(,"location":"US")"
    R"(,"useLegacySql":false,"timeoutMs":10000)"
    R"(,"useQueryCache":true,"createSession":false})";

std::ostream& PrintHelper(std::ostream& os, std::string const& endpoint,
                          std::string const& project_id,
                          std::string const& page_token, int max_results,
                          int thread_count, int connection_pool_size,
                          std::chrono::seconds const& test_duration) {
  return os << "\n# Endpoint: " << endpoint << "\n# Project: " << project_id
            << "\n# Page Token: " << page_token
            << "\n# Max Results: " << max_results
            << "\n# Thread Count: " << thread_count
            << "\n# Connection Size: " << connection_pool_size
            << "\n# Test Duration (in seconds): " << test_duration.count()
            << "\n# Compiler: " << google::cloud::internal::CompilerId() << "-"
            << google::cloud::internal::CompilerVersion()
            << "\n# Build Flags: " << google::cloud::internal::compiler_flags()
            << "\n";
}
}  // namespace

std::ostream& operator<<(std::ostream& os, Config const& config) {
  return PrintHelper(os, config.endpoint, config.project_id, config.page_token,
                     config.max_results, config.thread_count,
                     config.connection_pool_size, config.test_duration);
}

std::ostream& operator<<(std::ostream& os, DatasetConfig const& config) {
  os << "\n# Dataset: " << config.dataset_id << "\n# All: " << config.all
     << "\n# Filter: " << config.filter
     << "\n# Page Token: " << config.page_token;
  return PrintHelper(os, config.endpoint, config.project_id, config.page_token,
                     config.max_results, config.thread_count,
                     config.connection_pool_size, config.test_duration);
}

std::ostream& operator<<(std::ostream& os, TableConfig const& config) {
  os << "\n# Dataset: " << config.dataset_id << "\n# Table: " << config.table_id
     << "\n# Selected Fields: " << config.selected_fields
     << "\n# View: " << config.view.value;

  return PrintHelper(os, config.endpoint, config.project_id, config.page_token,
                     config.max_results, config.thread_count,
                     config.connection_pool_size, config.test_duration);
}

std::ostream& operator<<(std::ostream& os, JobConfig const& config) {
  os << "\n# Job: " << config.job_id << "\n# Location: " << config.location
     << "\n# All Users: " << config.all_users
     << "\n# Min Creation Time: " << config.min_creation_time
     << "\n# Man Creation Time: " << config.max_creation_time
     << "\n# Parent Job Id: " << config.parent_job_id
     << "\n# Projection: " << config.projection.value
     << "\n# State Filter: " << config.state_filter.value;

  return PrintHelper(os, config.endpoint, config.project_id, config.page_token,
                     config.max_results, config.thread_count,
                     config.connection_pool_size, config.test_duration);
}

void Config::PrintUsage() {
  std::cout << "Usage Information: " << std::endl;
  std::cout << "Brief information about the flags is listed below. Please take "
               "a look at the Bigquery api docs "
               "(https://cloud.google.com/bigquery/docs/reference/rest) "
               "for more details information "
               "regarding api specific flags."
            << std::endl;
  std::cout << std::endl;
  for (auto const& flag : flags_) {
    std::cout << flag.flag_name << "=>" << flag.flag_desc << std::endl;
  }
}

google::cloud::Status Config::ParseCommonArgs(
    std::vector<std::string> const& args) {
  flags_.push_back(
      {"--wants-description=", "print benchmark description",
       [this](std::string const& v) { wants_description = (v == "true"); }});
  flags_.push_back(
      {"--help=", "print usage information",
       [this](std::string const& v) { wants_help = (v == "true"); }});
  flags_.push_back({"--endpoint=", "the Bigquery api endpoint",
                    [this](std::string v) { endpoint = std::move(v); }});
  flags_.push_back({"--project=", "the GCP project ID",
                    [this](std::string v) { project_id = std::move(v); }});
  flags_.push_back({"--page-token=", "page token for multiple page results",
                    [this](std::string v) { page_token = std::move(v); }});
  flags_.push_back(
      {"--connection-pool-size=", "connection pool size for rest connections",
       [this](std::string const& v) { connection_pool_size = std::stoi(v); }});
  flags_.push_back(
      {"--maximum-results=", "the maximum results returned in a single page",
       [this](std::string const& v) { max_results = std::stoi(v); }});
  flags_.push_back(
      {"--thread-count=", "the number of threads to use for this benchmark",
       [this](std::string const& v) { thread_count = std::stoi(v); }});
  flags_.push_back({"--test-duration=", "the duration of this test",
                    [this](std::string const& v) {
                      test_duration = std::chrono::seconds(std::stoi(v));
                    }});

  return ValidateArgs(args);
}

google::cloud::Status Config::ValidateArgs(
    std::vector<std::string> const& args) {
  for (auto i = std::next(args.begin()); i != args.end(); ++i) {
    std::string const& arg = *i;
    bool found = false;
    for (auto const& flag : flags_) {
      if (!absl::StartsWith(arg, flag.flag_name)) continue;
      found = true;
      flag.parser(arg.substr(flag.flag_name.size()));
      break;
    }
    if (!found && absl::StartsWith(arg, "--")) {
      return invalid_argument("Unexpected command-line flag " + arg);
    }
  }

  if (wants_description || wants_help) {
    exit_after_parse_ = true;
  }

  return status_ok;
}

google::cloud::StatusOr<Config> Config::ParseArgs(
    std::vector<std::string> const& args) {
  if (!CommonFlagsParsed()) {
    endpoint = "https://bigquery.googleapis.com";
    project_id =
        google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");

    auto status = ParseCommonArgs(args);
    if (!status.ok()) {
      return status;
    }
  }

  if (ExitAfterParse()) {
    return *this;
  }

  if (project_id.empty()) {
    return invalid_argument(
        "The project id is not set, provide a value in the --project flag,"
        " or set the GOOGLE_CLOUD_PROJECT environment variable");
  }
  if (endpoint.empty()) {
    return invalid_argument(
        "The endpoint is not set, provide a value in the --endpoint flag");
  }
  if (max_results <= 0) {
    std::ostringstream os;
    os << "The maximum number of results (" << max_results << ")"
       << " must be greater than zero";
    return invalid_argument(os.str());
  }
  if (connection_pool_size <= 0) {
    std::ostringstream os;
    os << "The connection pool size (" << connection_pool_size << ")"
       << " must be greater than zero";
    return invalid_argument(os.str());
  }
  return *this;
}

google::cloud::StatusOr<DatasetConfig> DatasetConfig::ParseArgs(
    std::vector<std::string> const& args) {
  ParseCommonArgs(args);

  flags_.push_back({"--dataset=", "the Bigquery Dataset ID",
                    [this](std::string v) { dataset_id = std::move(v); }});
  flags_.push_back(
      {"--filter=", "the Dataset filter to filter the results by label",
       [this](std::string v) { filter = std::move(v); }});
  flags_.push_back(
      {"--all=", "whether to list all datasets, including hidden ones",
       [this](std::string const& v) { all = (v == "true"); }});

  if (ExitAfterParse()) {
    return *this;
  }
  auto status = ValidateArgs(args);
  if (!status.ok()) {
    return status;
  }

  auto parse_status = Config::ParseArgs(args);
  if (!parse_status.ok()) {
    return parse_status.status();
  }

  return *this;
}

google::cloud::StatusOr<TableConfig> TableConfig::ParseArgs(
    std::vector<std::string> const& args) {
  ParseCommonArgs(args);

  flags_.push_back({"--dataset=", "the Dataset ID of the requested table",
                    [this](std::string v) { dataset_id = std::move(v); }});
  flags_.push_back({"--table=", "the Table ID of the requested table",
                    [this](std::string v) { table_id = std::move(v); }});
  flags_.push_back(
      {"--selected-fields=", "list of table schema fields to return",
       [this](std::string v) { selected_fields = std::move(v); }});
  flags_.push_back({"--view=",
                    "specifies the view that determines which table "
                    "information is returned.",
                    [this](std::string const& v) {
                      if (v == "TABLE_METADATA_VIEW_UNSPECIFIED") {
                        view = TableMetadataView::UnSpecified();
                      } else if (v == "BASIC") {
                        view = TableMetadataView::Basic();
                      } else if (v == "STORAGE_STATS") {
                        view = TableMetadataView::StorageStats();
                      } else if (v == "FULL") {
                        view = TableMetadataView::Full();
                      }
                    }});

  if (ExitAfterParse()) {
    return *this;
  }

  auto status = ValidateArgs(args);
  if (!status.ok()) {
    return status;
  }

  if (dataset_id.empty()) {
    return invalid_argument(
        "The dataset id is not set, provide a value in the --dataset flag");
  }

  auto parse_status = Config::ParseArgs(args);
  if (!parse_status.ok()) {
    return parse_status.status();
  }

  return *this;
}

google::cloud::StatusOr<JobConfig> JobConfig::ParseArgs(
    std::vector<std::string> const& args) {
  ParseCommonArgs(args);

  flags_.push_back({"--job=", "the Job ID of the requested job.",
                    [this](std::string v) { job_id = std::move(v); }});
  flags_.push_back({"--location=", "the geographic location of the job",
                    [this](std::string v) { location = std::move(v); }});
  flags_.push_back({"--parent-job-id=",
                    "if set, show only child jobs of the specified parent",
                    [this](std::string v) { parent_job_id = std::move(v); }});
  flags_.push_back(
      {"--all-users=",
       "whether to display jobs owned by all users in the project",
       [this](std::string const& v) { all_users = (v == "true"); }});
  flags_.push_back({"--dry-run=", "dry run mode",
                    [this](std::string const& v) { dry_run = (v == "true"); }});
  flags_.push_back(
      {"--query-create-replace=", "whether to execute create-replace stmt",
       [this](std::string const& v) { query_create_replace = (v == "true"); }});
  flags_.push_back(
      {"--query-drop=", "whether to execute drop stmt",
       [this](std::string const& v) { query_drop = (v == "true"); }});
  flags_.push_back(
      {"--use-int64-timestamp=", "outputs timestamp as usec int64",
       [this](std::string const& v) { use_int64_timestamp = (v == "true"); }});
  flags_.push_back(
      {"--min-creation-time=",
       "min job creation time. If set, only jobs created after or at this "
       "timestamp are returned",
       [this](std::string v) { min_creation_time = std::move(v); }});
  flags_.push_back(
      {"--max-creation-time=",
       "max job creation time. If set, only jobs created before or at this "
       "timestamp are returned",
       [this](std::string v) { max_creation_time = std::move(v); }});
  flags_.push_back(
      {"--timeout-ms=",
       "specifies the maximum amount of time, in milliseconds, that the client "
       "is willing to wait for the query to complete",
       [this](std::string const& v) { timeout_ms = std::stoi(v); }});
  flags_.push_back(
      {"--start-index=", "zero-based index of the starting row",
       [this](std::string const& v) { start_index = std::stoi(v); }});
  flags_.push_back(
      {"--projection=",
       "restricts information returned to a set of selected fields",
       [this](std::string const& v) {
         if (v == "MINIMAL") {
           projection = Projection::Minimal();
         } else if (v == "FULL") {
           projection = Projection::Full();
         }
       }});
  flags_.push_back(
      {"--state-filter=", "filter for job state", [this](std::string const& v) {
         if (v == "RUNNING") {
           state_filter = StateFilter::Running();
         } else if (v == "PENDING") {
           state_filter = StateFilter::Pending();
         } else if (v == "DONE") {
           state_filter = StateFilter::Done();
         }
       }});

  if (ExitAfterParse()) {
    return *this;
  }

  auto status = ValidateArgs(args);
  if (!status.ok()) {
    return status;
  }

  auto parse_status = Config::ParseArgs(args);
  if (!parse_status.ok()) {
    return parse_status.status();
  }

  return *this;
}

std::string JobConfig::GetInsertJobDryRunRequestBody() {
  return insert_job_dr_request_body;
}
std::string JobConfig::GetInsertJobRequestBody() {
  return insert_job_request_body;
}
std::string JobConfig::GetQueryCreateReplaceDryRunRequestBody() {
  return query_create_replace_dr_request_body;
}
std::string JobConfig::GetQueryCreateReplaceRequestBody() {
  return query_create_replace_request_body;
}
std::string JobConfig::GetQueryDropDryRunRequestBody() {
  return query_drop_dr_request_body;
}
std::string JobConfig::GetQueryDropRequestBody() {
  return query_drop_request_body;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_benchmarks
}  // namespace cloud
}  // namespace google
