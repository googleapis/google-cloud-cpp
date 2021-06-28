// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BENCHMARKS_THROUGHPUT_EXPERIMENT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BENCHMARKS_THROUGHPUT_EXPERIMENT_H

#include "google/cloud/storage/benchmarks/throughput_options.h"
#include "google/cloud/storage/benchmarks/throughput_result.h"
#include <memory>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace storage_benchmarks {

struct ThroughputExperimentConfig {
  OpType op;
  std::int64_t object_size;
  std::size_t app_buffer_size;
  std::size_t lib_buffer_size;
  bool enable_crc32c;
  bool enable_md5;
};

/**
 * Run a single experiment in a throughput benchmark.
 *
 * Throughput benchmarks typically repeat the same "experiment" multiple times,
 * sometimes choosing at random which experiment to run, and which parameters to
 * use. An experiment might be "upload an object using XML" or "download an
 * an object using raw libcurl calls".
 */
class ThroughputExperiment {
 public:
  virtual ~ThroughputExperiment() = default;

  virtual ThroughputResult Run(std::string const& bucket_name,
                               std::string const& object_name,
                               ThroughputExperimentConfig const& config) = 0;
};

/**
 * Create the list of upload experiments based on the @p options.
 */
std::vector<std::unique_ptr<ThroughputExperiment>> CreateUploadExperiments(
    ThroughputOptions const& options,
    google::cloud::storage::Client rest_client,
    google::cloud::storage::Client grpc_client,
    google::cloud::Options const& client_options);

/**
 * Create the list of download experiments based on the @p options.
 *
 * Some benchmarks need to distinguish upload vs. download experiments because
 * they depend on the upload experiment to create the objects to be downloaded.
 */
std::vector<std::unique_ptr<ThroughputExperiment>> CreateDownloadExperiments(
    ThroughputOptions const& options,
    google::cloud::storage::Client rest_client,
    google::cloud::storage::Client grpc_client,
    google::cloud::Options const& client_options, int thread_id);

}  // namespace storage_benchmarks
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BENCHMARKS_THROUGHPUT_EXPERIMENT_H
