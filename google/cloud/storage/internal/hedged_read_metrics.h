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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HEDGED_READ_METRICS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HEDGED_READ_METRICS_H

#include "google/cloud/storage/version.h"
#include <opentelemetry/metrics/meter.h>
#include <opentelemetry/metrics/meter_provider.h>
#include <opentelemetry/metrics/sync_instruments.h>
#include <opentelemetry/nostd/shared_ptr.h>
#include <opentelemetry/nostd/unique_ptr.h>
#include <cstdint>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * OpenTelemetry counters for read hedging.
 *
 * The instruments are created once per connection, from the meter provider
 * given at construction, and shared by every hedged download on that
 * connection. With the default (no-op) meter provider nothing is recorded.
 *
 * - `storage.read_hedging.hedges_dispatched`: hedge attempts dispatched.
 * - `storage.read_hedging.hedge_won`: raced reads won by a hedge attempt.
 *
 * Both counters are monotonic and carry no attributes. This class is
 * thread-safe.
 */
class HedgedReadMetrics {
 public:
  explicit HedgedReadMetrics(
      opentelemetry::nostd::shared_ptr<
          opentelemetry::metrics::MeterProvider> const& provider);

  /// Records that a hedge attempt was dispatched.
  void OnHedgeDispatched();

  /// Records that a raced read was won by a hedge attempt.
  void OnHedgeWon();

 private:
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter> meter_;
  opentelemetry::nostd::unique_ptr<
      opentelemetry::metrics::Counter<std::uint64_t>>
      hedges_dispatched_;
  opentelemetry::nostd::unique_ptr<
      opentelemetry::metrics::Counter<std::uint64_t>>
      hedge_won_;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HEDGED_READ_METRICS_H
