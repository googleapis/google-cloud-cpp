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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BENCHMARKS_AGGREGATE_UPLOAD_THROUGHPUT_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BENCHMARKS_AGGREGATE_UPLOAD_THROUGHPUT_OPTIONS_H

#include "google/cloud/storage/benchmarks/benchmark_utils.h"
#include <cstdint>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace storage_benchmarks {

struct AggregateUploadThroughputOptions {
  std::string labels;
  std::string bucket_name;
  std::string object_prefix;
  int object_count = 1000;
  std::int64_t minimum_object_size = 64 * kMiB;
  std::int64_t maximum_object_size = 64 * kMiB;
  std::int64_t resumable_upload_chunk_size = 64 * kMiB;
  int thread_count = 1;
  int iteration_count = 1;
  std::string api = "JSON";
  bool client_per_thread = false;
  Options client_options;
  bool exit_after_parse = false;
};

google::cloud::StatusOr<AggregateUploadThroughputOptions>
ParseAggregateUploadThroughputOptions(std::vector<std::string> const& argv,
                                      std::string const& description);

}  // namespace storage_benchmarks
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BENCHMARKS_AGGREGATE_UPLOAD_THROUGHPUT_OPTIONS_H
