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
#include "absl/strings/strip.h"
#include <algorithm>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

RestContext& RestContext::AddHeader(HttpHeader header) & {
  auto iter = headers_.find(header.name());
  if (iter == headers_.end()) {
    headers_.emplace(header.name(), std::move(header));
  } else {
    iter->second.MergeHeader(std::move(header));
  }
  return *this;
}

RestContext& RestContext::AddHeader(std::string header, std::string value) & {
  return AddHeader(HttpHeader(std::move(header), std::move(value)));
}

HttpHeader RestContext::GetHeader(HttpHeaderName const& header) const {
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
