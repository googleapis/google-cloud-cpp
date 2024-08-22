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

#include "google/cloud/storage/benchmarks/throughput_experiment.h"
#include "google/cloud/storage/benchmarks/benchmark_utils.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/internal/make_status.h"
#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
#include "google/cloud/storage/internal/grpc/ctype_cord_workaround.h"
#include "google/cloud/grpc_error_delegate.h"
#include <google/storage/v2/storage.grpc.pb.h>
#include <memory>
#include <random>
#include <string>
#include <utility>
#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
#include <curl/curl.h>
#include <iterator>
#include <vector>

namespace google {
namespace cloud {
namespace storage_benchmarks {

namespace gcs = ::google::cloud::storage;

namespace {

std::string ExtractPeer(
    std::multimap<std::string, std::string> const& headers) {
  auto p = headers.find(":grpc-context-peer");
  if (p == headers.end()) {
    p = headers.find(":curl-peer");
  }
  return p == headers.end() ? std::string{"[peer-unknown]"} : p->second;
}

std::string ExtractRetryCount(
    std::multimap<std::string, std::string> const& headers) {
  auto const range = headers.equal_range(":retry-count");
  auto const d = std::distance(range.first, range.second);
  if (d == 0) return "[retry-count-unknown]";
  return std::next(range.first, d - 1)->second;
}

std::string ExtractUploadId(std::string v) {
  auto constexpr kRestField = "upload_id=";
  auto const pos = v.find(kRestField);
  if (pos == std::string::npos) return v;
  return v.substr(pos + std::strlen(kRestField));
}

class ResumableUpload : public ThroughputExperiment {
 public:
  explicit ResumableUpload(google::cloud::storage::Client client,
                           ExperimentTransport transport,
                           std::shared_ptr<std::string> random_data)
      : client_(std::move(client)),
        transport_(transport),
        random_data_(std::move(random_data)) {}
  ~ResumableUpload() override = default;

  ThroughputResult Run(std::string const& bucket_name,
                       std::string const& object_name,
                       ThroughputExperimentConfig const& config) override {
    std::vector<char> buffer(config.app_buffer_size);

    auto const start = std::chrono::system_clock::now();
    auto timer = Timer::PerThread();
    auto writer =
        client_.WriteObject(bucket_name, object_name,
                            gcs::DisableCrc32cChecksum(!config.enable_crc32c),
                            gcs::DisableMD5Hash(!config.enable_md5));
    auto upload_id = ExtractUploadId(writer.resumable_session_id());
    for (std::int64_t offset = 0; offset < config.object_size;
         offset += config.app_buffer_size) {
      auto len = config.app_buffer_size;
      if (offset + static_cast<std::int64_t>(len) > config.object_size) {
        len = static_cast<std::size_t>(config.object_size - offset);
      }
      writer.write(random_data_->data(), len);
    }
    writer.Close();
    auto const usage = timer.Sample();
    auto generation = writer.metadata()
                          ? std::to_string(writer.metadata()->generation())
                          : std::string{};

    return ThroughputResult{start,
                            ExperimentLibrary::kCppClient,
                            transport_,
                            kOpWrite,
                            config.object_size,
                            /*transfer_offset=*/0,
                            config.object_size,
                            config.app_buffer_size,
                            config.enable_crc32c,
                            config.enable_md5,
                            usage.elapsed_time,
                            usage.cpu_time,
                            writer.metadata().status(),
                            ExtractPeer(writer.headers()),
                            bucket_name,
                            object_name,
                            std::move(generation),
                            std::move(upload_id),
                            ExtractRetryCount(writer.headers())};
  }

 private:
  google::cloud::storage::Client client_;
  ExperimentTransport transport_;
  std::shared_ptr<std::string> random_data_;
};

class SimpleUpload : public ThroughputExperiment {
 public:
  explicit SimpleUpload(google::cloud::storage::Client client,
                        ExperimentTransport transport,
                        std::shared_ptr<std::string> random_data)
      : client_(std::move(client)),
        transport_(transport),
        random_data_(std::move(random_data)),
        fallback_(client_, transport_, random_data_) {}
  ~SimpleUpload() override = default;

  ThroughputResult Run(std::string const& bucket_name,
                       std::string const& object_name,
                       ThroughputExperimentConfig const& config) override {
    // If the requested object is too large, fall back on resumable uploads.
    if (static_cast<std::size_t>(config.object_size) > random_data_->size()) {
      return fallback_.Run(bucket_name, object_name, config);
    }

    // Only relatively small objects can be uploaded using `InsertObject()`, so
    // truncate the object to the right size.
    auto const start = std::chrono::system_clock::now();
    auto timer = Timer::PerThread();
    auto data = absl::string_view{*random_data_}.substr(
        0, static_cast<std::size_t>(config.object_size));
    auto object_metadata =
        client_.InsertObject(bucket_name, object_name, data,
                             gcs::DisableCrc32cChecksum(!config.enable_crc32c),
                             gcs::DisableMD5Hash(!config.enable_md5));
    auto const usage = timer.Sample();
    auto generation = object_metadata
                          ? std::to_string(object_metadata->generation())
                          : std::string{};
    return ThroughputResult{start,
                            ExperimentLibrary::kCppClient,
                            transport_,
                            kOpInsert,
                            config.object_size,
                            /*transfer_offset=*/0,
                            config.object_size,
                            config.app_buffer_size,
                            config.enable_crc32c,
                            config.enable_md5,
                            usage.elapsed_time,
                            usage.cpu_time,
                            object_metadata.status(),
                            "[peer-N/A]",
                            bucket_name,
                            object_name,
                            std::move(generation),
                            "[upload-id-N/A]",
                            "[retry-count-unknown]"};
  }

