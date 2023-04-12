// Copyright 2023 Google LLC
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

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#include "google/cloud/internal/tracing_rest_response.h"
#include "google/cloud/internal/tracing_http_payload.h"

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

HttpStatusCode TracingRestResponse::StatusCode() const {
  return impl_->StatusCode();
}

std::multimap<std::string, std::string> TracingRestResponse::Headers() const {
  return impl_->Headers();
}

std::unique_ptr<HttpPayload> TracingRestResponse::ExtractPayload() && {
  auto payload = std::move(*impl_).ExtractPayload();
  return std::make_unique<TracingHttpPayload>(std::move(payload),
                                              std::move(payload_span_));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
