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
#include "google/cloud/internal/random.h"
#include "absl/strings/match.h"
#include <chrono>
#include <functional>
#include <sstream>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_benchmarks {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::bigquery_v2_minimal_internal::Projection;
using ::google::cloud::bigquery_v2_minimal_internal::StateFilter;
using ::google::cloud::bigquery_v2_minimal_internal::TableMetadataView;

namespace {

auto invalid_argument = [](std::string msg) {
  return google::cloud::Status(google::cloud::StatusCode::kInvalidArgument,
                               std::move(msg));
};

std::string GenerateJobId() {
  auto constexpr kJobPrefix = "bqOdbcJob_benchmark_test_";
  auto generator = google::cloud::internal::MakeDefaultPRNG();
  auto job_id = kJobPrefix + google::cloud::internal::Sample(
                                 generator, 32, "abcdefghijklmnopqrstuvwxyz");
  return job_id;
}

std::string insert_job_dr_request_body =
    R"({"jobReference":{"projectId":"bigquery-devtools-drivers")"
    R"(,"jobId":")" +
    GenerateJobId() +
    R"("})"
    R"(,"configuration":{"dryRun":true)"
    R"(,"query":{"query":'insert into ODBCTESTDATASET.ODBCTESTTABLE VALUES(?)')"
    R"(,"useQueryCache":true,"useLegacySql":false,"createSession":false,"parameterMode":"POSITIONAL"}}})";

std::string insert_job_request_body =
    R"({"jobReference":{"projectId":"bigquery-devtools-drivers")"
    R"(,"jobId":")" +
    GenerateJobId() +
    R"("})"
    R"(,"configuration":{"dryRun":false)"
    R"(,"query":{"query":'insert into ODBCTESTDATASET.ODBCTESTTABLE VALUES(?)')"
    R"(,"useQueryCache":true,"useLegacySql":false)"
    R"(,"createSession":false,"parameterMode":"POSITIONAL")"
    R"(,"queryParameters":[{"parameterType":{"type":"STRING"},"parameterValue":{"value":"testdata"}}]}}})";

std::string query_create_replace_dr_request_body =
    R"({"query":'create or replace table ODBCTESTDATASET.ODBCTESTTABLE (name STRING)')"
    R"(,"dryRun":true,"maxResults":100000,"useLegacySql":false)"
    R"(,"timeoutMs":10000,"useQueryCache":true,"createSession":false,"parameterMode":"POSITIONAL"})";

std::string query_create_replace_request_body =
    R"({"query":'create or replace table ODBCTESTDATASET.ODBCTESTTABLE (name STRING)')"
    R"(,"dryRun":false,"maxResults":100000,"useLegacySql":false)"
    R"(,"timeoutMs":10000,"useQueryCache":true,"createSession":false})";

std::string query_drop_dr_request_body =
    R"({"query":"drop table if exists ODBCTESTDATASET.ODBCTESTTABLE")"
    R"(,"dryRun":true,"maxResults":100000)"
    R"(,"useLegacySql":false,"timeoutMs":10000)"
    R"(,"useQueryCache":true,"createSession":false,"parameterMode":"POSITIONAL"})";

std::string query_drop_request_body =
    R"({"query":"drop table if exists ODBCTESTDATASET.ODBCTESTTABLE")"
    R"(,"dryRun":false,"maxResults":100000)"
    R"(,"useLegacySql":false,"timeoutMs":10000)"
    R"(,"useQueryCache":true,"createSession":false})";

std::ostream& PrintHelper(std::ostream& os, std::string const& endpoint,
                          std::string const& project_id,
                          std::string const& page_token, int max_results,
                          int connection_pool_size) {
  return os << "\n# Endpoint: " << endpoint << "\n# Project: " << project_id
            << "\n# Page Token: " << page_token
            << "\n# Max Results: " << max_results
            << "\n# Connection Size: " << connection_pool_size
            << "\n# Compiler: " << google::cloud::internal::CompilerId() << "-"
            << google::cloud::internal::CompilerVersion()
            << "\n# Build Flags: " << google::cloud::internal::compiler_flags()
            << "\n";
}
}  // namespace

std::ostream& operator<<(std::ostream& os, Config const& config) {
  return PrintHelper(os, config.endpoint, config.project_id, config.page_token,
                     config.max_results, config.connection_pool_size);
}

