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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_RESPONSE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_RESPONSE_H

#include "google/cloud/internal/http_payload.h"
#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <cstdint>
#include <map>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

enum HttpStatusCode : std::int32_t {
  kMinContinue = 100,
  kMinSuccess = 200,
  kMinRedirects = 300,
  kMinRequestErrors = 400,
  kMinInternalErrors = 500,
  kMinInvalidCode = 600,

  kContinue = 100,

  kOk = 200,
  kCreated = 201,

  // The libcurl library handles (most) redirects, so anything above 300 is
  // actually an error.
  kMinNotSuccess = 300,
  // This is returned in some download requests instead of 412.
  kNotModified = 304,
  // Google's resumable upload protocol abuses 308 (Permanent Redirect) as
  // "Resume Incomplete".
  kResumeIncomplete = 308,

  kBadRequest = 400,
  kUnauthorized = 401,
  kForbidden = 403,
  kNotFound = 404,
  kMethodNotAllowed = 405,
  kRequestTimeout = 408,
  kConflict = 409,
  kGone = 410,
  kLengthRequired = 411,
  kPreconditionFailed = 412,
  kPayloadTooLarge = 413,
  kRequestRangeNotSatisfiable = 416,
  kTooManyRequests = 429,

  kClientClosedRequest = 499,
  kInternalServerError = 500,
  kBadGateway = 502,
  kServiceUnavailable = 503,
};

// This class contains the results of making a request to a RESTful service.
class RestResponse {
 public:
  virtual ~RestResponse() = default;
  virtual HttpStatusCode StatusCode() const = 0;
  virtual std::multimap<std::string, std::string> Headers() const = 0;
  // Creates a HttpPayload object from the underlying HTTP response,
  // invalidating the current RestResponse object.
  virtual std::unique_ptr<HttpPayload> ExtractPayload() && = 0;
};

/// Convert a HTTP status code to a google::cloud::StatusCode.
StatusCode MapHttpCodeToStatus(std::int32_t code);

/// Determines if @p response contains a successful result.
bool IsHttpSuccess(RestResponse const& response);

/// Determines if @p response contains an error.
bool IsHttpError(RestResponse const& response);

/**
 * Maps a response to a `Status`.
 *
 * HTTP responses have a wide range of status codes (100 to 599), and we have
 * a much more limited number of `StatusCode` values. This function performs
 * the mapping between the two.
 *
 * The general principles in this mapping are:
 * - A "code" outside the valid code for HTTP (from 100 to 599 both
 *   inclusive) is always kUnknown.
 * - Codes are mapped by these rules:
 *     [100,300) -> kOk because they are all success status codes.
 *     [300,400) -> kUnknown because libcurl should handle the redirects, so
 *                  getting one is fairly strange.
 *     [400,500) -> kInvalidArgument because these are generally "the client
 *                  sent an invalid request" errors.
 *     [500,600) -> kInternal because these are "server errors".
 *
 * JSON payloads in the response following the format specified in
 * https://cloud.google.com/apis/design/errors#http_mapping are parsed and
 * added to the Status message and error_info.
 *
 * @return A status with the code corresponding to @p RestResponse::Status,
 *     the error message in the status is initialized from any content in the
 *     response payload.
 */
Status AsStatus(RestResponse&& response);
Status AsStatus(HttpStatusCode http_status_code, std::string payload);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_RESPONSE_H
