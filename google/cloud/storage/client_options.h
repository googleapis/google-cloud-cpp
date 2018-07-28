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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_CLIENT_OPTIONS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_CLIENT_OPTIONS_H_

#include "google/cloud/storage/credentials.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
/**
 * Control the configuration for a `storage::Client` object.
 *
 * By default read several environment variables to configure the client:
 *
 * - `CLOUD_STORAGE_TESTBENCH_ENDPOINT`: if set, use this http endpoint to
 *   make all http requests instead of the production GCS service. Also,
 *   if set it uses the `google::cloud::storage::InsecureCredentials` by
 *   default.
 * - `CLOUD_STORAGE_ENABLE_CLOG`: if set, enable std::clog as a backend for
 *   `google::cloud::LogSink`.
 * - `CLOUD_STORAGE_ENABLE_TRACING`: if set, this is the list of components that
 *   will have logging enabled, the component this is:
 *   - `http`: trace all http request / responses.
 */
class ClientOptions {
 public:
  ClientOptions();
  explicit ClientOptions(std::shared_ptr<Credentials> credentials);

  std::shared_ptr<Credentials> credentials() const { return credentials_; }
  ClientOptions& set_credentials(std::shared_ptr<Credentials> credentials) {
    credentials_ = std::move(credentials);
    return *this;
  }

  std::string const& endpoint() const { return endpoint_; }
  ClientOptions& set_endpoint(std::string endpoint) {
    endpoint_ = std::move(endpoint);
    return *this;
  }

  std::string const& version() const { return version_; }
  ClientOptions& set_version(std::string version) {
    version_ = std::move(version);
    return *this;
  }

  bool enable_http_tracing() const { return enable_http_tracing_; }
  ClientOptions& set_enable_http_tracing(bool enable) {
    enable_http_tracing_ = enable;
    return *this;
  }

  bool enable_raw_client_tracing() const { return enable_raw_client_tracing_; }
  ClientOptions& set_enable_raw_client_tracing(bool enable) {
    enable_raw_client_tracing_ = enable;
    return *this;
  }

  std::string const& project_id() const { return project_id_; }
  ClientOptions& set_project_id(std::string v) {
    project_id_ = std::move(v);
    return *this;
  }

 private:
  void SetupFromEnvironment();

 private:
  std::shared_ptr<Credentials> credentials_;
  std::string endpoint_;
  std::string version_;
  bool enable_http_tracing_;
  bool enable_raw_client_tracing_;
  std::string project_id_;
};
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_CLIENT_OPTIONS_H_
