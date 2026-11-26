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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_PAYLOAD_METRICS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_PAYLOAD_METRICS_H

#include "google/cloud/storage/version.h"
#include <opentelemetry/context/context.h>
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

/// Labels for recording request or response payload body sizes.
struct PayloadLabels {
  std::string_view operation;
  std::string_view client_method;
};

/// Thread-safe recorder for payload size OpenTelemetry Storage metrics
/// (`gcp.storage.client.request.body.size` and
/// `gcp.storage.client.response.body.size`).
/// When constructed with a null provider, all methods are no-ops.
class PayloadMetrics {
 public:
  PayloadMetrics(opentelemetry::nostd::shared_ptr<
                     opentelemetry::metrics::MeterProvider> const& provider,
                 std::string server_address, std::string rpc_system);

  void RecordRequestBodySize(PayloadLabels const& labels, std::uint64_t bytes,
                             opentelemetry::context::Context const& ctx) const;

  void RecordResponseBodySize(PayloadLabels const& labels, std::uint64_t bytes,
                              opentelemetry::context::Context const& ctx) const;

 private:
  void RecordBodySize(
      opentelemetry::metrics::Histogram<std::uint64_t>* histogram,
      PayloadLabels const& labels, std::uint64_t bytes,
      opentelemetry::context::Context const& ctx) const;

  std::string client_version_;
  std::string server_address_;
  std::string rpc_system_;
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter> meter_;
  opentelemetry::nostd::unique_ptr<
      opentelemetry::metrics::Histogram<std::uint64_t>>
      request_body_size_;
  opentelemetry::nostd::unique_ptr<
      opentelemetry::metrics::Histogram<std::uint64_t>>
      response_body_size_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_PAYLOAD_METRICS_H
