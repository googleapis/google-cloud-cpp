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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_TRACING_REST_RESPONSE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_TRACING_REST_RESPONSE_H

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#include "google/cloud/internal/rest_response.h"
#include <opentelemetry/nostd/shared_ptr.h>
#include <opentelemetry/trace/span.h>
#include <memory>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class TracingRestResponse : public RestResponse {
 public:
  TracingRestResponse(
      std::unique_ptr<RestResponse> impl,
      opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> payload_span)
      : impl_(std::move(impl)), payload_span_(std::move(payload_span)) {}

  ~TracingRestResponse() override = default;
  HttpStatusCode StatusCode() const override;
  std::multimap<std::string, std::string> Headers() const override;
  std::unique_ptr<HttpPayload> ExtractPayload() && override;

 private:
  std::unique_ptr<RestResponse> impl_;
  opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> payload_span_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_TRACING_REST_RESPONSE_H
