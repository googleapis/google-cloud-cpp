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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BENCHMARKS_THROUGHPUT_DOWNLOAD_TASK_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BENCHMARKS_THROUGHPUT_DOWNLOAD_TASK_H

#include "google/cloud/storage/benchmarks/throughput_result.h"
#include <chrono>
#include <cstdint>

namespace google {
namespace cloud {
namespace storage_benchmarks {

struct DownloadConfig {
  OpType op;
  std::int64_t object_size;
  std::int64_t read_size;
  std::uint64_t download_buffer_size;
  bool enable_crc32c;
  bool enable_md5;
};

class DownloadTask {
 public:
  virtual ~DownloadTask() = default;

  virtual ThroughputResult PerformDownload(std::string const& bucket_name,
                                           std::string const& object_name,
                                           DownloadConfig const& config) = 0;
};

/**
 * Download objects using the GCS client.
 */
class ClientDownloadTask : public DownloadTask {
 public:
  explicit ClientDownloadTask(google::cloud::storage::Client client,
                              ApiName api)
      : client_(std::move(client)), api_(api) {}
  ~ClientDownloadTask() override = default;

  ThroughputResult PerformDownload(std::string const& bucket_name,
                                   std::string const& object_name,
                                   DownloadConfig const& config) override;

 private:
  google::cloud::storage::Client client_;
  ApiName api_;
};

}  // namespace storage_benchmarks
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BENCHMARKS_THROUGHPUT_DOWNLOAD_TASK_H
