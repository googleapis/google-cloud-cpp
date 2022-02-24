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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CURL_REST_RESPONSE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CURL_REST_RESPONSE_H

#include "google/cloud/internal/http_payload.h"
#include "google/cloud/internal/rest_response.h"
#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <map>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class CurlImpl;

// Implements RestResponse using libcurl.
class CurlRestResponse : public RestResponse {
 public:
  ~CurlRestResponse() override = default;

  CurlRestResponse(CurlRestResponse const&) = delete;
  CurlRestResponse(CurlRestResponse&&) = default;
  CurlRestResponse& operator=(CurlRestResponse const&) = delete;
  CurlRestResponse& operator=(CurlRestResponse&&) = default;

  HttpStatusCode StatusCode() const override;
  std::multimap<std::string, std::string> Headers() const override;
  std::unique_ptr<HttpPayload> ExtractPayload() && override;

 private:
  friend class CurlRestClient;
  CurlRestResponse(Options options, std::unique_ptr<CurlImpl> impl);

  std::unique_ptr<CurlImpl> impl_;
  Options options_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CURL_REST_RESPONSE_H