 private:
  google::cloud::storage::Client client_;
  ExperimentTransport transport_;
  std::shared_ptr<std::string> random_data_;
  ResumableUpload fallback_;
};

/**
 * Download objects using the GCS client.
 */
class DownloadObject : public ThroughputExperiment {
 public:
  explicit DownloadObject(google::cloud::storage::Client client,
                          ExperimentTransport transport)
      : client_(std::move(client)), transport_(transport) {}
  ~DownloadObject() override = default;

  ThroughputResult Run(std::string const& bucket_name,
                       std::string const& object_name,
                       ThroughputExperimentConfig const& config) override {
    std::vector<char> buffer(config.app_buffer_size);

    auto const start = std::chrono::system_clock::now();
    auto timer = Timer::PerThread();
    auto const offset = config.read_range.value_or(std::make_pair(0, 0)).first;
    auto read_range =
        config.read_range.has_value()
            ? gcs::ReadRange(offset, offset + config.read_range->second)
            : gcs::ReadRange();
    auto reader =
        client_.ReadObject(bucket_name, object_name, read_range,
                           gcs::DisableCrc32cChecksum(!config.enable_crc32c),
                           gcs::DisableMD5Hash(!config.enable_md5));
    std::int64_t transfer_size = 0;
    while (!reader.eof() && !reader.bad()) {
      reader.read(buffer.data(), buffer.size());
      transfer_size += reader.gcount();
    }
    auto const usage = timer.Sample();
    return ThroughputResult{start,
                            ExperimentLibrary::kCppClient,
                            transport_,
                            config.op,
                            config.object_size,
                            offset,
                            transfer_size,
                            config.app_buffer_size,
                            config.enable_crc32c,
                            config.enable_md5,
                            usage.elapsed_time,
                            usage.cpu_time,
                            reader.status(),
                            ExtractPeer(reader.headers()),
                            bucket_name,
                            object_name,
                            std::to_string(reader.generation().value_or(-1)),
                            "[upload-id-N/A]",
                            ExtractRetryCount(reader.headers())};
  }

 private:
  google::cloud::storage::Client client_;
  ExperimentTransport transport_;
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
  explicit DownloadObjectLibcurl(ThroughputOptions const& options)
      : endpoint_(options.rest_options.get<gcs::RestEndpointOption>()),
        target_api_version_path_(
            options.rest_options.get<gcs::internal::TargetApiVersionOption>()),
        creds_(google::cloud::storage::oauth2::GoogleDefaultCredentials()
                   .value()) {
    if (target_api_version_path_.empty()) {
      target_api_version_path_ = "v1";
    }
  }
  ~DownloadObjectLibcurl() override = default;

  ThroughputResult Run(std::string const& bucket_name,
                       std::string const& object_name,
                       ThroughputExperimentConfig const& config) override {
    auto header = creds_->AuthorizationHeader();
    if (!header) return {};

    auto const start = std::chrono::system_clock::now();
    auto timer = Timer::PerThread();
    struct curl_slist* slist1 = nullptr;
    slist1 = curl_slist_append(slist1, header->c_str());

    auto* hnd = curl_easy_init();
    curl_easy_setopt(hnd, CURLOPT_BUFFERSIZE, 102400L);
    // For this benchmark it is not necessary to URL escape the object name.
    auto const url = endpoint_ + "/storage/" + target_api_version_path_ +
                     "/b/" + bucket_name + "/o/" + object_name + "?alt=media";

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
    Status status = ret == CURLE_OK ? Status{}
                                    : google::cloud::internal::UnknownError(
                                          "curl failed", GCP_ERROR_INFO());
    auto peer = [&] {
      char* ip = nullptr;
      auto e = curl_easy_getinfo(hnd, CURLINFO_PRIMARY_IP, &ip);
      if (e == CURLE_OK && ip != nullptr) return std::string{ip};
      return std::string{"[error-fetching-peer]"};
    }();

    curl_easy_cleanup(hnd);
    curl_slist_free_all(slist1);
    auto const usage = timer.Sample();
    return ThroughputResult{start,
                            ExperimentLibrary::kRaw,
                            ExperimentTransport::kJson,
                            config.op,
                            config.object_size,
                            /*transfer_offset=*/0,
                            config.object_size,
                            config.app_buffer_size,
                            config.enable_crc32c,
                            config.enable_md5,
                            usage.elapsed_time,
                            usage.cpu_time,
                            status,
                            std::move(peer),
                            bucket_name,
                            object_name,
                            "[generation-N/A]",
                            "[upload-id-N/A]",
                            "[retry-count-N/A]"};
  }

