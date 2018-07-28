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

}  // namespace

ClientOptions::ClientOptions() : ClientOptions(StorageDefaultCredentials()) {}

ClientOptions::ClientOptions(std::shared_ptr<Credentials> credentials)
    : credentials_(std::move(credentials)),
      endpoint_("https://www.googleapis.com"),
      version_("v1"),
      enable_http_tracing_(false),
      enable_raw_client_tracing_(false) {
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

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
