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
#include "google/cloud/internal/curl_http_payload.h"
#include "google/cloud/internal/curl_impl.h"

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

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

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
