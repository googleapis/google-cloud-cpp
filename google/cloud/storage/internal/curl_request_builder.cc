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

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

CurlRequestBuilder::CurlRequestBuilder(std::string base_url)
    : headers_(nullptr, &curl_slist_free_all),
      url_(std::move(base_url)),
      query_parameter_separator_("?"),
      logging_enabled_(false) {}

CurlRequest CurlRequestBuilder::BuildRequest(std::string payload) {
  ValidateBuilderState(__func__);
  CurlRequest request;
  request.url_ = std::move(url_);
  request.headers_ = std::move(headers_);
  request.user_agent_ = user_agent_prefix_ + UserAgentSuffix();
  request.payload_ = std::move(payload);
  request.handle_ = std::move(handle_);
  request.logging_enabled_ = logging_enabled_;
  request.ResetOptions();
  return request;
}

CurlUploadRequest CurlRequestBuilder::BuildUpload() {
  ValidateBuilderState(__func__);
  CurlUploadRequest request;
  request.url_ = std::move(url_);
  request.headers_ = std::move(headers_);
  request.user_agent_ = user_agent_prefix_ + UserAgentSuffix();
  request.handle_ = std::move(handle_);
  request.multi_.reset(curl_multi_init());
  request.ResetOptions();
  return request;
}

CurlRequestBuilder& CurlRequestBuilder::AddUserAgentPrefix(
    std::string const& prefix) {
  ValidateBuilderState(__func__);
  user_agent_prefix_ = prefix + user_agent_prefix_;
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

CurlRequestBuilder& CurlRequestBuilder::SetDebugLogging(bool enabled) {
  ValidateBuilderState(__func__);
  logging_enabled_ = enabled;
  return *this;
}

std::string CurlRequestBuilder::UserAgentSuffix() const {
  ValidateBuilderState(__func__);
  // Pre-compute and cache the user agent string:
  static std::string const user_agent_suffix = [] {
    std::string agent = "gcs-c++/";
    agent += storage::version_string();
    agent += ' ';
    agent += curl_version();
    return agent;
  }();
  return user_agent_suffix;
}

void CurlRequestBuilder::ValidateBuilderState(char const* where) const {
  if (not static_cast<bool>(handle_.handle_)) {
    std::string msg = "Attempt to use invalidated CurlRequest in ";
    msg += where;
    google::cloud::internal::RaiseRuntimeError(msg);
  }
}
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
