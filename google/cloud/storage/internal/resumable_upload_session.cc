// Copyright 2019 Google LLC
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

#include "google/cloud/storage/internal/resumable_upload_session.h"
#include "google/cloud/storage/internal/binary_data_as_debug_string.h"
#include "google/cloud/storage/internal/object_metadata_parser.h"
#include "google/cloud/storage/internal/object_requests.h"
#include <iostream>
#include <sstream>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

StatusOr<ResumableUploadResponse> ResumableUploadResponse::FromHttpResponse(
    HttpResponse response) {
  ResumableUploadResponse result;
  if (response.status_code == HttpStatusCode::kOk ||
      response.status_code == HttpStatusCode::kCreated) {
    result.upload_state = kDone;
  } else {
    result.upload_state = kInProgress;
  }
  result.annotations += "code=" + std::to_string(response.status_code);
  // For the JSON API, the payload contains the object resource when the upload
  // is finished. In that case, we try to parse it.
  if (result.upload_state == kDone && !response.payload.empty()) {
    auto contents = ObjectMetadataParser::FromString(response.payload);
    if (!contents) return std::move(contents).status();
    result.payload = *std::move(contents);
  }
  if (response.headers.find("location") != response.headers.end()) {
    result.upload_session_url = response.headers.find("location")->second;
  }
  auto r = response.headers.find("range");
  if (r == response.headers.end()) return result;

  result.annotations += " range=" + r->second;
  auto last_committed_byte = ParseRangeHeader(r->second);
  if (!last_committed_byte) return std::move(last_committed_byte).status();
  result.committed_size = *last_committed_byte + 1;

  return result;
}

bool operator==(ResumableUploadResponse const& lhs,
                ResumableUploadResponse const& rhs) {
  return lhs.upload_session_url == rhs.upload_session_url &&
         lhs.committed_size == rhs.committed_size &&
         lhs.payload == rhs.payload && lhs.upload_state == rhs.upload_state;
}

bool operator!=(ResumableUploadResponse const& lhs,
                ResumableUploadResponse const& rhs) {
  return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os, ResumableUploadResponse const& r) {
  char const* state = r.upload_state == ResumableUploadResponse::kDone
                          ? "kDone"
                          : "kInProgress";
  os << "ResumableUploadResponse={upload_session_url=" << r.upload_session_url
     << ", upload_state=" << state << ", committed_size=";
  if (r.committed_size.has_value()) {
    os << *r.committed_size;
  } else {
    os << "{}";
  }
  os << ", payload=";
  if (r.payload.has_value()) {
    os << *r.payload;
  } else {
    os << "{}";
  }
  return os << ", annotations=" << r.annotations << "}";
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
