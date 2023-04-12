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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_TRACING_HTTP_PAYLOAD_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_TRACING_HTTP_PAYLOAD_H

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#include "google/cloud/internal/http_payload.h"
#include <opentelemetry/nostd/shared_ptr.h>
#include <opentelemetry/trace/span.h>
#include <memory>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class TracingHttpPayload : public HttpPayload {
 public:
  explicit TracingHttpPayload(
      std::unique_ptr<HttpPayload> impl,
      opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span);
  ~TracingHttpPayload() override = default;

  bool HasUnreadData() const override;
  StatusOr<std::size_t> Read(absl::Span<char> buffer) override;
  std::multimap<std::string, std::string> DebugHeaders() const override;

 private:
  std::unique_ptr<HttpPayload> impl_;
  opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_TRACING_HTTP_PAYLOAD_H
