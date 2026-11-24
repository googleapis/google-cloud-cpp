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

#include "google/cloud/storage/internal/operation_metrics.h"
#include "google/cloud/storage/internal/metrics_constants.h"
#include "google/cloud/version.h"
#include "absl/strings/str_cat.h"
#include <opentelemetry/common/attribute_value.h>
#include <opentelemetry/common/key_value_iterable_view.h>
#include <opentelemetry/context/context.h>
#include <opentelemetry/nostd/span.h>
#include <opentelemetry/nostd/string_view.h>
#include <array>
#include <chrono>
#include <utility>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using Attr = std::pair<opentelemetry::nostd::string_view,
                       opentelemetry::common::AttributeValue>;

opentelemetry::nostd::string_view ToNostd(std::string_view sv) {
  return opentelemetry::nostd::string_view(sv.data(), sv.size());
}

std::string SemanticVersionString() {
  return absl::StrCat(google::cloud::version_major(), ".",
                      google::cloud::version_minor(), ".",
                      google::cloud::version_patch());
}

}  // namespace

OperationMetrics::OperationMetrics(
    opentelemetry::nostd::shared_ptr<
        opentelemetry::metrics::MeterProvider> const& provider,
    std::string server_address, std::string rpc_system)
    : client_version_(SemanticVersionString()),
      server_address_(std::move(server_address)),
      rpc_system_(std::move(rpc_system)) {
  if (!provider) return;
  meter_ =
      provider->GetMeter(kStorageMeterName, google::cloud::version_string());
  if (!meter_) return;
  operations_ = meter_->CreateUInt64Counter(
      ToNostd(kOperationsMetric),
      "Number of logical GCS operations initiated by the client.",
      "{operation}");
  request_duration_ = meter_->CreateDoubleHistogram(
      ToNostd(kRequestDurationMetric),
      "Application-level end-to-end latency of logical GCS operations.", "s");
}

void OperationMetrics::RecordOperation(
    OperationLabels const& labels, std::chrono::steady_clock::duration elapsed,
    opentelemetry::context::Context const& ctx) const {
  if (!operations_ && !request_duration_) return;
  // `rpc.method` is placed last so `operations_` can exclude it via a prefix
  // span of size `attrs.size() - 1`.
  std::array<Attr, 9> const attrs{{
      {"gcp.client.version", ToNostd(client_version_)},
      {"gcp.client.artifact",
       opentelemetry::nostd::string_view("google-cloud-cpp")},
      {"gcp.client.service", opentelemetry::nostd::string_view("storage")},
      {"server.address", ToNostd(server_address_)},
      {"rpc.system.name", ToNostd(rpc_system_)},
      {"gcp.storage.operation", ToNostd(labels.operation)},
      {"gcp.client.method", ToNostd(labels.client_method)},
      {"error.type", ToNostd(labels.error_type)},
      {"rpc.method", ToNostd(labels.rpc_method)},
  }};
  if (operations_) {
    opentelemetry::nostd::span<Attr const> const op_attrs(attrs.data(),
                                                          attrs.size() - 1);
    operations_->Add(std::uint64_t{1},
                     opentelemetry::common::MakeKeyValueIterableView(op_attrs),
                     ctx);
  }
  if (request_duration_) {
    double const duration_seconds =
        elapsed < std::chrono::steady_clock::duration::zero()
            ? 0.0
            : std::chrono::duration<double>(elapsed).count();
    request_duration_->Record(
        duration_seconds,
        opentelemetry::common::MakeKeyValueIterableView(attrs), ctx);
  }
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
