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
#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
#include "google/cloud/storage/internal/grpc_client.h"
#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
#include "google/cloud/grpc_error_delegate.h"
#include "absl/memory/memory.h"
#include <google/storage/v1/storage.grpc.pb.h>
#include <curl/curl.h>
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

    // When the object is relatively small using `ObjectInsert` might be more
    // efficient. Randomly select about 1/2 of the small writes to use
    // ObjectInsert()
    if (static_cast<std::size_t>(config.object_size) < random_data_.size() &&
        prefer_insert_) {
      SimpleTimer timer;
      timer.Start();
      std::string data =
          random_data_.substr(0, static_cast<std::size_t>(config.object_size));
      auto object_metadata = client_.InsertObject(
          bucket_name, object_name, std::move(data),
          gcs::DisableCrc32cChecksum(!config.enable_crc32c),
          gcs::DisableMD5Hash(!config.enable_md5), api_selector);
      timer.Stop();
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
    SimpleTimer timer;
    timer.Start();
    auto writer = client_.WriteObject(
        bucket_name, object_name,
        gcs::DisableCrc32cChecksum(!config.enable_crc32c),
        gcs::DisableMD5Hash(!config.enable_md5), api_selector);
    for (std::int64_t offset = 0; offset < config.object_size;
         offset += config.app_buffer_size) {
      auto len = config.app_buffer_size;
      if (offset + static_cast<std::int64_t>(len) > config.object_size) {
        len = static_cast<std::size_t>(config.object_size - offset);
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
    for (std::uint64_t num_read = 0; reader.read(buffer.data(), buffer.size());
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

extern "C" std::size_t OnWrite(char* src, size_t size, size_t nmemb, void* d) {
  auto& buffer = *reinterpret_cast<std::vector<char>*>(d);
  std::memcpy(buffer.data(), src, size * nmemb);
  return size * nmemb;
}

extern "C" std::size_t OnHeader(char*, std::size_t size, std::size_t nitems,
                                void*) {
  return size * nitems;
}

class DownloadObjectLibcurl : public ThroughputExperiment {
 public:
  explicit DownloadObjectLibcurl(ApiName api)
      : creds_(
            google::cloud::storage::oauth2::GoogleDefaultCredentials().value()),
        api_(api) {}
  ~DownloadObjectLibcurl() override = default;

  ThroughputResult Run(std::string const& bucket_name,
                       std::string const& object_name,
                       ThroughputExperimentConfig const& config) override {
    auto header = creds_->AuthorizationHeader();
    if (!header) return {};

    SimpleTimer timer;
    timer.Start();
    struct curl_slist* slist1 = nullptr;
    slist1 = curl_slist_append(slist1, header->c_str());

    auto* hnd = curl_easy_init();
    curl_easy_setopt(hnd, CURLOPT_BUFFERSIZE, 102400L);
    std::string url;
    if (api_ == ApiName::kApiRawXml) {
      url = "https://storage.googleapis.com/" + bucket_name + "/" + object_name;
    } else {
      // For this benchmark it is not necessary to URL escape the object name.
      url = "https://storage.googleapis.com/storage/v1/b/" + bucket_name +
            "/o/" + object_name + "?alt=media";
    }
    curl_easy_setopt(hnd, CURLOPT_URL, url.c_str());
    curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, slist1);
    curl_easy_setopt(hnd, CURLOPT_USERAGENT, "curl/7.65.3");
    curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
    curl_easy_setopt(hnd, CURLOPT_HTTP09_ALLOWED, 1L);
    curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);

    std::vector<char> buffer(CURL_MAX_WRITE_SIZE);
    curl_easy_setopt(hnd, CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, &OnWrite);
    curl_easy_setopt(hnd, CURLOPT_HEADERDATA, nullptr);
    curl_easy_setopt(hnd, CURLOPT_HEADERFUNCTION, &OnHeader);

    std::vector<char> error_buffer(2 * CURL_ERROR_SIZE);
    curl_easy_setopt(hnd, CURLOPT_ERRORBUFFER, error_buffer.data());

    curl_easy_setopt(hnd, CURLOPT_STDERR, stderr);
    CURLcode ret = curl_easy_perform(hnd);
    auto status_code = ret == CURLE_OK ? google::cloud::StatusCode::kOk
                                       : google::cloud::StatusCode::kUnknown;

    curl_easy_cleanup(hnd);
    curl_slist_free_all(slist1);
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
                            status_code};
  }

 private:
  std::shared_ptr<google::cloud::storage::oauth2::Credentials> creds_;
  ApiName api_;
};

std::shared_ptr<grpc::ChannelInterface> CreateGcsChannel() {
#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
  gcs::ClientOptions options{gcs::oauth2::GoogleDefaultCredentials().value()};
  return gcs::internal::CreateGrpcChannel(options);
#else
  return grpc::CreateChannel("storage.googleapis.com",
                             grpc::GoogleDefaultCredentials());
#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
}

class DownloadObjectRawGrpc : public ThroughputExperiment {
 public:
  DownloadObjectRawGrpc()
      : stub_(google::storage::v1::Storage::NewStub(CreateGcsChannel())) {}
  ~DownloadObjectRawGrpc() override = default;

  ThroughputResult Run(std::string const& bucket_name,
                       std::string const& object_name,
                       ThroughputExperimentConfig const& config) override {
    SimpleTimer timer;

    timer.Start();
    google::storage::v1::GetObjectMediaRequest request;
    request.set_bucket(bucket_name);
    request.set_object(object_name);
    grpc::ClientContext context;
    auto stream = stub_->GetObjectMedia(&context, request);
    google::storage::v1::GetObjectMediaResponse response;
    std::int64_t bytes_received = 0;
    while (stream->Read(&response)) {
      if (response.has_checksummed_data()) {
        bytes_received += response.checksummed_data().content().size();
      }
    }
    auto const status =
        ::google::cloud::MakeStatusFromRpcError(stream->Finish());
    timer.Stop();

    return ThroughputResult{config.op,
                            config.object_size,
                            config.app_buffer_size,
                            /*lib_buffer_size=*/0,
                            /*crc_enabled=*/false,
                            /*md5_enabled=*/false,
                            ApiName::kApiRawGrpc,
                            timer.elapsed_time(),
                            timer.cpu_time(),
                            status.code()};
  }

 private:
  std::unique_ptr<google::storage::v1::Storage::StubInterface> stub_;
};

}  // namespace

