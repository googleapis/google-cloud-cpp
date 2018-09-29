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
#include <chrono>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable {
namespace benchmarks {
/**
 * The configuration for a benchmark.
 */
class BenchmarkSetup {
 public:
  BenchmarkSetup(std::string const& prefix, int& argc, char* argv[]);

  /// When did the benchmark start, this is used in reporting the results.
  std::string const& start_time() const { return start_time_; }
  /// Benchmark annotations, e.g., compiler version and flags.
  std::string const& notes() const { return notes_; }
  std::string const& project_id() const { return project_id_; }
  std::string const& instance_id() const { return instance_id_; }
  std::string const& app_profile_id() const { return app_profile_id_; }

  /// The randomly generated table id for the benchmark.
  std::string const& table_id() const { return table_id_; }

  long table_size() const { return table_size_; }
  int thread_count() const { return thread_count_; }
  std::chrono::seconds test_duration() const { return test_duration_; }
  bool use_embedded_server() const { return use_embedded_server_; }

 private:
  std::string start_time_;
  std::string notes_;
  std::string project_id_;
  std::string instance_id_;
  std::string app_profile_id_;
  std::string table_id_;
  int thread_count_ = kDefaultThreads;
  long table_size_ = kDefaultTableSize;
  std::chrono::seconds test_duration_ =
      std::chrono::seconds(kDefaultTestDuration * 60);
  bool use_embedded_server_ = false;
};

}  // namespace benchmarks
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_BENCHMARKS_SETUP_H_
