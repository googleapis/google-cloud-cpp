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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_REQUEST_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_REQUEST_H

#include "google/cloud/internal/rest_context.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <unordered_map>
#include <vector>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class RestClient;

// This class is a regular type that contains the path, headers, and query
// parameters for use in sending a request to a REST-ful service. It is intended
// to be passed to the appropriate HTTP method on RestClient, along with a
// payload if required.
class RestRequest {
 public:
  using HttpHeaders = std::unordered_map<std::string, std::vector<std::string>>;
  using HttpParameters = std::vector<std::pair<std::string, std::string>>;

  RestRequest();
  explicit RestRequest(std::string path);
  explicit RestRequest(RestContext const& rest_context);
  RestRequest(std::string path, HttpHeaders headers);
  RestRequest(std::string path, HttpParameters parameters);
  RestRequest(std::string path, HttpHeaders headers, HttpParameters parameters);

  std::string const& path() const { return path_; }
  HttpHeaders const& headers() const { return headers_; }
  HttpParameters const& parameters() const { return parameters_; }

  RestRequest& SetPath(std::string path) &;
  RestRequest&& SetPath(std::string path) && {
    return std::move(SetPath(std::move(path)));
  }

  RestRequest& AppendPath(std::string path) &;
  RestRequest&& AppendPath(std::string path) && {
    return std::move(AppendPath(std::move(path)));
  }

  // Adding a header/value pair that already exists results in the new value
  // appended to the list of values for the existing header.
  RestRequest& AddHeader(std::string header, std::string value) &;
  RestRequest&& AddHeader(std::string header, std::string value) && {
    return std::move(AddHeader(std::move(header), std::move(value)));
  }
  RestRequest& AddHeader(std::pair<std::string, std::string> header) &;
  RestRequest&& AddHeader(std::pair<std::string, std::string> header) && {
    return std::move(AddHeader(std::move(header)));
  }

  // Adding a duplicate param and or value results in both the new and original
  // pairs stored in order of addition.
  RestRequest& AddQueryParameter(std::string parameter, std::string value) &;
  RestRequest&& AddQueryParameter(std::string parameter, std::string value) && {
    return std::move(AddQueryParameter(std::move(parameter), std::move(value)));
  }
  RestRequest& AddQueryParameter(
      std::pair<std::string, std::string> parameter) &;
  RestRequest&& AddQueryParameter(
      std::pair<std::string, std::string> parameter) && {
    return std::move(AddQueryParameter(std::move(parameter)));
  }

  // Vector is empty if header name is not found.
  // Header names are case-insensitive; header values are case-sensitive.
  std::vector<std::string> GetHeader(std::string header) const;

  // Returns all values associated with parameter name.
  // Parameter names and values are case-sensitive.
  std::vector<std::string> GetQueryParameter(
      std::string const& parameter) const;

 private:
  friend bool operator==(RestRequest const& lhs, RestRequest const& rhs);
  std::string path_;
  HttpHeaders headers_;
  HttpParameters parameters_;
};

bool operator==(RestRequest const& lhs, RestRequest const& rhs);
bool operator!=(RestRequest const& lhs, RestRequest const& rhs);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_REQUEST_H
