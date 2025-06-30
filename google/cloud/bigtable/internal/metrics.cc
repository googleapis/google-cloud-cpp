// Copyright 2025 Google LLC
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

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS

#include "google/cloud/bigtable/internal/metrics.h"
#include "google/cloud/bigtable/table_resource.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include <opentelemetry/context/runtime_context.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h>
#include <opentelemetry/sdk/metrics/meter_provider_factory.h>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

struct ClusterZone {
  std::string cluster;
  std::string zone;
};

ClusterZone ProcessMetadata(
    std::multimap<grpc::string_ref, grpc::string_ref> const& metadata) {
  ClusterZone cz;
  for (auto const& kv : metadata) {
    auto key = std::string{kv.first.data(), kv.first.size()};

    if (absl::StartsWith(key, "x-goog-ext-425905942-bin")) {
      absl::string_view v{kv.second.data(), kv.second.size()};
      v = absl::StripAsciiWhitespace(v);
      std::vector<std::string> parts = absl::StrSplit(v, '-');
      cz.zone =
          absl::StrCat(parts[0], "-", parts[1], "-", parts[2].substr(0, 1));
      cz.cluster =
          absl::StrCat(parts[2].substr(1), "-", parts[3], "-", parts[4]);
    }
  }
  return cz;
}

}  // namespace

std::ostream& operator<<(std::ostream& os, ResourceLabels const& r) {
  return os << r.project_id << "/" << r.instance << "/" << r.table << "/"
            << r.cluster << "/" << r.zone;
}

LabelMap IntoMap(ResourceLabels const& r, DataLabels const& d) {
  return {
      {"project_id", r.project_id},
      {"instance", r.instance},
      {"table", r.table},
      {"cluster", r.cluster},
      {"zone", r.zone},
      {"method", d.method},
      {"streaming", d.streaming},
      {"client_name", d.client_name},
      {"client_uid", d.client_uid},
      {"app_profile", d.app_profile},
      {"status", d.status},
  };
}

std::ostream& operator<<(std::ostream& os, LabelMap const& m) {
  return os << absl::StrJoin(
             m, ", ",
             [](std::string* out, std::pair<std::string, std::string> p) {
               out->append(absl::StrCat(p.first, ":", p.second));
             });
}

Metric::~Metric() = default;

AttemptLatency::AttemptLatency(
    std::string const& name, std::string const& version,
    std::shared_ptr<opentelemetry::metrics::MeterProvider> const& provider)
    : attempt_latencies_(provider->GetMeter(name, version)
                             ->CreateDoubleHistogram("attempt_latencies")
                             .release()) {}

void AttemptLatency::PreCall(opentelemetry::context::Context const&,
                             PreCallParams p) {
  attempt_start_ = std::move(p.attempt_start);
}

void AttemptLatency::PostCall(
    opentelemetry::context::Context const& context,
    std::multimap<grpc::string_ref, grpc::string_ref> const& initial_metadata,
    std::multimap<grpc::string_ref, grpc::string_ref> const& trailing_metadata,
    PostCallParams p) {
  auto cluster_zone = ProcessMetadata(trailing_metadata);
  resource_labels_.cluster = cluster_zone.cluster;
  resource_labels_.zone = cluster_zone.zone;
  data_labels_.status = StatusCodeToString(p.attempt_status.code());
  using dmilliseconds = std::chrono::duration<double, std::milli>;
  auto attempt_elapsed =
      std::chrono::duration_cast<dmilliseconds>(p.attempt_end - attempt_start_);
  auto m = IntoMap(resource_labels_, data_labels_);
  attempt_latencies_->Record(attempt_elapsed.count(), std::move(m), context);
}

std::unique_ptr<Metric> AttemptLatency::clone(ResourceLabels resource_labels,
                                              DataLabels data_labels) const {
  auto m = std::make_unique<AttemptLatency>(*this);
  m->resource_labels_ = std::move(resource_labels);
  m->data_labels_ = std::move(data_labels);
  return m;
}

OperationLatency::OperationLatency(
    std::string const& name, std::string const& version,
    std::shared_ptr<opentelemetry::metrics::MeterProvider> const& provider)
    : operation_latencies_(provider->GetMeter(name, version)
                               ->CreateDoubleHistogram("operation_latencies")
                               .release()) {}

void OperationLatency::PreCall(opentelemetry::context::Context const&,
                               PreCallParams p) {
  operation_start_ = std::move(p.attempt_start);
}

