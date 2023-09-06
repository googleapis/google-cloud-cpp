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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_BENCHMARKS_BENCHMARKS_CONFIG_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_BENCHMARKS_BENCHMARKS_CONFIG_H

#include "google/cloud/bigquery/v2/minimal/internal/job_request.h"
#include "google/cloud/bigquery/v2/minimal/internal/table_view.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <chrono>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_benchmarks {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

struct Config {
  std::string endpoint;

  std::string project_id;

  std::string page_token;
  int max_results = 1000;
  int connection_pool_size = 4;

  google::cloud::StatusOr<Config> ParseArgs(
      std::vector<std::string> const& args);

 protected:
  struct Flag {
    std::string flag_name;
    std::function<void(std::string)> parser;
  };

  void ParseCommonFlags();
  inline bool CommonFlagsParsed() { return !flags_.empty(); }
  google::cloud::Status ValidateArgs(std::vector<std::string> const& args);

  std::vector<Config::Flag> flags_;
};

struct DatasetConfig : public Config {
  std::string filter;
  bool all;

  google::cloud::StatusOr<DatasetConfig> ParseArgs(
      std::vector<std::string> const& args);
};

struct TableConfig : public Config {
  std::string selected_fields;
  bigquery_v2_minimal_internal::TableMetadataView view;

  google::cloud::StatusOr<TableConfig> ParseArgs(
      std::vector<std::string> const& args);
};

struct JobConfig : public Config {
  std::string location;
  bool all_users;
  std::string min_creation_time;
  std::string max_creation_time;
  std::string parent_job_id;

  bigquery_v2_minimal_internal::Projection projection;
  bigquery_v2_minimal_internal::StateFilter state_filter;

  google::cloud::StatusOr<JobConfig> ParseArgs(
      std::vector<std::string> const& args);
};

std::ostream& operator<<(std::ostream& os, Config const& config);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_benchmarks
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_BENCHMARKS_BENCHMARKS_CONFIG_H
