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

#include "google/cloud/storage/internal/curl_request_builder.h"
#include "google/cloud/internal/build_info.h"
#include "google/cloud/storage/version.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

#ifndef GOOGLE_CLOUD_CPP_STORAGE_INITIAL_BUFFER_SIZE
#define GOOGLE_CLOUD_CPP_STORAGE_INITIAL_BUFFER_SIZE (128 * 1024)
#endif  // GOOGLE_CLOUD_CPP_STORAGE_INITIAL_BUFFER_SIZE

CurlRequestBuilder::CurlRequestBuilder(
    std::string base_url, std::shared_ptr<CurlHandleFactory> factory)
    : factory_(std::move(factory)),
      handle_(factory_->CreateHandle()),
      headers_(nullptr, &curl_slist_free_all),
      url_(std::move(base_url)),
      query_parameter_separator_("?"),
      logging_enabled_(false),
      download_stall_timeout_(0) {}

CurlRequest CurlRequestBuilder::BuildRequest() {
  ValidateBuilderState(__func__);
  CurlRequest request;
  request.url_ = std::move(url_);
  request.headers_ = std::move(headers_);
  request.user_agent_ = user_agent_prefix_ + UserAgentSuffix();
  request.handle_ = std::move(handle_);
  request.factory_ = std::move(factory_);
  request.logging_enabled_ = logging_enabled_;
  request.socket_options_ = socket_options_;
  request.ResetOptions();
  return request;
}

CurlDownloadRequest CurlRequestBuilder::BuildDownloadRequest(
    std::string payload) {
  ValidateBuilderState(__func__);
  CurlDownloadRequest request;
  request.url_ = std::move(url_);
  request.headers_ = std::move(headers_);
  request.user_agent_ = user_agent_prefix_ + UserAgentSuffix();
  request.payload_ = std::move(payload);
  request.handle_ = std::move(handle_);
  request.multi_ = factory_->CreateMultiHandle();
  request.factory_ = factory_;
  request.logging_enabled_ = logging_enabled_;
  request.socket_options_ = socket_options_;
  request.download_stall_timeout_ = download_stall_timeout_;
  request.SetOptions();
  return request;
}

CurlRequestBuilder& CurlRequestBuilder::ApplyClientOptions(
    ClientOptions const& options) {
  ValidateBuilderState(__func__);
  logging_enabled_ = options.enable_http_tracing();
  socket_options_.recv_buffer_size_ = options.maximum_socket_recv_size();
  socket_options_.send_buffer_size_ = options.maximum_socket_send_size();
  user_agent_prefix_ = options.user_agent_prefix() + user_agent_prefix_;
  download_stall_timeout_ = options.download_stall_timeout();
  return *this;
}

CurlRequestBuilder& CurlRequestBuilder::AddHeader(std::string const& header) {
  ValidateBuilderState(__func__);
  auto new_header = curl_slist_append(headers_.get(), header.c_str());
  (void)headers_.release();
  headers_.reset(new_header);
  return *this;
}

CurlRequestBuilder& CurlRequestBuilder::AddQueryParameter(
    std::string const& key, std::string const& value) {
  ValidateBuilderState(__func__);
  std::string parameter = query_parameter_separator_;
  parameter += handle_.MakeEscapedString(key).get();
  parameter += "=";
  parameter += handle_.MakeEscapedString(value).get();
  query_parameter_separator_ = "&";
  url_.append(parameter);
  return *this;
}

CurlRequestBuilder& CurlRequestBuilder::SetMethod(std::string const& method) {
  ValidateBuilderState(__func__);
  handle_.SetOption(CURLOPT_CUSTOMREQUEST, method.c_str());
  return *this;
}

CurlRequestBuilder& CurlRequestBuilder::SetCurlShare(CURLSH* share) {
  handle_.SetOption(CURLOPT_SHARE, share);
  return *this;
}

std::string CurlRequestBuilder::UserAgentSuffix() const {
  ValidateBuilderState(__func__);
  // Pre-compute and cache the user agent string:
  static std::string const kUserAgentSuffix = [] {
    std::string agent = "gcloud-cpp/" + storage::version_string() + " ";
    agent += curl_version();
    agent += " " + google::cloud::internal::compiler();
    return agent;
  }();
  return kUserAgentSuffix;
}

void CurlRequestBuilder::ValidateBuilderState(char const* where) const {
  if (handle_.handle_.get() == nullptr) {
    std::string msg = "Attempt to use invalidated CurlRequest in ";
    msg += where;
    google::cloud::internal::ThrowRuntimeError(msg);
  }
}
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