void OperationLatency::PostCall(
    opentelemetry::context::Context const& context,
    std::multimap<grpc::string_ref, grpc::string_ref> const& initial_metadata,
    std::multimap<grpc::string_ref, grpc::string_ref> const& trailing_metadata,
    PostCallParams p) {
  auto cluster_zone = ProcessMetadata(trailing_metadata);
  resource_labels_.cluster = cluster_zone.cluster;
  resource_labels_.zone = cluster_zone.zone;
}

void OperationLatency::OnDone(opentelemetry::context::Context const& context,
                              OnDoneParams p) {
  data_labels_.status = StatusCodeToString(p.operation_status.code());
  using dmilliseconds = std::chrono::duration<double, std::milli>;
  auto operation_elapsed = std::chrono::duration_cast<dmilliseconds>(
      p.operation_end - operation_start_);
  operation_latencies_->Record(operation_elapsed.count(),
                               IntoMap(resource_labels_, data_labels_),
                               context);
}

std::unique_ptr<Metric> OperationLatency::clone(ResourceLabels resource_labels,
                                                DataLabels data_labels) const {
  auto m = std::make_unique<OperationLatency>(*this);
  m->resource_labels_ = std::move(resource_labels);
  m->data_labels_ = std::move(data_labels);
  return m;
}

RetryCount::RetryCount(
    std::string const& name, std::string const& version,
    std::shared_ptr<opentelemetry::metrics::MeterProvider> const& provider)
    : retry_count_(provider->GetMeter(name, version)
                       ->CreateUInt64Counter("retry_count")
                       .release()) {}

void RetryCount::PreCall(opentelemetry::context::Context const&,
                         PreCallParams) {
  retry_count_->Add(1, IntoMap(resource_labels_, data_labels_));
}

void RetryCount::PostCall(
    opentelemetry::context::Context const& context,
    std::multimap<grpc::string_ref, grpc::string_ref> const& initial_metadata,
    std::multimap<grpc::string_ref, grpc::string_ref> const& trailing_metadata,
    PostCallParams p) {
  auto cluster_zone = ProcessMetadata(trailing_metadata);
  resource_labels_.cluster = cluster_zone.cluster;
  resource_labels_.zone = cluster_zone.zone;
}

std::unique_ptr<Metric> RetryCount::clone(ResourceLabels resource_labels,
                                          DataLabels data_labels) const {
  auto m = std::make_unique<RetryCount>(*this);
  m->resource_labels_ = std::move(resource_labels);
  m->data_labels_ = std::move(data_labels);
  return m;
}

FirstResponseLatency::FirstResponseLatency(
    std::string const& name, std::string const& version,
    std::shared_ptr<opentelemetry::metrics::MeterProvider> const& provider)
    : first_response_latencies_(
          provider->GetMeter(name, version)
              ->CreateDoubleHistogram("first_response_latencies")
              .release()) {}

void FirstResponseLatency::PreCall(opentelemetry::context::Context const&,
                                   PreCallParams p) {
  operation_start_ = std::move(p.attempt_start);
}

void FirstResponseLatency::PostCall(
    opentelemetry::context::Context const& context,
    std::multimap<grpc::string_ref, grpc::string_ref> const& initial_metadata,
    std::multimap<grpc::string_ref, grpc::string_ref> const& trailing_metadata,
    PostCallParams p) {
  auto cluster_zone = ProcessMetadata(trailing_metadata);
  resource_labels_.cluster = cluster_zone.cluster;
  resource_labels_.zone = cluster_zone.zone;
}

void FirstResponseLatency::ElementDelivery(
    opentelemetry::context::Context const& context, ElementDeliveryParams p) {
  using dmilliseconds = std::chrono::duration<double, std::milli>;
  if (p.first_response) {
    auto first_response_elapsed = std::chrono::duration_cast<dmilliseconds>(
        p.element_delivery - operation_start_);
    first_response_latencies_->Record(first_response_elapsed.count(),
                                      IntoMap(resource_labels_, data_labels_),
                                      context);
  }
}

std::unique_ptr<Metric> FirstResponseLatency::clone(
    ResourceLabels resource_labels, DataLabels data_labels) const {
  auto m = std::make_unique<FirstResponseLatency>(*this);
  m->resource_labels_ = std::move(resource_labels);
  m->data_labels_ = std::move(data_labels);
  return m;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
