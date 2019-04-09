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
#include "google/cloud/internal/getenv.h"
#include "google/cloud/log.h"
#include "google/cloud/storage/oauth2/credentials.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include <cstdlib>
#include <set>
#include <sstream>
#include <thread>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {
StatusOr<std::shared_ptr<oauth2::Credentials>> StorageDefaultCredentials() {
  auto emulator = cloud::internal::GetEnv("CLOUD_STORAGE_TESTBENCH_ENDPOINT");
  if (emulator.has_value()) {
    return StatusOr<std::shared_ptr<oauth2::Credentials>>(
        oauth2::CreateAnonymousCredentials());
  }
  return oauth2::GoogleDefaultCredentials();
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
#define GOOGLE_CLOUD_CPP_STORAGE_DEFAULT_BUFFER_SIZE (128 * 1024)
#endif  // GOOGLE_CLOUD_CPP_STORAGE_DEFAULT_BUFFER_SIZE

// The documentation recommends uploads below "5MiB" to use simple uploads:
//   https://cloud.google.com/storage/docs/json_api/v1/how-tos/upload
#ifndef GOOGLE_CLOUD_CPP_STORAGE_DEFAULT_MAXIMUM_SIMPLE_UPLOAD_SIZE
#define GOOGLE_CLOUD_CPP_STORAGE_DEFAULT_MAXIMUM_SIMPLE_UPLOAD_SIZE \
  (5 * 1024 * 1024L)
#endif  // GOOGLE_CLOUD_CPP_STORAGE_DEFAULT_MAXIMUM_SIMPLE_UPLOAD_SIZE

}  // namespace

StatusOr<ClientOptions> ClientOptions::CreateDefaultClientOptions() {
  auto creds = StorageDefaultCredentials();
  if (!creds) {
    return creds.status();
  }
  return ClientOptions(*creds);
}

ClientOptions::ClientOptions(std::shared_ptr<oauth2::Credentials> credentials)
    : credentials_(std::move(credentials)),
      endpoint_("https://www.googleapis.com"),
      iam_endpoint_("https://iamcredentials.googleapis.com/v1"),
      version_("v1"),
      enable_http_tracing_(false),
      enable_raw_client_tracing_(false),
      connection_pool_size_(DefaultConnectionPoolSize()),
      download_buffer_size_(GOOGLE_CLOUD_CPP_STORAGE_DEFAULT_BUFFER_SIZE),
      upload_buffer_size_(GOOGLE_CLOUD_CPP_STORAGE_DEFAULT_BUFFER_SIZE),
      maximum_simple_upload_size_(
          GOOGLE_CLOUD_CPP_STORAGE_DEFAULT_MAXIMUM_SIMPLE_UPLOAD_SIZE) {
  auto emulator =
      google::cloud::internal::GetEnv("CLOUD_STORAGE_TESTBENCH_ENDPOINT");
  if (emulator.has_value()) {
    endpoint_ = *emulator;
    iam_endpoint_ = *emulator + "/iamapi";
  }
  SetupFromEnvironment();
}

void ClientOptions::SetupFromEnvironment() {
  auto enable_clog =
      google::cloud::internal::GetEnv("CLOUD_STORAGE_ENABLE_CLOG");
  if (enable_clog.has_value()) {
    google::cloud::LogSink::EnableStdClog();
  }
  // This is overkill right now, eventually we will have different components
  // that can be traced (http being the first), so we parse the environment
  // variable.
  auto tracing =
      google::cloud::internal::GetEnv("CLOUD_STORAGE_ENABLE_TRACING");
  if (tracing.has_value()) {
    std::set<std::string> enabled;
    std::istringstream is{*tracing};
    std::string token;
    while (std::getline(is, token, ',')) {
      enabled.emplace(token);
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

  auto project_id = google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT");
  if (project_id.has_value()) {
    project_id_ = std::move(*project_id);
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
