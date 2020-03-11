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

#include "google/cloud/storage/internal/http_response.h"
#include <iostream>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

Status AsStatus(HttpResponse const& http_response) {
  // The code here is organized by increasing range (or value) of the errors,
  // just to make it readable. There are probably shorter (and/or more
  // efficient) ways to write this, but we went for readability.

  if (http_response.status_code < HttpStatusCode::kMinContinue) {
    return Status(StatusCode::kUnknown, http_response.payload);
  }
  if (HttpStatusCode::kMinContinue <= http_response.status_code &&
      http_response.status_code < HttpStatusCode::kMinSuccess) {
    // We treat the 100s (e.g. 100 Continue) as OK results. They normally are
    // ignored by libcurl, so we do not really expect to see them.
    return Status(StatusCode::kOk, std::string{});
  }
  if (HttpStatusCode::kMinSuccess <= http_response.status_code &&
      http_response.status_code < HttpStatusCode::kMinRedirects) {
    // We treat the 200s as Okay results.
    return Status(StatusCode::kOk, std::string{});
  }
  if (http_response.status_code == HttpStatusCode::kResumeIncomplete) {
    // 308 - Resume Incomplete: this one is terrible. When performing a PUT
    // for a resumable upload this means "The client and server are out of sync
    // in this resumable upload, please reset". Unfortunately, during a
    // "reset" this means "The reset worked, here is the next committed byte,
    // keep in mind that the server is still doing work".  The second is more
    // like a kOk, the first is more like a kFailedPrecondition.
    // This level of complexity / detail is something that the caller should
    // handle, i.e., the mapping depends on the operation.
    return Status(StatusCode::kFailedPrecondition, http_response.payload);
  }
  if (HttpStatusCode::kMinRedirects <= http_response.status_code &&
      http_response.status_code < HttpStatusCode::kMinRequestErrors) {
    // The 300s should be handled by libcurl, we should not get them, according
    // to the Google Cloud Storage documentation these are:
    // 302 - Found
    // 303 - See Other
    // 304 - Not Modified
    // 307 - Temporary Redirect
    return Status(StatusCode::kUnknown, http_response.payload);
  }
  if (http_response.status_code == HttpStatusCode::kBadRequest) {
    return Status(StatusCode::kInvalidArgument, http_response.payload);
  }
  if (http_response.status_code == HttpStatusCode::kUnauthorized) {
    return Status(StatusCode::kUnauthenticated, http_response.payload);
  }
  if (http_response.status_code == HttpStatusCode::kForbidden) {
    return Status(StatusCode::kPermissionDenied, http_response.payload);
  }
  if (http_response.status_code == HttpStatusCode::kNotFound) {
    return Status(StatusCode::kNotFound, http_response.payload);
  }
  if (http_response.status_code == HttpStatusCode::kMethodNotAllowed) {
    return Status(StatusCode::kPermissionDenied, http_response.payload);
  }
  if (http_response.status_code == HttpStatusCode::kConflict) {
    return Status(StatusCode::kAborted, http_response.payload);
  }
  if (http_response.status_code == HttpStatusCode::kGone) {
    return Status(StatusCode::kNotFound, http_response.payload);
  }
  if (http_response.status_code == HttpStatusCode::kLengthRequired) {
    return Status(StatusCode::kInvalidArgument, http_response.payload);
  }
  if (http_response.status_code == HttpStatusCode::kPreconditionFailed) {
    return Status(StatusCode::kFailedPrecondition, http_response.payload);
  }
  if (http_response.status_code == HttpStatusCode::kPayloadTooLarge) {
    return Status(StatusCode::kOutOfRange, http_response.payload);
  }
  if (http_response.status_code ==
      HttpStatusCode::kRequestRangeNotSatisfiable) {
    return Status(StatusCode::kOutOfRange, http_response.payload);
  }
  if (http_response.status_code == HttpStatusCode::kTooManyRequests) {
    return Status(StatusCode::kUnavailable, http_response.payload);
  }
  if (HttpStatusCode::kMinRequestErrors <= http_response.status_code &&
      http_response.status_code < HttpStatusCode::kMinInternalErrors) {
    // 4XX - A request error.
    return Status(StatusCode::kInvalidArgument, http_response.payload);
  }
  if (http_response.status_code == HttpStatusCode::kInternalServerError) {
    return Status(StatusCode::kUnavailable, http_response.payload);
  }
  if (http_response.status_code == HttpStatusCode::kBadGateway) {
    return Status(StatusCode::kUnavailable, http_response.payload);
  }
  if (http_response.status_code == HttpStatusCode::kServiceUnavailable) {
    return Status(StatusCode::kUnavailable, http_response.payload);
  }
  if (HttpStatusCode::kMinInternalErrors <= http_response.status_code &&
      http_response.status_code < HttpStatusCode::kMinInvalidCode) {
    // 5XX - server errors are mapped to kInternal.
    return Status(StatusCode::kInternal, http_response.payload);
  }
  return Status(StatusCode::kUnknown, http_response.payload);
}

std::ostream& operator<<(std::ostream& os, HttpResponse const& rhs) {
  os << "status_code=" << rhs.status_code << ", headers={";
  char const* sep = "";
  for (auto const& kv : rhs.headers) {
    os << sep << kv.first << ": " << kv.second;
    sep = ", ";
  }
  return os << "}, payload=<" << rhs.payload << ">";
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
