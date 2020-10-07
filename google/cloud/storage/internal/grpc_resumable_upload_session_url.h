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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_RESUMABLE_UPLOAD_SESSION_URL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_RESUMABLE_UPLOAD_SESSION_URL_H

#include "google/cloud/storage/version.h"
#include "google/cloud/status_or.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

/*
 * These functions synthesize a session URL for gRPC API.
 *
 * In the REST API, whenever we operate on resumable uploads, GCS returns a
 * resumable session URL, which identifies the bucket, object and the session
 * ID. In gRPC implementation this is no longer true. In order to not change the
 * external API, we made a decision to synthesize such a link for the gRPC
 * implementation. More details in the GitHub issue:
 * https://github.com/googleapis/google-cloud-cpp/issues/5030
 */

/// Parameters that should be bundled with a resumable upload session ID.
struct ResumableUploadSessionGrpcParams {
  std::string bucket_name;
  std::string object_name;
  std::string upload_id;
};

/// Encode `ResumableUploadSessionGrpcParams` into a URI.
std::string EncodeGrpcResumableUploadSessionUrl(
    ResumableUploadSessionGrpcParams const& upload_session_params);

/// Decode `ResumableUploadSessionGrpcParams` from a URI.
StatusOr<ResumableUploadSessionGrpcParams> DecodeGrpcResumableUploadSessionUrl(
    std::string const& upload_session_url);

/// Check if a URI is a representation of `ResumableUploadSessionGrpcParams`.
bool IsGrpcResumableSessionUrl(std::string const& upload_session_url);

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_RESUMABLE_UPLOAD_SESSION_URL_H
