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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CURL_HTTP_PAYLOAD_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CURL_HTTP_PAYLOAD_H

#include "google/cloud/internal/http_payload.h"
#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include "absl/types/span.h"
#include <map>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class CurlImpl;

// Implementation using libcurl.
class CurlHttpPayload : public HttpPayload {
 public:
  ~CurlHttpPayload() override = default;

  CurlHttpPayload(CurlHttpPayload const&) = delete;
  CurlHttpPayload(CurlHttpPayload&&) = default;
  CurlHttpPayload& operator=(CurlHttpPayload const&) = delete;
  CurlHttpPayload& operator=(CurlHttpPayload&&) = default;

  StatusOr<std::size_t> Read(absl::Span<char> buffer) override;

 private:
  friend class CurlRestResponse;
  CurlHttpPayload(std::unique_ptr<CurlImpl> impl, Options options);

  std::unique_ptr<CurlImpl> impl_;
  Options options_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_CURL_HTTP_PAYLOAD_H
