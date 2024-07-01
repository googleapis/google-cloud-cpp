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

#include "google/cloud/internal/tracing_http_payload.h"
#include "google/cloud/internal/opentelemetry.h"
#include <chrono>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

TracingHttpPayload::TracingHttpPayload(
    std::unique_ptr<HttpPayload> impl,
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span)
    : impl_(std::move(impl)), span_(std::move(span)) {}

bool TracingHttpPayload::HasUnreadData() const {
  return impl_->HasUnreadData();
}

StatusOr<std::size_t> TracingHttpPayload::Read(absl::Span<char> buffer) {
  auto scope = opentelemetry::trace::Scope(span_);
  auto const start = std::chrono::system_clock::now();
  auto status = impl_->Read(buffer);
  auto const latency = std::chrono::duration_cast<std::chrono::microseconds>(
                           std::chrono::system_clock::now() - start)
                           .count();
  if (!status) {
    auto const code = StatusCodeToString(status.status().code());
    span_->AddEvent(
        "gl-cpp.read", opentelemetry::common::SystemTimestamp(start),
        {{"read.status.code", code},
         {"read.buffer.size", static_cast<std::int64_t>(buffer.size())},
         {"read.latency.us", static_cast<std::int64_t>(latency)}});
    return internal::EndSpan(*span_, std::move(status));
  }
  span_->AddEvent(
      "gl-cpp.read", opentelemetry::common::SystemTimestamp(start),
      {{"read.buffer.size", static_cast<std::int64_t>(buffer.size())},
       {"read.returned.size", static_cast<std::int64_t>(*status)},
       {"read.latency.us", static_cast<std::int64_t>(latency)}});
  if (*status != 0) return status;
  return internal::EndSpan(*span_, std::move(status));
}

std::multimap<std::string, std::string> TracingHttpPayload::DebugHeaders()
    const {
  return impl_->DebugHeaders();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
