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

#include "google/cloud/storage/internal/grpc/metrics_meter_provider.h"
#include "google/cloud/storage/internal/grpc/metrics_histograms.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include <grpcpp/grpcpp.h>
#include <opentelemetry/version.h>
#if OPENTELEMETRY_VERSION_MAJOR > 1 || \
    (OPENTELEMETRY_VERSION_MAJOR == 1 && OPENTELEMETRY_VERSION_MINOR >= 10)
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h>
#include <opentelemetry/sdk/metrics/meter_provider_factory.h>
#include <opentelemetry/sdk/metrics/view/instrument_selector_factory.h>
#include <opentelemetry/sdk/metrics/view/meter_selector_factory.h>
#include <opentelemetry/sdk/metrics/view/view_factory.h>
#endif  // OPENTELEMETRY_VERSION
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h>
#include <opentelemetry/sdk/metrics/meter_provider.h>
#include <opentelemetry/sdk/metrics/view/instrument_selector.h>
#include <opentelemetry/sdk/metrics/view/meter_selector.h>
#include <opentelemetry/sdk/metrics/view/view.h>
#include <opentelemetry/sdk/resource/resource.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

void AddHistogramView(opentelemetry::sdk::metrics::MeterProvider& provider,
                      std::vector<double> boundaries, std::string const& name,
                      std::string const& unit) {
  auto constexpr kGrpcMeterName = "grpc-c++";
  auto constexpr kGrpcSchema = "";  // gRPC sets an empty schema.

  auto histogram_aggregation_config = std::make_unique<
      opentelemetry::sdk::metrics::HistogramAggregationConfig>();
  histogram_aggregation_config->boundaries_ = std::move(boundaries);
  // Type-erase and convert to shared_ptr.
  auto aggregation_config =
      std::shared_ptr<opentelemetry::sdk::metrics::AggregationConfig>(
          std::move(histogram_aggregation_config));

  auto description = absl::StrCat("A view of ", name,
                                  " with histogram boundaries more appropriate "
                                  "for Google Cloud Storage RPCs");

#if OPENTELEMETRY_VERSION_MAJOR > 1 || \
    (OPENTELEMETRY_VERSION_MAJOR == 1 && OPENTELEMETRY_VERSION_MINOR >= 10)
  provider.AddView(
      opentelemetry::sdk::metrics::InstrumentSelectorFactory::Create(
          opentelemetry::sdk::metrics::InstrumentType::kHistogram, name, unit),
      opentelemetry::sdk::metrics::MeterSelectorFactory::Create(
          kGrpcMeterName, grpc::Version(), kGrpcSchema),
      opentelemetry::sdk::metrics::ViewFactory::Create(
          name, std::move(description), unit,
          opentelemetry::sdk::metrics::AggregationType::kHistogram,
          std::move(aggregation_config)));
#else
  (void)unit;
  provider.AddView(
      std::make_unique<opentelemetry::sdk::metrics::InstrumentSelector>(
          opentelemetry::sdk::metrics::InstrumentType::kHistogram, name),
      std::make_unique<opentelemetry::sdk::metrics::MeterSelector>(
          kGrpcMeterName, grpc::Version(), kGrpcSchema),
      std::make_unique<opentelemetry::sdk::metrics::View>(
          name, std::move(description),
          opentelemetry::sdk::metrics::AggregationType::kHistogram,
          std::move(aggregation_config)));
#endif
}

void AddLatencyHistogramView(
    opentelemetry::sdk::metrics::MeterProvider& provider,
    std::string const& name) {
  return AddHistogramView(provider, MakeLatencyHistogramBoundaries(), name,
                          "s");
}

void AddSizeHistogramView(opentelemetry::sdk::metrics::MeterProvider& provider,
                          std::string const& name) {
  return AddHistogramView(provider, MakeSizeHistogramBoundaries(), name, "By");
}

}  // namespace

std::shared_ptr<opentelemetry::metrics::MeterProvider> MakeGrpcMeterProvider(
    std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> exporter,
    opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions
        reader_options) {
#if OPENTELEMETRY_VERSION_MAJOR > 1 || \
    (OPENTELEMETRY_VERSION_MAJOR == 1 && OPENTELEMETRY_VERSION_MINOR >= 10)
  auto provider = opentelemetry::sdk::metrics::MeterProviderFactory::Create(
      std::make_unique<opentelemetry::sdk::metrics::ViewRegistry>(),
      opentelemetry::sdk::resource::Resource::Create({}));
#else
  auto provider = std::make_unique<opentelemetry::sdk::metrics::MeterProvider>(
      std::make_unique<opentelemetry::sdk::metrics::ViewRegistry>(),
      opentelemetry::sdk::resource::Resource::Create({}));
#endif
  auto* p =
      static_cast<opentelemetry::sdk::metrics::MeterProvider*>(provider.get());
  AddLatencyHistogramView(*p, "grpc.client.attempt.duration");
  AddSizeHistogramView(
      *p, "grpc.client.attempt.rcvd_total_compressed_message_size");
  AddSizeHistogramView(
      *p, "grpc.client.attempt.sent_total_compressed_message_size");

#if OPENTELEMETRY_VERSION_MAJOR > 1 || OPENTELEMETRY_VERSION_MINOR >= 10
  p->AddMetricReader(
      opentelemetry::sdk::metrics::PeriodicExportingMetricReaderFactory::Create(
          std::move(exporter), std::move(reader_options)));
#else
  p->AddMetricReader(
      std::make_unique<
          opentelemetry::sdk::metrics::PeriodicExportingMetricReader>(
          std::move(exporter), std::move(reader_options)));
#endif

  return std::shared_ptr<opentelemetry::metrics::MeterProvider>(
      std::move(provider));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  //  GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
