// Copyright 2026 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ATTEMPT_METRICS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ATTEMPT_METRICS_H

#include "google/cloud/storage/version.h"
#include <opentelemetry/common/attribute_value.h>
#include <opentelemetry/metrics/meter.h>
#include <opentelemetry/metrics/meter_provider.h>
#include <opentelemetry/metrics/sync_instruments.h>
#include <opentelemetry/nostd/shared_ptr.h>
#include <opentelemetry/nostd/unique_ptr.h>
#include <cstdint>
#include <string>
#include <string_view>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// Thread-safe recorder for attempt-scoped OpenTelemetry Storage counters
/// (`attempts`, `errors`, and `retries`). No-op when `provider` is null.
class AttemptMetrics {
 public:
  AttemptMetrics(opentelemetry::nostd::shared_ptr<
                     opentelemetry::metrics::MeterProvider> const& provider,
                 std::string server_address, std::string rpc_system);

  void RecordGrpcAttempt(std::string_view operation,
                         std::string_view error_type,
                         std::string_view rpc_status_code,
                         std::string_view network_path) const;

  void RecordHttpAttempt(std::string_view operation,
                         std::string_view error_type,
                         std::int64_t http_status_code,
                         std::string_view network_path) const;

  void RecordRetry(std::string_view operation, std::string_view error_type,
                   std::string_view network_path) const;

 private:
  void RecordAttemptImpl(
      std::string_view operation, std::string_view error_type,
      std::string_view network_path, char const* status_key,
      opentelemetry::common::AttributeValue status_value) const;

  std::string version_;
  std::string server_address_;
  std::string rpc_system_;
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter> meter_;
  opentelemetry::nostd::unique_ptr<
      opentelemetry::metrics::Counter<std::uint64_t>>
      attempts_;
  opentelemetry::nostd::unique_ptr<
      opentelemetry::metrics::Counter<std::uint64_t>>
      errors_;
  opentelemetry::nostd::unique_ptr<
      opentelemetry::metrics::Counter<std::uint64_t>>
      retries_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ATTEMPT_METRICS_H