 private:
  std::string endpoint_;
  std::string target_api_version_path_;
  std::shared_ptr<google::cloud::storage::oauth2::Credentials> creds_;
};

#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
std::shared_ptr<grpc::ChannelInterface> CreateGcsChannel(
    ThroughputOptions const& options, int thread_id,
    ExperimentTransport transport) {
  grpc::ChannelArguments args;
  args.SetInt("grpc.channel_id", thread_id);
  if (transport == ExperimentTransport::kGrpc) {
    return grpc::CreateCustomChannel(options.grpc_options.get<EndpointOption>(),
                                     grpc::GoogleDefaultCredentials(), args);
  }
  return grpc::CreateCustomChannel(
      options.direct_path_options.get<EndpointOption>(),
      grpc::GoogleDefaultCredentials(), args);
}

class DownloadObjectRawGrpc : public ThroughputExperiment {
 public:
  explicit DownloadObjectRawGrpc(ThroughputOptions const& options,
                                 int thread_id, ExperimentTransport transport)
      : stub_(google::storage::v2::Storage::NewStub(
            CreateGcsChannel(options, thread_id, transport))),
        transport_(transport) {}
  ~DownloadObjectRawGrpc() override = default;

  ThroughputResult Run(std::string const& bucket_name,
                       std::string const& object_name,
                       ThroughputExperimentConfig const& config) override {
    auto const start = std::chrono::system_clock::now();
    auto timer = Timer::PerThread();
    google::storage::v2::ReadObjectRequest request;
    request.set_bucket(bucket_name);
    request.set_object(object_name);
    grpc::ClientContext context;
    auto stream = stub_->ReadObject(&context, request);
    google::storage::v2::ReadObjectResponse response;
    std::int64_t bytes_received = 0;
    std::string generation = "[generation-N/A]";
    while (stream->Read(&response)) {
      if (response.has_checksummed_data()) {
        bytes_received +=
            storage_internal::GetContent(response.checksummed_data()).size();
      }
      if (response.has_metadata()) {
        generation = std::to_string(response.metadata().generation());
      }
    }
    auto const status =
        ::google::cloud::MakeStatusFromRpcError(stream->Finish());
    auto const usage = timer.Sample();

    return ThroughputResult{start,
                            ExperimentLibrary::kRaw,
                            transport_,
                            config.op,
                            config.object_size,
                            /*transfer_offset=*/0,
                            bytes_received,
                            config.app_buffer_size,
                            /*crc_enabled=*/false,
                            /*md5_enabled=*/false,
                            usage.elapsed_time,
                            usage.cpu_time,
                            status,
                            context.peer(),
                            bucket_name,
                            object_name,
                            generation,
                            "[upload-id-N/A]",
                            "[retry-count-N/A]"};
  }

 private:
  std::unique_ptr<google::storage::v2::Storage::StubInterface> stub_;
  ExperimentTransport transport_;
};
#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC

}  // namespace

std::vector<std::unique_ptr<ThroughputExperiment>> CreateUploadExperiments(
    ThroughputOptions const& options, ClientProvider const& provider) {
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto contents = std::make_shared<std::string>(
      MakeRandomData(generator, options.maximum_write_buffer_size));

  std::vector<std::unique_ptr<ThroughputExperiment>> result;
  for (auto l : options.libs) {
    if (l == ExperimentLibrary::kRaw) continue;
    for (auto t : options.transports) {
      for (auto const& function : options.upload_functions) {
        if (function == "InsertObject") {
          result.push_back(
              std::make_unique<SimpleUpload>(provider(t), t, contents));
        } else /* if (function == "WriteObject") */ {
          result.push_back(
              std::make_unique<ResumableUpload>(provider(t), t, contents));
        }
      }
    }
  }
  return result;
}

std::vector<std::unique_ptr<ThroughputExperiment>> CreateDownloadExperiments(
    ThroughputOptions const& options, ClientProvider const& provider,
    int thread_id) {
  std::vector<std::unique_ptr<ThroughputExperiment>> result;
  for (auto l : options.libs) {
    if (l != ExperimentLibrary::kRaw) {
      for (auto t : options.transports) {
        result.push_back(std::make_unique<DownloadObject>(provider(t), t));
      }
      continue;
    }
    for (auto t : options.transports) {
      if (t != ExperimentTransport::kGrpc &&
          t != ExperimentTransport::kDirectPath) {
        result.push_back(std::make_unique<DownloadObjectLibcurl>(options));
        continue;
      }
#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
      result.push_back(
          std::make_unique<DownloadObjectRawGrpc>(options, thread_id, t));
#else
      (void)thread_id;
#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
    }
  }
  return result;
}

}  // namespace storage_benchmarks
}  // namespace cloud
}  // namespace google
