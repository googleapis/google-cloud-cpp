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

#include "google/cloud/internal/rest_response.h"
#include "google/cloud/internal/rest_parse_json_error.h"

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {
StatusCode MapHttpCodeToStatus0xx(std::int32_t) {
  // We treat anything less than 100 as kUnknown.
  return StatusCode::kUnknown;
}

StatusCode MapHttpCodeToStatus1xx(std::int32_t) {
  // We treat the 100s (e.g. 100 Continue) as OK results. They normally are
  // ignored by libcurl, so we do not really expect to see them.
  return StatusCode::kOk;
}

StatusCode MapHttpCodeToStatus2xx(std::int32_t) {
  // We treat the 100s (e.g. 100 Continue) as OK results. They normally are
  // ignored by libcurl, so we do not really expect to see them.
  return StatusCode::kOk;
}

StatusCode MapHttpCodeToStatus3xx(std::int32_t code) {
  if (code == HttpStatusCode::kResumeIncomplete) {
    // 308 - Resume Incomplete: this one is terrible. In GCS this has two
    // meanings:
    // - When performing a PUT for a resumable upload this means "The client and
    //   server are out of sync in this resumable upload, please reset". Akin to
    //   kAborted (which implies "retry at a higher level").
    // - During a "reset" this means "The reset worked, here is the next
    //   committed byte, keep in mind that the server is still doing work". This
    //   is a success status.
    //
    // This level of complexity / detail is something that the caller should
    // handle.
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
  // The 300s should be handled by libcurl, we should not get them.
  return StatusCode::kUnknown;
}

StatusCode MapHttpCodeToStatus4xx(std::int32_t code) {
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
  // 4XX - A request error.
  return StatusCode::kInvalidArgument;
}

StatusCode MapHttpCodeToStatus5xx(std::int32_t code) {
  if (code == HttpStatusCode::kInternalServerError) {
    return StatusCode::kUnavailable;
  }
  if (code == HttpStatusCode::kBadGateway) {
    return StatusCode::kUnavailable;
  }
  if (code == HttpStatusCode::kServiceUnavailable) {
    return StatusCode::kUnavailable;
  }
  // 5XX - server errors are mapped to kInternal.
  return StatusCode::kInternal;
}

}  // namespace

StatusCode MapHttpCodeToStatus(std::int32_t code) {
  if (code < 100) return MapHttpCodeToStatus0xx(code);
  if (code < 200) return MapHttpCodeToStatus1xx(code);
  if (code < 300) return MapHttpCodeToStatus2xx(code);
  if (code < 400) return MapHttpCodeToStatus3xx(code);
  if (code < 500) return MapHttpCodeToStatus4xx(code);
  if (code < 600) return MapHttpCodeToStatus5xx(code);
  return StatusCode::kUnknown;
}

bool IsHttpSuccess(RestResponse const& response) {
  static_assert(HttpStatusCode::kMinSuccess < HttpStatusCode::kMinNotSuccess,
                "Invalid HTTP code success range");
  return response.StatusCode() < HttpStatusCode::kMinNotSuccess &&
         response.StatusCode() >= HttpStatusCode::kMinSuccess;
}

bool IsHttpError(RestResponse const& response) {
  return !IsHttpSuccess(response);
}

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
