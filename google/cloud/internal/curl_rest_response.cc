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

#include "google/cloud/internal/curl_rest_response.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/curl_http_payload.h"
#include "google/cloud/internal/curl_impl.h"
#include "google/cloud/internal/rest_parse_json_error.h"
#include "google/cloud/log.h"
#include <iostream>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {
// Maps HTTP status codes to enumerators in `StatusCode`. The code here is
// organized by increasing range (or value) of the errors, just to make it
// readable. There are probably shorter (and/or more efficient) ways to write
// this, but we went for readability.
StatusCode MapHttpCodeToStatus(long code) {  // NOLINT(google-runtime-int)
  // We treat the 100s (e.g. 100 Continue) as OK results. They normally are
  // ignored by libcurl, so we do not really expect to see them.
  if (HttpStatusCode::kMinContinue <= code &&
      code < HttpStatusCode::kMinSuccess) {
    return StatusCode::kOk;
  }
  // We treat the 200s as Okay results.
  if (HttpStatusCode::kMinSuccess <= code &&
      code < HttpStatusCode::kMinRedirects) {
    return StatusCode::kOk;
  }
  if (code == HttpStatusCode::kResumeIncomplete) {
    // TODO(#7876): Determine if this mapping is correct for all GCP services,
    // not just GCS.

    // 308 - Resume Incomplete: this one is terrible. When performing a PUT
    // for a resumable upload this means "The client and server are out of sync
    // in this resumable upload, please reset". Unfortunately, during a
    // "reset" this means "The reset worked, here is the next committed byte,
    // keep in mind that the server is still doing work".  The second is more
    // like a kOk, the first is more like a kFailedPrecondition.
    // This level of complexity / detail is something that the caller should
    // handle, i.e., the mapping depends on the operation.
    return StatusCode::kFailedPrecondition;
  }
  if (code == HttpStatusCode::kNotModified) {
    // 304 - Not Modified: evidently GCS returns 304 for some failed
    // pre-conditions. It is somewhat strange that it also returns this error
    // code for downloads, which is always read-only and was not going to modify
    // anything. In any case, it seems too confusing to return anything other
    // than kFailedPrecondition here.
    return StatusCode::kFailedPrecondition;
  }
  if (HttpStatusCode::kMinRedirects <= code &&
      code < HttpStatusCode::kMinRequestErrors) {
    // The 300s should be handled by libcurl, we should not get them, according
    // to the Google Cloud Storage documentation these are:
    // 302 - Found
    // 303 - See Other
    // 307 - Temporary Redirect
    return StatusCode::kUnknown;
  }
  if (code == HttpStatusCode::kBadRequest) {
    return StatusCode::kInvalidArgument;
  }
  if (code == HttpStatusCode::kUnauthorized) {
    return StatusCode::kUnauthenticated;
  }
  if (code == HttpStatusCode::kForbidden) {
    return StatusCode::kPermissionDenied;
  }
  if (code == HttpStatusCode::kNotFound) {
    return StatusCode::kNotFound;
  }
  if (code == HttpStatusCode::kMethodNotAllowed) {
    return StatusCode::kPermissionDenied;
  }
  if (code == HttpStatusCode::kRequestTimeout) {
    // GCS uses a 408 to signal that an upload has suffered a broken
    // connection, and that the client should retry.
    return StatusCode::kUnavailable;
  }
  if (code == HttpStatusCode::kConflict) {
    return StatusCode::kAborted;
  }
  if (code == HttpStatusCode::kGone) {
    return StatusCode::kNotFound;
  }
  if (code == HttpStatusCode::kLengthRequired) {
    return StatusCode::kInvalidArgument;
  }
  if (code == HttpStatusCode::kPreconditionFailed) {
    return StatusCode::kFailedPrecondition;
  }
  if (code == HttpStatusCode::kPayloadTooLarge) {
    return StatusCode::kOutOfRange;
  }
  if (code == HttpStatusCode::kRequestRangeNotSatisfiable) {
    return StatusCode::kOutOfRange;
  }
  if (code == HttpStatusCode::kTooManyRequests) {
    return StatusCode::kUnavailable;
  }
  if (HttpStatusCode::kMinRequestErrors <= code &&
      code < HttpStatusCode::kMinInternalErrors) {
    // 4XX - A request error.
    return StatusCode::kInvalidArgument;
  }
  if (code == HttpStatusCode::kInternalServerError) {
    return StatusCode::kUnavailable;
  }
  if (code == HttpStatusCode::kBadGateway) {
    return StatusCode::kUnavailable;
  }
  if (code == HttpStatusCode::kServiceUnavailable) {
    return StatusCode::kUnavailable;
  }
  if (HttpStatusCode::kMinInternalErrors <= code &&
      code < HttpStatusCode::kMinInvalidCode) {
    // 5XX - server errors are mapped to kInternal.
    return StatusCode::kInternal;
  }
  return StatusCode::kUnknown;
}

}  // namespace

Status AsStatus(HttpStatusCode http_status_code, std::string payload) {
  auto const status_code = MapHttpCodeToStatus(http_status_code);
  if (status_code == StatusCode::kOk) return {};
  if (payload.empty()) {
    // If there's no payload, create one to make sure the original http status
    // code received is available.
    return Status(status_code, "Received HTTP status code: " +
                                   std::to_string(http_status_code));
  }

  auto p =
      ParseJsonError(static_cast<int>(http_status_code), std::move(payload));
  return Status(status_code, std::move(p.first), std::move(p.second));
}

CurlRestResponse::CurlRestResponse(Options options,
                                   std::unique_ptr<CurlImpl> impl)
    : impl_(std::move(impl)), options_(std::move(options)) {}

HttpStatusCode CurlRestResponse::StatusCode() const {
  return impl_->status_code();
}

std::multimap<std::string, std::string> CurlRestResponse::Headers() const {
  return impl_->headers();
}

std::unique_ptr<HttpPayload> CurlRestResponse::ExtractPayload() && {
  // Constructor is private so make_unique is not an option here.
  return std::unique_ptr<CurlHttpPayload>(
      new CurlHttpPayload(std::move(impl_), std::move(options_)));
}

Status AsStatus(RestResponse&& response) {
  auto const http_status_code = response.StatusCode();
  auto payload = rest_internal::ReadAll(std::move(response).ExtractPayload());
  if (!payload.ok()) return AsStatus(http_status_code, "");
  return AsStatus(http_status_code, *std::move(payload));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
