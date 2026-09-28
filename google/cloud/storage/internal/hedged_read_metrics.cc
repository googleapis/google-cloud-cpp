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

#include "google/cloud/storage/internal/hedged_read_metrics.h"
#include "google/cloud/version.h"

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

HedgedReadMetrics::HedgedReadMetrics(
    opentelemetry::nostd::shared_ptr<
        opentelemetry::metrics::MeterProvider> const& provider) {
  if (!provider) return;
  meter_ = provider->GetMeter("gl-cpp", google::cloud::version_string());
  if (!meter_) return;
  hedges_dispatched_ = meter_->CreateUInt64Counter(
      "storage.read_hedging.hedges_dispatched",
      "The number of hedge read attempts dispatched.", "{hedge}");
  hedge_won_ = meter_->CreateUInt64Counter(
      "storage.read_hedging.hedge_won",
      "The number of raced reads won by a hedge attempt.", "{read}");
}

void HedgedReadMetrics::OnHedgeDispatched() {
  if (hedges_dispatched_) hedges_dispatched_->Add(1);
}

void HedgedReadMetrics::OnHedgeWon() {
  if (hedge_won_) hedge_won_->Add(1);
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
