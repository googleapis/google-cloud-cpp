// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_BENCHMARKS_BENCHMARKS_CONFIG_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_BENCHMARKS_BENCHMARKS_CONFIG_H

#include "google/cloud/spanner/version.h"
#include "google/cloud/status_or.h"
#include <chrono>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace spanner_benchmarks {
inline namespace SPANNER_CLIENT_NS {

struct Config {
  std::string experiment;

  std::string project_id;
  std::string instance_id;
  std::string database_id;

  int samples = 2;
  std::chrono::seconds iteration_duration = std::chrono::seconds(5);

  int minimum_threads = 1;
  int maximum_threads = 1;
  // TODO(#1193) change these variable names from `*_clients` to `*_channels`
  int minimum_clients = 1;
  int maximum_clients = 1;

  std::int64_t table_size = 1000 * 1000L;
  std::int64_t query_size = 1000;

  bool use_only_clients = false;
  bool use_only_stubs = false;
};

std::ostream& operator<<(std::ostream& os, Config const& config);

google::cloud::StatusOr<Config> ParseArgs(std::vector<std::string> args);

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_benchmarks
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_BENCHMARKS_BENCHMARKS_CONFIG_H
