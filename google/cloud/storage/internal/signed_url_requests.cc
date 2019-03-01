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

#include "google/cloud/storage/internal/signed_url_requests.h"
#include "google/cloud/storage/internal/curl_handle.h"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

void SignUrlRequestCommon::SetOption(AddExtensionHeaderOption const& o) {
  if (!o.has_value()) {
    return;
  }
  auto kv = o.value();
  // Normalize the header, they are not case sensitive.
  std::transform(kv.first.begin(), kv.first.end(), kv.first.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  auto res = extension_headers_.insert(kv);
  if (!res.second) {
    // The element already exists, we need to append:
    res.first->second.push_back(',');
    res.first->second.append(kv.second);
  }
}

std::chrono::system_clock::time_point
V2SignUrlRequest::DefaultExpirationTime() {
  return std::chrono::system_clock::now() + std::chrono::hours(7 * 24);
}

std::string V2SignUrlRequest::StringToSign() const {
  std::ostringstream os;

  os << verb() << "\n"
     << md5_hash_value_ << "\n"
     << content_type_ << "\n"
     << expiration_time_as_seconds().count() << "\n";

  for (auto const& kv : common_request_.extension_headers()) {
    os << kv.first << ":" << kv.second << "\n";
  }

  CurlHandle curl;
  os << '/' << bucket_name();
  if (!object_name().empty()) {
    os << '/' << curl.MakeEscapedString(object_name()).get();
  }
  char const* sep = "?";
  if (!sub_resource().empty()) {
    os << sep << curl.MakeEscapedString(sub_resource()).get();
    sep = "&";
  }
  for (auto const& kv : common_request_.query_parameters()) {
    os << sep << curl.MakeEscapedString(kv.first).get() << "="
       << curl.MakeEscapedString(kv.second).get();
    sep = "&";
  }

  return std::move(os).str();
}

std::ostream& operator<<(std::ostream& os, V2SignUrlRequest const& r) {
  return os << "SingUrlRequest={" << r.StringToSign() << "}";
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
