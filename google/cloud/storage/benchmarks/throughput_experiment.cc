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

#include "google/cloud/storage/benchmarks/throughput_experiment.h"
#include "google/cloud/storage/benchmarks/benchmark_utils.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/grpc_plugin.h"
#include "absl/memory/memory.h"
#include <vector>

namespace google {
namespace cloud {
namespace storage_benchmarks {

namespace gcs = google::cloud::storage;

namespace {

class UploadObject : public ThroughputExperiment {
 public:
  explicit UploadObject(google::cloud::storage::Client client, ApiName api,
                        std::string random_data, bool prefer_insert)
      : client_(std::move(client)),
        api_(api),
        random_data_(std::move(random_data)),
        prefer_insert_(prefer_insert) {}
  ~UploadObject() override = default;

  ThroughputResult Run(std::string const& bucket_name,
                       std::string const& object_name,
                       ThroughputExperimentConfig const& config) override {
    auto api_selector = gcs::Fields();
    if (api_ == ApiName::kApiXml) {
      // The default API is JSON, we force XML by not using features that XML
      // does not implement:
      api_selector = gcs::Fields("");
    }

    std::vector<char> buffer(config.app_buffer_size);

    SimpleTimer timer;
    timer.Start();
    // When the object is relatively small using `ObjectInsert` might be more
    // efficient. Randomly select about 1/2 of the small writes to use
    // ObjectInsert()
    if (static_cast<std::size_t>(config.object_size) < random_data_.size() &&
        prefer_insert_) {
      std::string data = random_data_.substr(0, config.object_size);
      auto object_metadata = client_.InsertObject(
          bucket_name, object_name, std::move(data),
          gcs::DisableCrc32cChecksum(!config.enable_crc32c),
          gcs::DisableMD5Hash(!config.enable_md5), api_selector);
      return ThroughputResult{kOpInsert,
                              config.object_size,
                              config.app_buffer_size,
                              config.lib_buffer_size,
                              config.enable_crc32c,
                              config.enable_md5,
                              api_,
                              timer.elapsed_time(),
                              timer.cpu_time(),
                              object_metadata.status().code()};
    }
    auto writer = client_.WriteObject(
        bucket_name, object_name,
        gcs::DisableCrc32cChecksum(!config.enable_crc32c),
        gcs::DisableMD5Hash(!config.enable_md5), api_selector);
    for (std::int64_t offset = 0; offset < config.object_size;
         offset += config.app_buffer_size) {
      auto len = config.app_buffer_size;
      if (offset + len > config.object_size) {
        len = config.object_size - offset;
      }
      writer.write(random_data_.data(), len);
    }
    writer.Close();
    timer.Stop();

    return ThroughputResult{kOpWrite,
                            config.object_size,
                            config.app_buffer_size,
                            config.lib_buffer_size,
                            config.enable_crc32c,
                            config.enable_md5,
                            api_,
                            timer.elapsed_time(),
                            timer.cpu_time(),
                            writer.metadata().status().code()};
  }

 private:
  google::cloud::storage::Client client_;
  ApiName api_;
  std::string random_data_;
  bool prefer_insert_;
};

/**
 * Download objects using the GCS client.
 */
class DownloadObject : public ThroughputExperiment {
 public:
  explicit DownloadObject(google::cloud::storage::Client client, ApiName api)
      : client_(std::move(client)), api_(api) {}
  ~DownloadObject() override = default;

  ThroughputResult Run(std::string const& bucket_name,
                       std::string const& object_name,
                       ThroughputExperimentConfig const& config) override {
    auto api_selector = gcs::IfGenerationNotMatch();
    if (api_ == ApiName::kApiJson) {
      // The default API is XML, we force JSON by using a feature not available
      // in XML.
      api_selector = gcs::IfGenerationNotMatch(0);
    }

    std::vector<char> buffer(config.app_buffer_size);

    SimpleTimer timer;
    timer.Start();
    auto reader = client_.ReadObject(
        bucket_name, object_name,
        gcs::DisableCrc32cChecksum(!config.enable_crc32c),
        gcs::DisableMD5Hash(!config.enable_md5), api_selector);
    for (size_t num_read = 0; reader.read(buffer.data(), buffer.size());
         num_read += reader.gcount()) {
    }
    timer.Stop();
    return ThroughputResult{config.op,
                            config.object_size,
                            config.app_buffer_size,
                            config.lib_buffer_size,
                            config.enable_crc32c,
                            config.enable_md5,
                            api_,
                            timer.elapsed_time(),
                            timer.cpu_time(),
                            reader.status().code()};
  }

 private:
  google::cloud::storage::Client client_;
  ApiName api_;
};
}  // namespace

std::vector<std::unique_ptr<ThroughputExperiment>> CreateUploadExperiments(
    ThroughputOptions const& options,
    google::cloud::storage::ClientOptions const& client_options) {
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto contents = MakeRandomData(generator, options.maximum_write_size);
  gcs::Client rest_client(client_options);

#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
  gcs::Client grpc_client =
      google::cloud::storage_experimental::DefaultGrpcClient(client_options);
#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC

  std::vector<std::unique_ptr<ThroughputExperiment>> result;
  for (auto a : options.enabled_apis) {
    switch (a) {
#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
      case ApiName::kApiGrpc:
        result.push_back(
            absl::make_unique<UploadObject>(grpc_client, a, contents, false));
        result.push_back(
            absl::make_unique<UploadObject>(grpc_client, a, contents, true));
        break;
#else
      case ApiName::kApiGrpc:
        break;
#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
      case ApiName::kApiXml:
      case ApiName::kApiJson:
        result.push_back(
            absl::make_unique<UploadObject>(rest_client, a, contents, false));
        result.push_back(
            absl::make_unique<UploadObject>(rest_client, a, contents, true));
        break;
    }
  }
  return result;
}

std::vector<std::unique_ptr<ThroughputExperiment>> CreateDownloadExperiments(
    ThroughputOptions const& options,
    google::cloud::storage::ClientOptions const& client_options) {
  gcs::Client rest_client(client_options);

#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
  gcs::Client grpc_client =
      google::cloud::storage_experimental::DefaultGrpcClient(client_options);
#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC

  std::vector<std::unique_ptr<ThroughputExperiment>> result;
  for (auto a : options.enabled_apis) {
    switch (a) {
#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
      case ApiName::kApiGrpc:
        result.push_back(absl::make_unique<DownloadObject>(grpc_client, a));
        break;
#else
      case ApiName::kApiGrpc:
        break;
#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
      case ApiName::kApiXml:
      case ApiName::kApiJson:
        result.push_back(absl::make_unique<DownloadObject>(rest_client, a));
        break;
    }
  }
  return result;
}

}  // namespace storage_benchmarks
}  // namespace cloud
}  // namespace google
