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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_CONTEXT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_CONTEXT_H

#include "google/cloud/version.h"
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * This type is used to track state and for decorators to modify across a
 * request and response round trip rpc.
 */
class RestContext {
 public:
  using HttpHeaders = std::unordered_map<std::string, std::vector<std::string>>;
  RestContext() = default;
  explicit RestContext(HttpHeaders headers) : headers_(std::move(headers)) {}

  HttpHeaders const& headers() const { return headers_; }

  // Adding a header/value pair that already exists results in the new value
  // appended to the list of values for the existing header.
  RestContext& AddHeader(std::string header, std::string value) &;
  RestContext&& AddHeader(std::string header, std::string value) && {
    return std::move(AddHeader(std::move(header), std::move(value)));
  }
  RestContext& AddHeader(std::pair<std::string, std::string> header) &;
  RestContext&& AddHeader(std::pair<std::string, std::string> header) && {
    return std::move(AddHeader(std::move(header)));
  }

  // Vector is empty if header name is not found.
  // Header names are case-insensitive; header values are case-sensitive.
  std::vector<std::string> GetHeader(std::string header) const;

 private:
  friend bool operator==(RestContext const& lhs, RestContext const& rhs);
  HttpHeaders headers_;
};

bool operator==(RestContext const& lhs, RestContext const& rhs);
bool operator!=(RestContext const& lhs, RestContext const& rhs);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_CONTEXT_H
