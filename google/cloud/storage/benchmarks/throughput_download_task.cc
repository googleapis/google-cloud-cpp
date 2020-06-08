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

#include "google/cloud/storage/benchmarks/throughput_download_task.h"
#include "google/cloud/storage/benchmarks/benchmark_utils.h"
#include "google/cloud/storage/client.h"
#include <vector>

namespace google {
namespace cloud {
namespace storage_benchmarks {

namespace gcs = google::cloud::storage;

ThroughputResult ClientDownloadTask::PerformDownload(
    std::string const& bucket_name, std::string const& object_name,
    DownloadConfig const& config) {
  auto json_read_selector = gcs::IfGenerationNotMatch();
  if (api_ == ApiName::kApiJson) {
    // The default API is XML, we force JSON by using a feature not available
    // in XML.
    json_read_selector = gcs::IfGenerationNotMatch(0);
  }

  std::vector<char> buffer(config.read_size);

  SimpleTimer timer;
  timer.Start();
  auto reader = client_.ReadObject(
      bucket_name, object_name,
      gcs::DisableCrc32cChecksum(!config.enable_crc32c),
      gcs::DisableMD5Hash(!config.enable_md5), json_read_selector);
  for (size_t num_read = 0; reader.read(buffer.data(), buffer.size());
       num_read += reader.gcount()) {
  }
  timer.Stop();
  return ThroughputResult{config.op,
                          config.object_size,
                          config.read_size,
                          config.download_buffer_size,
                          config.enable_crc32c,
                          config.enable_md5,
                          api_,
                          timer.elapsed_time(),
                          timer.cpu_time(),
                          reader.status().code()};
}

}  // namespace storage_benchmarks
}  // namespace cloud
}  // namespace google
