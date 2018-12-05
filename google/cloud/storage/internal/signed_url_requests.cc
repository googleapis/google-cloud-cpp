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
#include <sstream>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

std::string SignUrlRequest::StringToSign() const {
  std::ostringstream os;

  auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
      expiration_time_.time_since_epoch());

  os << verb() << "\n"
     << md5_hash_value_ << "\n"
     << content_type_ << "\n"
     << seconds.count() << "\n";

  for (auto const& kv : extension_headers_) {
    os << kv.first << ":" << kv.second << "\n";
  }
  CurlHandle curl;
  os << "/" << bucket_name() << "/"
     << curl.MakeEscapedString(object_name()).get();

  return std::move(os).str();
}

std::ostream& operator<<(std::ostream& os, SignUrlRequest const& r) {
  return os << "SingUrlRequest={" << r.StringToSign() << "}";
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
