// Copyright 2021 Google Inc.
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_BENCHMARKS_MUTATION_BATCHER_THROUGHPUT_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_BENCHMARKS_MUTATION_BATCHER_THROUGHPUT_OPTIONS_H

#include "google/cloud/status_or.h"
#include <chrono>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable {
namespace benchmarks {

struct MutationBatcherThroughputOptions {
  std::string project_id;
  std::string instance_id;
  std::string table_id;
  std::string column_family = "cf1";
  std::string column = "c1";
  std::chrono::seconds max_time = std::chrono::seconds(0);
  int shard_count = 1;
  int write_thread_count = 1;
  int batcher_thread_count = 1;
  std::int64_t mutation_count = 1000000;
  int max_batches = 10;
  int batch_size = 1000;
  bool exit_after_parse = false;
};

google::cloud::StatusOr<MutationBatcherThroughputOptions>
ParseMutationBatcherThroughputOptions(std::vector<std::string> const& argv,
                                      std::string const& description);

}  // namespace benchmarks
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_BENCHMARKS_MUTATION_BATCHER_THROUGHPUT_OPTIONS_H
