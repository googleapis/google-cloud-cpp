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

#include "google/cloud/storage/internal/grpc_sign_blob_request_parser.h"
#include "google/cloud/storage/internal/openssl_util.h"

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

google::iam::credentials::v1::SignBlobRequest ToProto(
    storage::internal::SignBlobRequest const& rhs) {
  google::iam::credentials::v1::SignBlobRequest request;
  request.set_name("projects/-/serviceAccounts/" + rhs.service_account());
  for (auto const& d : rhs.delegates()) request.add_delegates(d);
  auto b64decoded = storage::internal::Base64Decode(rhs.base64_encoded_blob());
  if (!b64decoded) return request;
  request.mutable_payload()->assign(b64decoded->begin(), b64decoded->end());
  return request;
}

storage::internal::SignBlobResponse FromProto(
    google::iam::credentials::v1::SignBlobResponse const& rhs) {
  storage::internal::SignBlobResponse response;
  response.key_id = rhs.key_id();
  response.signed_blob = storage::internal::Base64Encode(rhs.signed_blob());
  return response;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
