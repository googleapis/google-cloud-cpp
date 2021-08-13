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
#include "google/cloud/internal/getenv.h"
#include "absl/algorithm/container.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_split.h"
#include <google/storage/v1/storage.grpc.pb.h>
#include <curl/curl.h>
#include <vector>

namespace google {
namespace cloud {
namespace storage_benchmarks {

namespace gcs = ::google::cloud::storage;

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
      auto timer = Timer::PerThread();
      std::string data =
          random_data_.substr(0, static_cast<std::size_t>(config.object_size));
      auto object_metadata = client_.InsertObject(
          bucket_name, object_name, std::move(data),
          gcs::DisableCrc32cChecksum(!config.enable_crc32c),
          gcs::DisableMD5Hash(!config.enable_md5), api_selector);
      auto const usage = timer.Sample();
      return ThroughputResult{kOpInsert,
                              config.object_size,
                              config.app_buffer_size,
                              config.lib_buffer_size,
                              config.enable_crc32c,
                              config.enable_md5,
                              api_,
                              usage.elapsed_time,
                              usage.cpu_time,
                              object_metadata.status()};
    }
    auto timer = Timer::PerThread();
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
    auto const usage = timer.Sample();

    return ThroughputResult{kOpWrite,
                            config.object_size,
                            config.app_buffer_size,
                            config.lib_buffer_size,
                            config.enable_crc32c,
                            config.enable_md5,
                            api_,
                            usage.elapsed_time,
                            usage.cpu_time,
                            writer.metadata().status()};
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

    auto timer = Timer::PerThread();
    auto reader = client_.ReadObject(
        bucket_name, object_name,
        gcs::DisableCrc32cChecksum(!config.enable_crc32c),
        gcs::DisableMD5Hash(!config.enable_md5), api_selector);
    for (std::uint64_t num_read = 0; reader.read(buffer.data(), buffer.size());
         num_read += reader.gcount()) {
    }
    auto const usage = timer.Sample();
    return ThroughputResult{config.op,
                            config.object_size,
                            config.app_buffer_size,
                            config.lib_buffer_size,
                            config.enable_crc32c,
                            config.enable_md5,
                            api_,
                            usage.elapsed_time,
                            usage.cpu_time,
                            reader.status()};
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

    auto timer = Timer::PerThread();
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
    curl_easy_setopt(hnd, CURLOPT_USERAGENT, curl_version());
    curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
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
    Status status = ret == CURLE_OK
                        ? Status{}
                        : Status{StatusCode::kUnknown, "curl failed"};

    curl_easy_cleanup(hnd);
    curl_slist_free_all(slist1);
    auto const usage = timer.Sample();
    return ThroughputResult{config.op,
                            config.object_size,
                            config.app_buffer_size,
                            config.lib_buffer_size,
                            config.enable_crc32c,
                            config.enable_md5,
                            api_,
                            usage.elapsed_time,
                            usage.cpu_time,
                            status};
  }

 private:
  std::shared_ptr<google::cloud::storage::oauth2::Credentials> creds_;
  ApiName api_;
};

auto constexpr kDirectPathConfig = R"json({
    "loadBalancingConfig": [{
      "grpclb": {
        "childPolicy": [{
          "pick_first": {}
        }]
      }
    }]
  })json";

bool DirectPathEnabled() {
  auto const direct_path_settings =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_ENABLE_DIRECT_PATH")
          .value_or("");
  return absl::c_any_of(absl::StrSplit(direct_path_settings, ','),
                        [](absl::string_view v) { return v == "storage"; });
}

std::shared_ptr<grpc::ChannelInterface> CreateGcsChannel(int thread_id) {
  grpc::ChannelArguments args;
  args.SetInt("grpc.channel_id", thread_id);
  if (DirectPathEnabled()) {
    args.SetServiceConfigJSON(kDirectPathConfig);
  }
  return grpc::CreateCustomChannel("storage.googleapis.com",
                                   grpc::GoogleDefaultCredentials(), args);
}

class DownloadObjectRawGrpc : public ThroughputExperiment {
 public:
  explicit DownloadObjectRawGrpc(int thread_id)
      : stub_(google::storage::v1::Storage::NewStub(
            CreateGcsChannel(thread_id))) {}
  ~DownloadObjectRawGrpc() override = default;

  ThroughputResult Run(std::string const& bucket_name,
                       std::string const& object_name,
                       ThroughputExperimentConfig const& config) override {
    auto timer = Timer::PerThread();
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
    auto const usage = timer.Sample();

    return ThroughputResult{config.op,
                            config.object_size,
                            config.app_buffer_size,
                            /*lib_buffer_size=*/0,
                            /*crc_enabled=*/false,
                            /*md5_enabled=*/false,
                            ApiName::kApiRawGrpc,
                            usage.elapsed_time,
                            usage.cpu_time,
                            status};
  }

 private:
  std::unique_ptr<google::storage::v1::Storage::StubInterface> stub_;
};

}  // namespace

using ::google::cloud::storage_experimental::DefaultGrpcClient;

std::vector<std::unique_ptr<ThroughputExperiment>> CreateUploadExperiments(
    ThroughputOptions const& options,
    google::cloud::storage::Client rest_client,
    google::cloud::storage::Client grpc_client,
    google::cloud::Options const& client_options) {
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto contents = MakeRandomData(generator, options.maximum_write_size);

  if (options.client_per_thread) rest_client = gcs::Client(client_options);
#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
  if (options.client_per_thread) {
    grpc_client = DefaultGrpcClient(client_options);
  }
#else
  if (options.client_per_thread) grpc_client = gcs::Client(client_options);
#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC

  std::vector<std::unique_ptr<ThroughputExperiment>> result;
  for (auto a : options.enabled_apis) {
    switch (a) {
      case ApiName::kApiGrpc:
      case ApiName::kApiRawGrpc:
        result.push_back(
            absl::make_unique<UploadObject>(grpc_client, a, contents, false));
        result.push_back(
            absl::make_unique<UploadObject>(grpc_client, a, contents, true));
        break;
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
    google::cloud::storage::Client rest_client,
    google::cloud::storage::Client grpc_client,
    google::cloud::Options const& client_options, int thread_id) {
  if (options.client_per_thread) rest_client = gcs::Client(client_options);
#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
  if (options.client_per_thread) {
    grpc_client = DefaultGrpcClient(client_options);
  }
#else
  if (options.client_per_thread) grpc_client = gcs::Client(client_options);
#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC

  std::vector<std::unique_ptr<ThroughputExperiment>> result;
  for (auto a : options.enabled_apis) {
    switch (a) {
      case ApiName::kApiGrpc:
        result.push_back(absl::make_unique<DownloadObject>(grpc_client, a));
        break;
      case ApiName::kApiXml:
      case ApiName::kApiJson:
        result.push_back(absl::make_unique<DownloadObject>(rest_client, a));
        break;
      case ApiName::kApiRawXml:
      case ApiName::kApiRawJson:
        result.push_back(absl::make_unique<DownloadObjectLibcurl>(a));
        break;
      case ApiName::kApiRawGrpc:
        result.push_back(absl::make_unique<DownloadObjectRawGrpc>(thread_id));
        break;
    }
  }
  return result;
}

}  // namespace storage_benchmarks
}  // namespace cloud
}  // namespace google
