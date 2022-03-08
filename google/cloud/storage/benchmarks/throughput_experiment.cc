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
#include "google/cloud/grpc_error_delegate.h"
#include "absl/algorithm/container.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_split.h"
#include <google/storage/v2/storage.grpc.pb.h>
#include <curl/curl.h>
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
  return p == headers.end() ? std::string{"unknown"} : p->second;
}

class UploadObject : public ThroughputExperiment {
 public:
  explicit UploadObject(google::cloud::storage::Client client,
                        ExperimentTransport transport, std::string random_data,
                        bool prefer_insert)
      : client_(std::move(client)),
        transport_(transport),
        random_data_(std::move(random_data)),
        prefer_insert_(prefer_insert) {}
  ~UploadObject() override = default;

  ThroughputResult Run(std::string const& bucket_name,
                       std::string const& object_name,
                       ThroughputExperimentConfig const& config) override {
    auto api_selector = gcs::Fields();
    if (transport_ == ExperimentTransport::kXml) {
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
      return ThroughputResult{ExperimentLibrary::kCppClient,
                              transport_,
                              kOpInsert,
                              config.object_size,
                              config.object_size,
                              config.app_buffer_size,
                              config.lib_buffer_size,
                              config.enable_crc32c,
                              config.enable_md5,
                              usage.elapsed_time,
                              usage.cpu_time,
                              object_metadata.status(),
                              "[insert-no-peer]"};
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

    return ThroughputResult{ExperimentLibrary::kCppClient,
                            transport_,
                            kOpWrite,
                            config.object_size,
                            config.object_size,
                            config.app_buffer_size,
                            config.lib_buffer_size,
                            config.enable_crc32c,
                            config.enable_md5,
                            usage.elapsed_time,
                            usage.cpu_time,
                            writer.metadata().status(),
                            ExtractPeer(writer.headers())};
  }

 private:
  google::cloud::storage::Client client_;
  ExperimentTransport transport_;
  std::string random_data_;
  bool prefer_insert_;
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
    auto api_selector = gcs::IfGenerationNotMatch();
    if (transport_ == ExperimentTransport::kJson) {
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
    std::int64_t transfer_size = 0;
    while (!reader.eof() && !reader.bad()) {
      reader.read(buffer.data(), buffer.size());
      transfer_size += reader.gcount();
    }
    auto const usage = timer.Sample();
    return ThroughputResult{ExperimentLibrary::kCppClient,
                            transport_,
                            config.op,
                            config.object_size,
                            transfer_size,
                            config.app_buffer_size,
                            config.lib_buffer_size,
                            config.enable_crc32c,
                            config.enable_md5,
                            usage.elapsed_time,
                            usage.cpu_time,
                            reader.status(),
                            ExtractPeer(reader.headers())};
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
  explicit DownloadObjectLibcurl(ThroughputOptions const& options,
                                 ExperimentTransport transport)
      : endpoint_(options.rest_endpoint),
        creds_(
            google::cloud::storage::oauth2::GoogleDefaultCredentials().value()),
        transport_(transport) {}
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
    if (transport_ == ExperimentTransport::kXml) {
      url = endpoint_ + "/" + bucket_name + "/" + object_name;
    } else {
      // For this benchmark it is not necessary to URL escape the object name.
      url = endpoint_ + "/storage/v1/b/" + bucket_name + "/o/" + object_name +
            "?alt=media";
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
    auto peer = [&] {
      char* ip = nullptr;
      auto e = curl_easy_getinfo(hnd, CURLINFO_PRIMARY_IP, &ip);
      if (e == CURLE_OK && ip != nullptr) return std::string{ip};
      return std::string{"[error-fetching-peer]"};
    }();

    curl_easy_cleanup(hnd);
    curl_slist_free_all(slist1);
    auto const usage = timer.Sample();
    return ThroughputResult{ExperimentLibrary::kRaw,
                            ExperimentTransport::kXml,
                            config.op,
                            config.object_size,
                            config.object_size,
                            config.app_buffer_size,
                            config.lib_buffer_size,
                            config.enable_crc32c,
                            config.enable_md5,
                            usage.elapsed_time,
                            usage.cpu_time,
                            status,
                            std::move(peer)};
  }

 private:
  std::string endpoint_;
  std::shared_ptr<google::cloud::storage::oauth2::Credentials> creds_;
  ExperimentTransport transport_;
};

std::shared_ptr<grpc::ChannelInterface> CreateGcsChannel(
    ThroughputOptions const& options, int thread_id,
    ExperimentTransport transport) {
  grpc::ChannelArguments args;
  args.SetInt("grpc.channel_id", thread_id);
  if (transport == ExperimentTransport::kGrpc) {
    return grpc::CreateCustomChannel(options.grpc_endpoint,
                                     grpc::GoogleDefaultCredentials(), args);
  }
  return grpc::CreateCustomChannel(options.direct_path_endpoint,
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
    auto timer = Timer::PerThread();
    google::storage::v2::ReadObjectRequest request;
    request.set_bucket(bucket_name);
    request.set_object(object_name);
    grpc::ClientContext context;
    auto stream = stub_->ReadObject(&context, request);
    google::storage::v2::ReadObjectResponse response;
    std::int64_t bytes_received = 0;
    while (stream->Read(&response)) {
      if (response.has_checksummed_data()) {
        bytes_received += response.checksummed_data().content().size();
      }
    }
    auto const status =
        ::google::cloud::MakeStatusFromRpcError(stream->Finish());
    auto const usage = timer.Sample();

    return ThroughputResult{ExperimentLibrary::kRaw,
                            transport_,
                            config.op,
                            config.object_size,
                            bytes_received,
                            config.app_buffer_size,
                            /*lib_buffer_size=*/0,
                            /*crc_enabled=*/false,
                            /*md5_enabled=*/false,
                            usage.elapsed_time,
                            usage.cpu_time,
                            status,
                            context.peer()};
  }

 private:
  std::unique_ptr<google::storage::v2::Storage::StubInterface> stub_;
  ExperimentTransport transport_;
};

using ::google::cloud::storage::Client;

ClientProvider PerTransport(ClientProvider const& provider) {
  std::map<ExperimentTransport, Client> clients;
  return [clients, provider](ExperimentTransport t) mutable {
    auto l = clients.find(t);
    if (l != clients.end()) return l->second;
    auto p = clients.emplace(t, provider(t));
    return p.first->second;
  };
}

}  // namespace

std::vector<std::unique_ptr<ThroughputExperiment>> CreateUploadExperiments(
    ThroughputOptions const& options, ClientProvider provider) {
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto contents = MakeRandomData(generator, options.maximum_write_size);

  if (!options.client_per_thread) provider = PerTransport(provider);

  std::vector<std::unique_ptr<ThroughputExperiment>> result;
  for (auto l : options.libs) {
    if (l == ExperimentLibrary::kRaw) continue;
    for (auto t : options.transports) {
      result.push_back(
          absl::make_unique<UploadObject>(provider(t), t, contents, false));
      result.push_back(
          absl::make_unique<UploadObject>(provider(t), t, contents, true));
    }
  }
  return result;
}

std::vector<std::unique_ptr<ThroughputExperiment>> CreateDownloadExperiments(
    ThroughputOptions const& options, ClientProvider provider, int thread_id) {
  if (!options.client_per_thread) provider = PerTransport(provider);

  std::vector<std::unique_ptr<ThroughputExperiment>> result;
  for (auto l : options.libs) {
    if (l != ExperimentLibrary::kRaw) {
      for (auto t : options.transports) {
        result.push_back(absl::make_unique<DownloadObject>(provider(t), t));
      }
      continue;
    }
    for (auto t : options.transports) {
      if (t == ExperimentTransport::kJson || t == ExperimentTransport::kXml) {
        result.push_back(absl::make_unique<DownloadObjectLibcurl>(options, t));
      } else {
        result.push_back(
            absl::make_unique<DownloadObjectRawGrpc>(options, thread_id, t));
      }
    }
  }
  return result;
}

}  // namespace storage_benchmarks
}  // namespace cloud
}  // namespace google
