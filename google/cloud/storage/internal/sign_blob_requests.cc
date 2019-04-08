// Copyright 2019 Google LLC
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

#include "google/cloud/storage/internal/sign_blob_requests.h"
#include "google/cloud/storage/internal/nljson.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

std::ostream& operator<<(std::ostream& os, SignBlobRequest const& r) {
  auto join = [](std::vector<std::string> const& v) -> std::string {
    if (v.empty()) {
      return std::string{};
    }
    return std::accumulate(++v.begin(), v.end(), v.front(),
                           [](std::string a, std::string const& b) {
                             a += "; ";
                             a += b;
                             return a;
                           });
  };
  return os << "SignBlobRequest={service_account=" << r.service_account()
            << ", base64_encoded_blob=" << r.base64_encoded_blob()
            << ", delegates=" << join(r.delegates()) << "}";
}

StatusOr<SignBlobResponse> SignBlobResponse::FromHttpResponse(
    std::string const& payload) {
  auto json = storage::internal::nl::json::parse(payload, nullptr, false);
  if (!json.is_object()) {
    return Status(StatusCode::kInvalidArgument, __func__);
  }
  SignBlobResponse result;
  result.key_id = json.value("keyId", "");
  result.signed_blob = json.value("signedBlob", "");
  return result;
}

std::ostream& operator<<(std::ostream& os, SignBlobResponse const& r) {
  return os << "SignBlobResponse={key_id=" << r.key_id
            << ", signed_blob=" << r.signed_blob << "}";
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
