// Copyright 2017 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_BENCHMARKS_SETUP_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_BENCHMARKS_SETUP_H_

#include "google/cloud/bigtable/benchmarks/constants.h"
#include "google/cloud/status_or.h"
#include <chrono>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable {
namespace benchmarks {
/**
 * The configuration data for a benchmark.
 */
struct BenchmarkSetupData {
  std::string start_time;
  std::string notes;
  std::string project_id;
  std::string instance_id;
  std::string app_profile_id;
  std::string table_id;
  int thread_count;
  long table_size;
  std::chrono::seconds test_duration;
  bool use_embedded_server;

  int parallel_requests;
};

/**
 * The configuration for a benchmark.
 */
class BenchmarkSetup {
 public:
  explicit BenchmarkSetup(BenchmarkSetupData setup_data);

  /// When did the benchmark start, this is used in reporting the results.
  std::string const& start_time() const { return setup_data_.start_time; }
  /// Benchmark annotations, e.g., compiler version and flags.
  std::string const& notes() const { return setup_data_.notes; }
  std::string const& project_id() const { return setup_data_.project_id; }
  std::string const& instance_id() const { return setup_data_.instance_id; }
  std::string const& app_profile_id() const {
    return setup_data_.app_profile_id;
  }

  /// The randomly generated table id for the benchmark.
  std::string const& table_id() const { return setup_data_.table_id; }

  long table_size() const { return setup_data_.table_size; }
  int thread_count() const { return setup_data_.thread_count; }
  std::chrono::seconds test_duration() const {
    return setup_data_.test_duration;
  }
  bool use_embedded_server() const { return setup_data_.use_embedded_server; }

  int parallel_requests() const { return setup_data_.parallel_requests; }

 private:
  BenchmarkSetupData setup_data_;
};

/**
 * Does the actual work in constructing a BenchmarkSetup. Since we do not want
 * to use exceptions here, we factor out the logic in which an error can occur
 * into a separate function.
 */
google::cloud::StatusOr<BenchmarkSetup> MakeBenchmarkSetup(
    std::string const& prefix, int& argc, char* argv[]);

}  // namespace benchmarks
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_BENCHMARKS_SETUP_H_
