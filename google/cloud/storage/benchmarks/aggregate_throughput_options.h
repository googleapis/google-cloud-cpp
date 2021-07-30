// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BENCHMARKS_AGGREGATE_THROUGHPUT_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BENCHMARKS_AGGREGATE_THROUGHPUT_OPTIONS_H

#include "google/cloud/storage/benchmarks/benchmark_utils.h"
#include <cstdint>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace storage_benchmarks {

struct AggregateThroughputOptions {
  std::string labels;
  std::string bucket_name;
  std::string object_prefix;
  int thread_count = 1;
  int iteration_count = 1;
  int repeats_per_iteration = 1;
  std::int64_t read_size = 0;  // 0 means "read the whole file"
  std::size_t read_buffer_size = 4 * kMiB;
  ApiName api = ApiName::kApiGrpc;
  int grpc_channel_count = 0;
  std::string grpc_plugin_config;
  std::string rest_http_version;
  bool client_per_thread = false;
  bool exit_after_parse = false;
};

google::cloud::StatusOr<AggregateThroughputOptions>
ParseAggregateThroughputOptions(std::vector<std::string> const& argv,
                                std::string const& description);

}  // namespace storage_benchmarks
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BENCHMARKS_AGGREGATE_THROUGHPUT_OPTIONS_H
