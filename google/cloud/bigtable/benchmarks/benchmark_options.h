// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_BENCHMARKS_BENCHMARK_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_BENCHMARKS_BENCHMARK_OPTIONS_H

#include "google/cloud/bigtable/benchmarks/constants.h"
#include "google/cloud/status_or.h"
#include <chrono>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable {
namespace benchmarks {
/// The configuration data for a benchmark.
struct BenchmarkOptions {
  std::string project_id;
  std::string instance_id;
  std::string table_id;
  std::string start_time;
  std::string notes;
  std::string app_profile_id = "default";
  int thread_count = kDefaultThreads;
  std::int64_t table_size = kDefaultTableSize;
  std::chrono::seconds test_duration =
      std::chrono::seconds(kDefaultTestDuration * 60);
  bool use_embedded_server = false;
  int parallel_requests = 10;
  bool exit_after_parse = false;
  bool include_read_rows = false;
};

google::cloud::StatusOr<BenchmarkOptions> ParseBenchmarkOptions(
    std::vector<std::string> const& argv, std::string const& description);

}  // namespace benchmarks
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_BENCHMARKS_BENCHMARK_OPTIONS_H
