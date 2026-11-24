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

#include "google/cloud/storage/internal/attempt_metrics.h"
#include "google/cloud/version.h"
#include <opentelemetry/common/key_value_iterable_view.h>
#include <opentelemetry/context/context.h>
#include <opentelemetry/nostd/span.h>
#include <opentelemetry/nostd/string_view.h>
#include <array>
#include <string>
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
  return std::to_string(google::cloud::version_major()) + "." +
         std::to_string(google::cloud::version_minor()) + "." +
         std::to_string(google::cloud::version_patch());
}

}  // namespace

AttemptMetrics::AttemptMetrics(
    opentelemetry::nostd::shared_ptr<
        opentelemetry::metrics::MeterProvider> const& provider,
    std::string server_address)
    : client_version_(SemanticVersionString()),
      server_address_(std::move(server_address)) {
  if (!provider) return;
  meter_ = provider->GetMeter("gl-cpp", google::cloud::version_string());
  if (!meter_) return;
  attempts_ = meter_->CreateUInt64Counter(
      "gcp.storage.client.attempts",
      "Total number of individual backend attempts made during operations.",
      "{attempt}");
  errors_ = meter_->CreateUInt64Counter(
      "gcp.storage.client.errors",
      "Number of failed backend attempts encountered by the client.",
      "{error}");
  retries_ = meter_->CreateUInt64Counter(
      "gcp.storage.client.retries",
      "Number of retry attempts initiated by the client.", "{retry}");
}

void AttemptMetrics::RecordGrpcAttempt(
    GrpcAttemptLabels const& labels,
    opentelemetry::context::Context const& ctx) const {
  RecordAttemptImpl("grpc", labels.operation, labels.error_type,
                    labels.network_path, "rpc.response.status_code",
                    ToNostd(labels.rpc_status_code), ctx);
}

void AttemptMetrics::RecordHttpAttempt(
    HttpAttemptLabels const& labels,
    opentelemetry::context::Context const& ctx) const {
  RecordAttemptImpl("http", labels.operation, labels.error_type,
                    labels.network_path, "http.response.status_code",
                    labels.http_status_code, ctx);
}

void AttemptMetrics::RecordAttemptImpl(
    std::string_view rpc_system, std::string_view operation,
    std::string_view error_type, std::string_view network_path,
    char const* status_key, opentelemetry::common::AttributeValue status_value,
    opentelemetry::context::Context const& ctx) const {
  if (!attempts_ && !errors_) return;
  // The status code attribute is placed last so `errors_` can exclude it via a
  // prefix span of size `attrs.size() - 1`.
  std::array<Attr, 9> const attrs{{
      {"gcp.client.version", ToNostd(client_version_)},
      {"gcp.client.artifact",
       opentelemetry::nostd::string_view("google-cloud-cpp")},
      {"gcp.client.service", opentelemetry::nostd::string_view("storage")},
      {"server.address", ToNostd(server_address_)},
      {"rpc.system.name", ToNostd(rpc_system)},
      {"gcp.storage.operation", ToNostd(operation)},
      {"error.type", ToNostd(error_type)},
      {"network.path", ToNostd(network_path)},
      {status_key, status_value},
  }};
  if (attempts_) {
    attempts_->Add(std::uint64_t{1},
                   opentelemetry::common::MakeKeyValueIterableView(attrs), ctx);
  }
  if (errors_ && error_type != "OK") {
    opentelemetry::nostd::span<Attr const> const err_attrs(attrs.data(),
                                                           attrs.size() - 1);
    errors_->Add(std::uint64_t{1},
                 opentelemetry::common::MakeKeyValueIterableView(err_attrs),
                 ctx);
  }
}

void AttemptMetrics::RecordRetry(
    RetryLabels const& labels,
    opentelemetry::context::Context const& ctx) const {
  if (!retries_) return;
  std::array<Attr, 8> const attrs{{
      {"gcp.client.version", ToNostd(client_version_)},
      {"gcp.client.artifact",
       opentelemetry::nostd::string_view("google-cloud-cpp")},
      {"gcp.client.service", opentelemetry::nostd::string_view("storage")},
      {"server.address", ToNostd(server_address_)},
      {"rpc.system.name", ToNostd(labels.rpc_system)},
      {"gcp.storage.operation", ToNostd(labels.operation)},
      {"error.type", ToNostd(labels.error_type)},
      {"network.path", ToNostd(labels.network_path)},
  }};
  retries_->Add(std::uint64_t{1},
                opentelemetry::common::MakeKeyValueIterableView(attrs), ctx);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
