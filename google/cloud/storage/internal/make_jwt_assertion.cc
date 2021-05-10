// Copyright 2021 Google LLC
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

#include "google/cloud/storage/internal/make_jwt_assertion.h"
#include "google/cloud/storage/internal/openssl_util.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

StatusOr<std::string> MakeJWTAssertionNoThrow(std::string const& header,
                                              std::string const& payload,
                                              std::string const& pem_contents) {
  auto const body =
      UrlsafeBase64Encode(header) + '.' + UrlsafeBase64Encode(payload);
  auto pem_signature = internal::SignStringWithPem(
      body, pem_contents, storage::oauth2::JwtSigningAlgorithms::RS256);
  if (!pem_signature) return std::move(pem_signature).status();
  return body + '.' + UrlsafeBase64Encode(*pem_signature);
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
