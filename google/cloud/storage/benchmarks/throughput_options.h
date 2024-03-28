// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BENCHMARKS_THROUGHPUT_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BENCHMARKS_THROUGHPUT_OPTIONS_H

#include "google/cloud/storage/benchmarks/benchmark_utils.h"
#include "google/cloud/options.h"
#include "absl/types/optional.h"
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace storage_benchmarks {

struct ThroughputOptions {
  std::string bucket;
  std::string labels;
  std::chrono::seconds duration =
      std::chrono::seconds(std::chrono::minutes(15));
  int thread_count = 1;
  bool client_per_thread = false;
  std::int64_t minimum_object_size = 32 * kMiB;
  std::int64_t maximum_object_size = 256 * kMiB;
  // Control the size of the read and write buffers in the application.
  std::size_t minimum_write_buffer_size = 16 * kMiB;
  std::size_t maximum_write_buffer_size = 64 * kMiB;
  std::size_t write_buffer_quantum = 256 * kKiB;
  std::size_t minimum_read_buffer_size = 4 * kMiB;
  std::size_t maximum_read_buffer_size = 8 * kMiB;
  std::size_t read_buffer_quantum = 1 * kMiB;
  std::int32_t minimum_sample_count = 0;
  std::int32_t maximum_sample_count = std::numeric_limits<std::int32_t>::max();
  std::vector<ExperimentLibrary> libs = {
      ExperimentLibrary::kCppClient,
  };
  std::vector<ExperimentTransport> transports = {
      ExperimentTransport::kGrpc,
      ExperimentTransport::kJson,
  };
  std::vector<std::string> upload_functions = {"InsertObject", "WriteObject"};
  std::vector<bool> enabled_crc32c = {false, true};
  std::vector<bool> enabled_md5 = {false, true};
  std::chrono::milliseconds minimum_sample_delay{};
  absl::optional<std::int64_t> minimum_read_offset;
  absl::optional<std::int64_t> maximum_read_offset;
  std::int64_t read_offset_quantum = 128 * kKiB;
  absl::optional<std::int64_t> minimum_read_size;
  absl::optional<std::int64_t> maximum_read_size;
  std::int64_t read_size_quantum = 128 * kKiB;
  Options client_options;
  Options rest_options;
  Options grpc_options;
  Options direct_path_options =
      Options{}.set<EndpointOption>("google-c2p:///storage.googleapis.com");
  bool exit_after_parse = false;
};

google::cloud::StatusOr<ThroughputOptions> ParseThroughputOptions(
    std::vector<std::string> const& argv, std::string const& description = {});

}  // namespace storage_benchmarks
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BENCHMARKS_THROUGHPUT_OPTIONS_H
