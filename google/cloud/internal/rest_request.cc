// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/internal/rest_request.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/log.h"
#include "absl/strings/strip.h"
#include <cctype>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

RestRequest::RestRequest() = default;
RestRequest::RestRequest(std::string path) : path_(std::move(path)) {}
RestRequest::RestRequest(std::string path, HttpHeaders headers)
    : path_(std::move(path)), headers_(std::move(headers)) {}
RestRequest::RestRequest(std::string path, HttpParameters parameters)
    : path_(std::move(path)), parameters_(std::move(parameters)) {}
RestRequest::RestRequest(std::string path, HttpHeaders headers,
                         HttpParameters parameters)
    : path_(std::move(path)),
      headers_(std::move(headers)),
      parameters_(std::move(parameters)) {}

RestRequest& RestRequest::SetPath(std::string path) & {
  path_ = std::move(path);
  return *this;
}

RestRequest& RestRequest::AppendPath(std::string path) & {
  if (path_.empty()) return SetPath(std::move(path));
  if (path_.back() != '/' && path.front() != '/') {
    path_ = absl::StrCat(path_, "/", path);
  } else if (path_.back() == '/' && path.front() == '/') {
    path_ = absl::StrCat(path_, absl::StripPrefix(path, "/"));
  } else {
    path_ = absl::StrCat(path_, path);
  }
  return *this;
}

RestRequest& RestRequest::AddHeader(std::string header, std::string value) & {
  std::transform(header.begin(), header.end(), header.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  auto iter = headers_.find(header);
  if (iter == headers_.end()) {
    std::vector<std::string> v = {std::move(value)};
    headers_.emplace(std::move(header), std::move(v));
  } else {
    iter->second.push_back(value);
  }
  return *this;
}

RestRequest& RestRequest::AddHeader(
    std::pair<std::string, std::string> header) & {
  return AddHeader(std::move(header.first), std::move(header.second));
}

RestRequest& RestRequest::AddQueryParameter(std::string parameter,
                                            std::string value) & {
  parameters_.emplace_back(std::move(parameter), std::move(value));
  return *this;
}

RestRequest& RestRequest::AddQueryParameter(
    std::pair<std::string, std::string> parameter) & {
  return AddQueryParameter(std::move(parameter.first),
                           std::move(parameter.second));
}

std::vector<std::string> RestRequest::GetHeader(std::string header) const {
  std::transform(header.begin(), header.end(), header.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  auto iter = headers_.find(header);
  if (iter == headers_.end()) {
    return {};
  }
  return iter->second;
}

std::vector<std::string> RestRequest::GetQueryParameter(
    std::string const& parameter) const {
  std::vector<std::string> parameter_values;
  for (auto const& p : parameters_) {
    if (p.first == parameter) {
      parameter_values.push_back(p.second);
    }
  }
  return parameter_values;
}

bool operator==(RestRequest const& lhs, RestRequest const& rhs) {
  return (lhs.path_ == rhs.path_) && (lhs.headers_ == rhs.headers_) &&
         (lhs.parameters_ == rhs.parameters_);
}

bool operator!=(RestRequest const& lhs, RestRequest const& rhs) {
  return !(lhs == rhs);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
