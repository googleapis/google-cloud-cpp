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

#include "google/cloud/internal/rest_context.h"
#include <algorithm>
#include <cctype>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

RestContext& RestContext::AddHeader(std::string header, std::string value) & {
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

RestContext& RestContext::AddHeader(
    std::pair<std::string, std::string> header) & {
  return AddHeader(std::move(header.first), std::move(header.second));
}

std::vector<std::string> RestContext::GetHeader(std::string header) const {
  std::transform(header.begin(), header.end(), header.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  auto iter = headers_.find(header);
  if (iter == headers_.end()) {
    return {};
  }
  return iter->second;
}

bool operator==(RestContext const& lhs, RestContext const& rhs) {
  return (lhs.headers_ == rhs.headers_);
}

bool operator!=(RestContext const& lhs, RestContext const& rhs) {
  return !(lhs == rhs);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
