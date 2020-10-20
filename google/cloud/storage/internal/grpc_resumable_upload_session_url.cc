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
#include "google/cloud/storage/internal/grpc_resumable_upload_session_url.pb.h"
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
  GrpcResumableUploadSessionUrl proto;
  proto.set_bucket_name(upload_session_params.bucket_name);
  proto.set_object_name(upload_session_params.object_name);
  proto.set_upload_id(upload_session_params.upload_id);
  std::string proto_rep;
  proto.SerializeToString(&proto_rep);
  return kUriScheme + UrlsafeBase64Encode(proto_rep);
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
  auto const decoded_vec = UrlsafeBase64Decode(payload);
  std::string decoded(decoded_vec.begin(), decoded_vec.end());

  GrpcResumableUploadSessionUrl proto;
  if (!proto.ParseFromString(decoded)) {
    return Status(StatusCode::kInvalidArgument,
                  "Malformed gRPC resumable upload session URL");
  }
  return ResumableUploadSessionGrpcParams{
      proto.bucket_name(), proto.object_name(), proto.upload_id()};
}

bool IsGrpcResumableSessionUrl(std::string const& upload_session_url) {
  return absl::StartsWith(upload_session_url, kUriScheme);
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
