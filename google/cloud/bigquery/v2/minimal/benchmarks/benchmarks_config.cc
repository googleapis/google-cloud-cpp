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
#include "absl/strings/match.h"
#include <functional>
#include <sstream>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_benchmarks {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::bigquery_v2_minimal_internal::Projection;
using ::google::cloud::bigquery_v2_minimal_internal::StateFilter;
using ::google::cloud::bigquery_v2_minimal_internal::TableMetadataView;

std::ostream& operator<<(std::ostream& os, Config const& config) {
  return os << "\n# Endpoint: " << config.endpoint
            << "\n# Project: " << config.project_id
            << "\n# Page Token: " << config.page_token
            << "\n# Max Results: " << config.max_results
            << "\n# Connection Size: " << config.connection_pool_size
            << "\n# Compiler: " << google::cloud::internal::CompilerId() << "-"
            << google::cloud::internal::CompilerVersion()
            << "\n# Build Flags: " << google::cloud::internal::compiler_flags()
            << "\n";
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
  auto invalid_argument = [](std::string msg) {
    return google::cloud::Status(google::cloud::StatusCode::kInvalidArgument,
                                 std::move(msg));
  };

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
  auto invalid_argument = [](std::string msg) {
    return google::cloud::Status(google::cloud::StatusCode::kInvalidArgument,
                                 std::move(msg));
  };

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
      {"--location=", [this](std::string v) { location = std::move(v); }});
  flags_.push_back({"--parent-job-id=",
                    [this](std::string v) { parent_job_id = std::move(v); }});
  flags_.push_back({"--all-users=", [this](std::string const& v) {
                      all_users = (v == "true");
                    }});
  flags_.push_back({"--min-creation-time=", [this](std::string v) {
                      min_creation_time = std::move(v);
                    }});
  flags_.push_back({"--max-creation-time=", [this](std::string v) {
                      max_creation_time = std::move(v);
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

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_benchmarks
}  // namespace cloud
}  // namespace google