std::vector<std::unique_ptr<ThroughputExperiment>> CreateUploadExperiments(
    ThroughputOptions const& options,
    google::cloud::storage::ClientOptions const& client_options) {
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto contents = MakeRandomData(generator, options.maximum_write_size);
  gcs::Client rest_client(client_options);

  std::vector<std::unique_ptr<ThroughputExperiment>> result;
  for (auto a : options.enabled_apis) {
    switch (a) {
#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
      case ApiName::kApiGrpc:
      case ApiName::kApiRawGrpc: {
        gcs::Client grpc_client =
            google::cloud::storage_experimental::DefaultGrpcClient(
                client_options);
        result.push_back(
            absl::make_unique<UploadObject>(grpc_client, a, contents, false));
        result.push_back(
            absl::make_unique<UploadObject>(grpc_client, a, contents, true));
      }
      // this comment keeps clang-format from merging to previous line
      // we thing `} break;` is just weird.
      break;
#else
      case ApiName::kApiGrpc:
      case ApiName::kApiRawGrpc:
        break;
#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
      case ApiName::kApiXml:
      case ApiName::kApiJson:
      case ApiName::kApiRawJson:
      case ApiName::kApiRawXml:
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

  std::vector<std::unique_ptr<ThroughputExperiment>> result;
  for (auto a : options.enabled_apis) {
    switch (a) {
#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
      case ApiName::kApiGrpc:
        result.push_back(absl::make_unique<DownloadObject>(
            google::cloud::storage_experimental::DefaultGrpcClient(
                client_options),
            a));
        break;
#else
      case ApiName::kApiGrpc:
        break;
#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
      case ApiName::kApiXml:
      case ApiName::kApiJson:
        result.push_back(absl::make_unique<DownloadObject>(rest_client, a));
        break;
      case ApiName::kApiRawXml:
      case ApiName::kApiRawJson:
        result.push_back(absl::make_unique<DownloadObjectLibcurl>(a));
        break;
      case ApiName::kApiRawGrpc:
        result.push_back(absl::make_unique<DownloadObjectRawGrpc>());
        break;
    }
  }
  return result;
}

}  // namespace storage_benchmarks
}  // namespace cloud
}  // namespace google
