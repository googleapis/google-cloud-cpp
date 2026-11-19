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
#include "google/cloud/version.h"
#include <opentelemetry/common/attribute_value.h>
#include <opentelemetry/common/key_value_iterable_view.h>
#include <opentelemetry/context/runtime_context.h>
#include <opentelemetry/nostd/span.h>
#include <opentelemetry/nostd/string_view.h>
#include <array>
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

}  // namespace

OperationMetrics::OperationMetrics(
    opentelemetry::nostd::shared_ptr<
        opentelemetry::metrics::MeterProvider> const& provider,
    std::string server_address, std::string rpc_system)
    : server_address_(std::move(server_address)),
      rpc_system_(std::move(rpc_system)) {
  if (!provider) return;
  meter_ = provider->GetMeter("gl-cpp", google::cloud::version_string());
  if (!meter_) return;
  operations_ = meter_->CreateUInt64Counter(
      "gcp.storage.client.operations",
      "Number of logical GCS operations initiated by the client.",
      "{operation}");
  request_duration_ = meter_->CreateDoubleHistogram(
      "gcp.client.request.duration",
      "Application-level end-to-end latency of logical GCS operations.", "s");
}

void OperationMetrics::RecordOperation(std::string_view operation,
                                       std::string_view client_method,
                                       std::string_view rpc_method,
                                       std::string_view error_type,
                                       double duration_seconds) const {
  if (!operations_ && !request_duration_) return;
  auto const ctx = opentelemetry::context::RuntimeContext::GetCurrent();
  auto const ver = google::cloud::version_string();
  std::array<Attr, 9> const attrs{{
      {"gcp.client.version", ToNostd(ver)},
      {"gcp.client.artifact", "google-cloud-cpp"},
      {"gcp.client.service", "storage"},
      {"server.address", ToNostd(server_address_)},
      {"rpc.system.name", ToNostd(rpc_system_)},
      {"gcp.storage.operation", ToNostd(operation)},
      {"gcp.client.method", ToNostd(client_method)},
      {"error.type", ToNostd(error_type)},
      {"rpc.method", ToNostd(rpc_method)},
  }};
  if (operations_) {
    opentelemetry::nostd::span<Attr const> const op_attrs(attrs.data(), 8);
    operations_->Add(std::uint64_t{1},
                     opentelemetry::common::MakeKeyValueIterableView(op_attrs),
                     ctx);
  }
  if (request_duration_) {
    request_duration_->Record(
        duration_seconds,
        opentelemetry::common::MakeKeyValueIterableView(attrs), ctx);
  }
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