std::ostream& operator<<(std::ostream& os, DatasetConfig const& config) {
  os << "\n# Dataset: " << config.dataset_id << "\n# All: " << config.all
     << "\n# Filter: " << config.filter
     << "\n# Page Token: " << config.page_token;
  return PrintHelper(os, config.endpoint, config.project_id, config.page_token,
                     config.max_results, config.connection_pool_size);
}

std::ostream& operator<<(std::ostream& os, TableConfig const& config) {
  os << "\n# Dataset: " << config.dataset_id << "\n# Table: " << config.table_id
     << "\n# Selected Fields: " << config.selected_fields
     << "\n# View: " << config.view.value;

  return PrintHelper(os, config.endpoint, config.project_id, config.page_token,
                     config.max_results, config.connection_pool_size);
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
                     config.max_results, config.connection_pool_size);
}

void Config::ParseCommonFlags() {
  flags_.push_back(
      {"--endpoint=", [this](std::string v) { endpoint = std::move(v); }});
  flags_.push_back(
      {"--project=", [this](std::string v) { project_id = std::move(v); }});
  flags_.push_back(
      {"--page-token=", [this](std::string v) { page_token = std::move(v); }});
  flags_.push_back({"--connection-pool-size=", [this](std::string const& v) {
                      connection_pool_size = std::stoi(v);
                    }});
  flags_.push_back({"--maximum-results=", [this](std::string const& v) {
                      max_results = std::stoi(v);
                    }});
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

  return google::cloud::Status(StatusCode::kOk, "");
}

google::cloud::StatusOr<Config> Config::ParseArgs(
    std::vector<std::string> const& args) {
  if (!CommonFlagsParsed()) {
    endpoint = "https://bigquery.googleapis.com";
    project_id =
        google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");

    ParseCommonFlags();
    auto status = ValidateArgs(args);

    if (!status.ok()) {
      return status;
    }
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
  ParseCommonFlags();
  flags_.push_back(
      {"--dataset=", [this](std::string v) { dataset_id = std::move(v); }});
  flags_.push_back(
      {"--filter=", [this](std::string v) { filter = std::move(v); }});
  flags_.push_back(
      {"--all=", [this](std::string const& v) { all = (v == "true"); }});

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
  ParseCommonFlags();
  flags_.push_back(
      {"--dataset=", [this](std::string v) { dataset_id = std::move(v); }});
  flags_.push_back(
      {"--table=", [this](std::string v) { table_id = std::move(v); }});
  flags_.push_back({"--selected-fields=",
                    [this](std::string v) { selected_fields = std::move(v); }});
  flags_.push_back({"--view=", [this](std::string const& v) {
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
  ParseCommonFlags();
  flags_.push_back(
      {"--job=", [this](std::string v) { job_id = std::move(v); }});
  flags_.push_back(
      {"--location=", [this](std::string v) { location = std::move(v); }});
  flags_.push_back({"--parent-job-id=",
                    [this](std::string v) { parent_job_id = std::move(v); }});
  flags_.push_back({"--all-users=", [this](std::string const& v) {
                      all_users = (v == "true");
                    }});
  flags_.push_back({"--dry-run=",
                    [this](std::string const& v) { dry_run = (v == "true"); }});
  flags_.push_back({"--query-create-replace=", [this](std::string const& v) {
                      query_create_replace = (v == "true");
                    }});
  flags_.push_back({"--query-drop=", [this](std::string const& v) {
                      query_drop = (v == "true");
                    }});
  flags_.push_back({"--use-int64-timestamp=", [this](std::string const& v) {
                      use_int64_timestamp = (v == "true");
                    }});
  flags_.push_back({"--min-creation-time=", [this](std::string v) {
                      min_creation_time = std::move(v);
                    }});
  flags_.push_back({"--max-creation-time=", [this](std::string v) {
                      max_creation_time = std::move(v);
                    }});
  flags_.push_back({"--timeout-ms=", [this](std::string const& v) {
                      timeout_ms = std::stoi(v);
                    }});
  flags_.push_back({"--start-index=", [this](std::string const& v) {
                      start_index = std::stoi(v);
                    }});
  flags_.push_back({"--projection=", [this](std::string const& v) {
                      if (v == "MINIMAL") {
                        projection = Projection::Minimal();
                      } else if (v == "FULL") {
                        projection = Projection::Full();
                      }
                    }});
  flags_.push_back({"--state-filter=", [this](std::string const& v) {
                      if (v == "RUNNING") {
                        state_filter = StateFilter::Running();
                      } else if (v == "PENDING") {
                        state_filter = StateFilter::Pending();
                      } else if (v == "DONE") {
                        state_filter = StateFilter::Done();
                      }
                    }});

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
