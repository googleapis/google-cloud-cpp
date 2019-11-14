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

#include "google/cloud/storage/internal/resumable_upload_session.h"
#include "google/cloud/storage/internal/binary_data_as_debug_string.h"
#include "google/cloud/storage/internal/object_requests.h"
#include <iostream>
#include <sstream>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
StatusOr<ResumableUploadResponse> ResumableUploadResponse::FromHttpResponse(
    HttpResponse response) {
  ResumableUploadResponse result;
  if (response.status_code == 200 || response.status_code == 201) {
    result.upload_state = kDone;
  } else {
    result.upload_state = kInProgress;
  }
  result.last_committed_byte = 0;
  // For the JSON API, the payload contains the object resource when the upload
  // is finished. In that case, we try to parse it.
  if (result.upload_state == kDone && !response.payload.empty()) {
    auto contents = ObjectMetadataParser::FromString(response.payload);
    if (!contents) {
      return std::move(contents).status();
    }
    result.payload = *std::move(contents);
  }
  if (response.headers.find("location") != response.headers.end()) {
    result.upload_session_url = response.headers.find("location")->second;
  }
  auto r = response.headers.find("range");
  if (r == response.headers.end()) {
    std::ostringstream os;
    os << __func__ << "() missing range header in resumable upload response"
       << ", response=" << response;
    result.annotations = std::move(os).str();
    return result;
  }
  // We expect a `Range:` header in the format described here:
  //    https://cloud.google.com/storage/docs/json_api/v1/how-tos/resumable-upload
  // that is the value should match `bytes=0-[0-9]+`:
  std::string const& range = r->second;

  if (range.rfind("bytes=0-", 0) != 0) {
    std::ostringstream os;
    os << __func__ << "() cannot parse range: header in resumable upload"
       << " response, header=" << range << ", response=" << response;
    result.annotations = std::move(os).str();
    return result;
  }
  char const* buffer = range.data() + 8;
  char* endptr;
  auto last = std::strtoll(buffer, &endptr, 10);
  if (buffer != endptr && *endptr == '\0' && 0 <= last) {
    result.last_committed_byte = static_cast<std::uint64_t>(last);
  } else {
    std::ostringstream os;
    os << __func__ << "() cannot parse range: header in resumable upload"
       << " response, header=" << range << ", response=" << response;
    result.annotations = std::move(os).str();
  }

  return result;
}

bool operator==(ResumableUploadResponse const& lhs,
                ResumableUploadResponse const& rhs) {
  return lhs.upload_session_url == rhs.upload_session_url &&
         lhs.last_committed_byte == rhs.last_committed_byte &&
         lhs.payload == rhs.payload && lhs.upload_state == rhs.upload_state;
}

bool operator!=(ResumableUploadResponse const& lhs,
                ResumableUploadResponse const& rhs) {
  return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os, ResumableUploadResponse const& r) {
  os << "ResumableUploadResponse={upload_session_url=" << r.upload_session_url
     << ", last_committed_byte=" << r.last_committed_byte << ", payload=";
  if (r.payload.has_value()) {
    os << *r.payload;
  } else {
    os << "{}";
  }
  return os << ", upload_state=" << r.upload_state
            << ", annotations=" << r.annotations << "}";
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
