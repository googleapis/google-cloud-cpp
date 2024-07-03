// Copyright 2024 Google LLC
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

#include "google/cloud/storage/internal/tracing_object_read_source.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/status.h"
#include <opentelemetry/trace/scope.h>
#include <chrono>
#include <utility>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

TracingObjectReadSource::TracingObjectReadSource(
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span,
    std::unique_ptr<storage::internal::ObjectReadSource> child)
    : span_(std::move(span)), child_(std::move(child)) {}

TracingObjectReadSource::~TracingObjectReadSource() {
  // A download may be abandoned by the application, or kept open after all
  // the data is received. Note that if there was an unrecoverable error the
  // span is already finished in `TracingObjectReadOSource::Read()`.
  internal::EndSpan(*span_);
}

bool TracingObjectReadSource::IsOpen() const {
  auto scope = opentelemetry::trace::Scope(span_);
  return child_->IsOpen();
}

StatusOr<storage::internal::HttpResponse> TracingObjectReadSource::Close() {
  auto scope = opentelemetry::trace::Scope(span_);
  span_->AddEvent("gl-cpp.close");
  return child_->Close();
}

StatusOr<storage::internal::ReadSourceResult> TracingObjectReadSource::Read(
    char* buf, std::size_t n) {
  auto scope = opentelemetry::trace::Scope(span_);
  auto const start = std::chrono::system_clock::now();
  auto response = child_->Read(buf, n);
  auto const latency = std::chrono::duration_cast<std::chrono::microseconds>(
                           std::chrono::system_clock::now() - start)
                           .count();

  if (!response) {
    auto const code = StatusCodeToString(response.status().code());
    span_->AddEvent("gl-cpp.read",
                    opentelemetry::common::SystemTimestamp(start),
                    {{"read.status.code", code},
                     {"read.buffer.size", static_cast<std::int64_t>(n)},
                     {"read.latency.us", static_cast<std::int64_t>(latency)}});
    return google::cloud::internal::EndSpan(*span_, std::move(response));
  }
  span_->AddEvent("gl-cpp.read", opentelemetry::common::SystemTimestamp(start),
                  {{"read.buffer.size", static_cast<std::int64_t>(n)},
                   {"read.returned.size",
                    static_cast<std::int64_t>(response->bytes_received)},
                   {"read.latency.us", static_cast<std::int64_t>(latency)}});
  return response;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
