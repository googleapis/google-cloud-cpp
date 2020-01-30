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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HTTP_RESPONSE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HTTP_RESPONSE_H

#include "google/cloud/status.h"
#include "google/cloud/storage/version.h"
#include <iosfwd>
#include <map>
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

/**
 * Contains the results of a HTTP request.
 */
struct HttpResponse {
  long status_code;
  std::string payload;
  std::multimap<std::string, std::string> headers;
};

/**
 * Maps a HTTP response to a `Status`.
 *
 * HTTP responses have a wide range of status codes (100 to 599), and we have
 * a much more limited number of `StatusCode` values. This function performs
 * the mapping between the two.
 *
 * The general principles in this mapping are:
 * - A "code" outside the valid code for HTTP (from 100 to 599 both
 *   inclusive) is always kUnknown.
 * - Codes that are not specifically documented in
 *     https://cloud.google.com/storage/docs/json_api/v1/status-codes
 *   are mapped by these rules:
 *     [100,300) -> kOk because they are all success status codes.
 *     [300,400) -> kUnknown because libcurl should handle the redirects, so
 *                  getting one is fairly strange.
 *     [400,500) -> kInvalidArgument because these are generally "the client
 *                  sent an invalid request" errors.
 *     [500,600) -> kInternal because these are "server errors".
 *
 * @return A status with the code corresponding to @p http_response.status_code,
 *     the error message in the status is initialized with
 *     @p http_response.payload.
 */
Status AsStatus(HttpResponse const& http_response);

std::ostream& operator<<(std::ostream& os, HttpResponse const& rhs);

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HTTP_RESPONSE_H
