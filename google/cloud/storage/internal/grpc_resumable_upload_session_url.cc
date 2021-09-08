// Copyright 2020 Google LLC
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

#include "google/cloud/storage/internal/grpc_resumable_upload_session_url.h"
#include "google/cloud/storage/internal/openssl_util.h"
#include "absl/strings/match.h"
#include <cstring>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

auto constexpr kUriScheme = "grpc://";

std::string EncodeGrpcResumableUploadSessionUrl(
    ResumableUploadSessionGrpcParams const& upload_session_params) {
  return kUriScheme + UrlsafeBase64Encode(upload_session_params.upload_id);
}

StatusOr<ResumableUploadSessionGrpcParams> DecodeGrpcResumableUploadSessionUrl(
    std::string const& upload_session_url) {
  if (!IsGrpcResumableSessionUrl(upload_session_url)) {
    return Status(
        StatusCode::kInvalidArgument,
        "gRPC implementation of GCS client cannot interpret a resumable upload "
        "session from a different implementation (e.g. cURL based). Check your "
        "configuration");
  }
  auto const payload = upload_session_url.substr(std::strlen(kUriScheme));
  auto decoded_vec = UrlsafeBase64Decode(payload);
  if (!decoded_vec) return std::move(decoded_vec).status();
  return ResumableUploadSessionGrpcParams{
      std::string(decoded_vec->begin(), decoded_vec->end())};
}

bool IsGrpcResumableSessionUrl(std::string const& upload_session_url) {
  return absl::StartsWith(upload_session_url, kUriScheme);
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
