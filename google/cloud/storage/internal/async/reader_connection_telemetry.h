// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_READER_CONNECTION_TELEMETRY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_READER_CONNECTION_TELEMETRY_H

#include "google/cloud/storage/async/reader_connection.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/version.h"
#include <chrono>
#include <string>
#include <string_view>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class ReaderConnectionTelemetry {
 public:
  void RecordRead(
      storage::ReadPayload const& payload,
      std::chrono::steady_clock::time_point t7, std::string const& bucket_name,
      opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> const& span,
      std::string_view event_name) const;

 private:
#ifdef GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS
  void RecordMetrics(
      std::string const& bucket_name, double p1, double p2, double p3,
      opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> const& span)
      const;
#endif
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_READER_CONNECTION_TELEMETRY_H
