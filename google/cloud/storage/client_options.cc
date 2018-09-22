// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/client_options.h"
#include "google/cloud/log.h"
#include <cstdlib>
#include <set>
#include <sstream>
#include <thread>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {
std::shared_ptr<google::cloud::storage::Credentials>
StorageDefaultCredentials() {
  char const* emulator = std::getenv("CLOUD_STORAGE_TESTBENCH_ENDPOINT");
  if (emulator != nullptr) {
    return google::cloud::storage::CreateInsecureCredentials();
  }
  return google::cloud::storage::GoogleDefaultCredentials();
}

std::size_t DefaultConnectionPoolSize() {
  std::size_t nthreads = std::thread::hardware_concurrency();
  if (nthreads == 0) {
    return 4;
  }
  return 4 * nthreads;
}

// There is nothing special about the buffer sizes here. They are relatively
// small, because we do not want to consume too much memory from the
// application. They are larger than the typical socket buffer size (64KiB), to
// be able to read the full buffer into userspace if it happens to be full.
#ifndef GOOGLE_CLOUD_CPP_STORAGE_DEFAULT_BUFFER_SIZE
#define GOOGLE_CLOUD_CPP_STORAGE_DEFAULT_BUFFER_SIZE 128 * 1024
#endif  // GOOGLE_CLOUD_CPP_STORAGE_DEFAULT_BUFFER_SIZE

}  // namespace

ClientOptions::ClientOptions() : ClientOptions(StorageDefaultCredentials()) {}

ClientOptions::ClientOptions(std::shared_ptr<Credentials> credentials)
    : credentials_(std::move(credentials)),
      endpoint_("https://www.googleapis.com"),
      version_("v1"),
      enable_http_tracing_(false),
      enable_raw_client_tracing_(false),
      connection_pool_size_(DefaultConnectionPoolSize()),
      download_buffer_size_(GOOGLE_CLOUD_CPP_STORAGE_DEFAULT_BUFFER_SIZE),
      upload_buffer_size_(GOOGLE_CLOUD_CPP_STORAGE_DEFAULT_BUFFER_SIZE) {
  char const* emulator = std::getenv("CLOUD_STORAGE_TESTBENCH_ENDPOINT");
  if (emulator != nullptr) {
    endpoint_ = emulator;
  }
  SetupFromEnvironment();
}

void ClientOptions::SetupFromEnvironment() {
  char const* enable_clog = std::getenv("CLOUD_STORAGE_ENABLE_CLOG");
  if (enable_clog != nullptr) {
    google::cloud::LogSink::EnableStdClog();
  }
  // This is overkill right now, eventually we will have different components
  // that can be traced (http being the first), so we parse the environment
  // variable.
  char const* tracing = std::getenv("CLOUD_STORAGE_ENABLE_TRACING");
  if (tracing != nullptr) {
    std::set<std::string> enabled;
    std::istringstream is{std::string(tracing)};
    while (not is.eof()) {
      std::string token;
      std::getline(is, token, ',');
      enabled.emplace(std::move(token));
    }
    if (enabled.end() != enabled.find("http")) {
      GCP_LOG(INFO) << "Enabling logging for http";
      set_enable_http_tracing(true);
    }
    if (enabled.end() != enabled.find("raw-client")) {
      GCP_LOG(INFO) << "Enabling logging for RawClient functions";
      set_enable_raw_client_tracing(true);
    }
  }

  char const* project_id = std::getenv("GOOGLE_CLOUD_PROJECT");
  if (project_id != nullptr) {
    project_id_ = project_id;
  }
}

ClientOptions& ClientOptions::SetDownloadBufferSize(std::size_t size) {
  if (size == 0) {
    download_buffer_size_ = GOOGLE_CLOUD_CPP_STORAGE_DEFAULT_BUFFER_SIZE;
  } else {
    download_buffer_size_ = size;
  }
  return *this;
}

ClientOptions& ClientOptions::SetUploadBufferSize(std::size_t size) {
  if (size == 0) {
    upload_buffer_size_ = GOOGLE_CLOUD_CPP_STORAGE_DEFAULT_BUFFER_SIZE;
  } else {
    upload_buffer_size_ = size;
  }
  return *this;
}

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
