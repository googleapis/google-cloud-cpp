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

#include "google/cloud/bigtable/internal/metrics.h"
#include "google/cloud/bigtable/table_resource.h"
#include "google/cloud/opentelemetry/monitoring_exporter.h"
#include "google/cloud/common_options.h"
#include <opentelemetry/context/runtime_context.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h>
#include <opentelemetry/sdk/metrics/meter_context_factory.h>
#include <opentelemetry/sdk/metrics/meter_provider_factory.h>
// #include <google/api/monitored_resource.pb.h>
#include "google/cloud/internal/absl_str_join_quiet.h"

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

otel::LabelMap IntoMap(MetricLabels labels) {
  // Annoyingly, we need to split the table resource name into its components.
  // TODO : we need to handle errors, although the client should never give us
  // bad input.
  auto tr = bigtable::MakeTableResource(labels.table_name);
  auto const& instance = tr->instance();

  return {
      {"method", labels.method},
      {"streaming", labels.streaming},
      {"client_name", labels.client_name},
      {"client_uid", labels.client_uid},
      {"project_id", instance.project_id()},
      {"instance", instance.instance_id()},
      {"table", tr->table_id()},
      {"app_profile", labels.app_profile},
      {"cluster", labels.cluster},
      {"zone", labels.zone},
      {"status", labels.status},
  };
}

otel::LabelMap IntoMap(ResourceLabels const& r, DataLabels const& d) {
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

std::ostream& operator<<(std::ostream& os, otel::LabelMap const& m) {
  return os << absl::StrJoin(
             m, ", ",
             [](std::string* out, std::pair<std::string, std::string> p) {
               out->append(absl::StrCat(p.first, ":", p.second));
             });
}

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
namespace {

class OTelMetrics : public Metrics, std::enable_shared_from_this<OTelMetrics> {
 public:
  OTelMetrics();
  //~OTelMetrics() override = default;

  otel::LabelMap const& labels() const override { return *labels_; }

  void RecordAttemptLatency(double value,
                            MetricLabels const& labels) const override {
    static int call_number = 0;
    if (call_number++ % 500 == 0) {
      std::cout << __func__ << std::endl;
    }

    auto context = opentelemetry::context::RuntimeContext::GetCurrent();
    if (!labels_.has_value()) {
      labels_ = IntoMap(labels);
    }
    attempt_latencies_->Record(value, IntoMap(labels), context);
  }

  void RecordOperationLatency(double value,
                              MetricLabels const& labels) const override {
    static int call_number = 0;
    if (call_number++ % 500 == 0) {
      std::cout << __func__ << std::endl;
    }

    auto context = opentelemetry::context::RuntimeContext::GetCurrent();
    if (!labels_.has_value()) {
      labels_ = IntoMap(labels);
    }
    operation_latencies_->Record(value, IntoMap(labels), context);
  }

 private:
  mutable absl::optional<otel::LabelMap> labels_;
  // The exporter is in here somewhere.
  std::shared_ptr<opentelemetry::metrics::MeterProvider> provider_;

  // The meters.
  std::shared_ptr<opentelemetry::metrics::Histogram<double>> attempt_latencies_;
  std::shared_ptr<opentelemetry::metrics::Histogram<double>>
      operation_latencies_;
  // TODO : consider making meters for the global metric provider, if one is
  // set.
};

OTelMetrics::OTelMetrics() {
  // TODO : what is the project? I assume we get it from the request.
  auto project = Project("cloud-cpp-testing-resources");

  // Hardcode the monitored resource override, for now.
  // TODO : support dynamic MRs.
  //  auto monitored_resource = google::api::MonitoredResource{};
  //  monitored_resource.set_type("bigtable_client_raw");
  //  auto& labels = *monitored_resource.mutable_labels();
  //  labels["project_id"] = "cloud-cpp-testing-resources";
  //  labels["instance"] = "test-instance";
  //  labels["cluster"] = "test-instance-c1";
  //  labels["table"] = "endurance";
  //  labels["zone"] = "us-central1-f";

  //  auto self = shared_from_this();
  auto lambda =
      [self = this](opentelemetry::sdk::common::AttributeMap const& attr_map) {
        std::cout << ": MonitoredResourceFactory lambda called" << std::endl;
        google::api::MonitoredResource mr;
        mr.set_type("bigtable_client_raw");
        auto& labels = *mr.mutable_labels();

        auto const& l = self->labels();
        labels["project_id"] = l.find("project_id")->second;
        labels["instance"] = l.find("instance")->second;
        labels["cluster"] = l.find("cluster")->second;
        labels["table"] = l.find("table")->second;
        labels["zone"] = l.find("zone")->second;

        //    auto get_field = [](opentelemetry::sdk::common::AttributeMap
        //    const& attr_map,
        //                        std::string const& field) {
        //      auto iter = attr_map.find(field);
        //      if (iter == attr_map.end()) {
        //        std::cout << "AttributeMap: key NOT found=" << field <<
        //        std::endl; return std::string{};
        //      }
        //
        //      std::cout << "AttributeMap: key found=" << field << std::endl;
        //      auto v = iter->second;
        //      if (absl::holds_alternative<std::string>(v)) {
        //        return absl::get<std::string>(v);
        //      }
        //
        //      std::cout << "AttributeMap: value does not hold string; key=" <<
        //      field << std::endl; return std::string{};
        //
        //      };
        //
        //    labels["project_id"] = get_field(attr_map, "project_id");
        //    labels["instance"] = get_field(attr_map, "instance");
        //    labels["cluster"] = get_field(attr_map, "cluster");
        //    labels["table"] = get_field(attr_map, "table");
        //    labels["zone"] = get_field(attr_map, "zone");
        return mr;
      };

  auto o =
      Options{}
          .set<LoggingComponentsOption>({"rpc"})
          .set<otel::ServiceTimeSeriesOption>(true)
          // NOTE : we need a different option for dynamic MRs.
          //          .set<otel::MonitoredResourceOption>(std::move(monitored_resource))
          .set<otel::MonitoredResourceFactoryOption>(lambda)
          .set<otel::MetricNameFormatterOption>([](auto name) {
            return "bigtable.googleapis.com/internal/client/" + name;
          });
  auto exporter =
      otel::MakeMonitoringExporter(std::move(project), std::move(o));
  auto options =
      opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions{};
  // Empirically, it seems like 30s is the minimum.
  options.export_interval_millis = std::chrono::seconds(30);
  options.export_timeout_millis = std::chrono::seconds(1);

  auto reader =
      opentelemetry::sdk::metrics::PeriodicExportingMetricReaderFactory::Create(
          std::move(exporter), options);

  auto context = opentelemetry::sdk::metrics::MeterContextFactory::Create(
      std::make_unique<opentelemetry::sdk::metrics::ViewRegistry>(),
      // NOTE : this skips OTel's built in resource detection which is more
      // confusing than helpful. (The default is {{"service_name",
      // "unknown_service" }}). And after #14930, this gets copied into our
      // resource labels. oh god why.
      opentelemetry::sdk::resource::Resource::GetEmpty());
  context->AddMetricReader(std::move(reader));
  std::cout << __func__
            << ": opentelemetry::sdk::metrics::MeterProviderFactory::Create"
            << std::endl;
  provider_ = opentelemetry::sdk::metrics::MeterProviderFactory::Create(
      std::move(context));

  auto meter = provider_->GetMeter("bigtable", "");
  attempt_latencies_ = meter->CreateDoubleHistogram("attempt_latencies");
  operation_latencies_ = meter->CreateDoubleHistogram("operation_latencies");
}
}  // namespace

std::shared_ptr<Metrics> MakeMetrics() {
  return std::make_shared<OTelMetrics>();
}

// std::shared_ptr<std::vector<std::shared_ptr<Metric>>> MakeMetricCollection()
// {
//   auto m = std::make_shared<std::vector<std::shared_ptr<Metric>>>();
//
//   // TODO : what is the project? I assume we get it from the request.
//   auto project = Project("cloud-cpp-testing-resources");
//
//   // Hardcode the monitored resource override, for now.
//   // TODO : support dynamic MRs.
//   auto monitored_resource = google::api::MonitoredResource{};
//   monitored_resource.set_type("bigtable_client_raw");
//   auto& labels = *monitored_resource.mutable_labels();
//   labels["project_id"] = "cloud-cpp-testing-resources";
//   labels["instance"] = "test-instance";
//   labels["cluster"] = "test-cluster";
//   labels["table"] = "endurance";
//   labels["zone"] = "us-east4-a";
//
//   auto o =
//       Options{}
//           .set<LoggingComponentsOption>({"rpc"})
//           .set<otel::ServiceTimeSeriesOption>(true)
//           // NOTE : we need a different option for dynamic MRs.
//           .set<otel::MonitoredResourceOption>(std::move(monitored_resource))
//           .set<otel::MetricNameFormatterOption>([](auto name) {
//             return "bigtable.googleapis.com/internal/client/" + name;
//           });
//   auto exporter =
//       otel::MakeMonitoringExporter(std::move(project), std::move(o));
//   auto options =
//       opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions{};
//   // Empirically, it seems like 30s is the minimum.
//   options.export_interval_millis = std::chrono::seconds(30);
//   options.export_timeout_millis = std::chrono::seconds(1);
//
//   auto reader =
//       opentelemetry::sdk::metrics::PeriodicExportingMetricReaderFactory::Create(
//           std::move(exporter), options);
//
//   auto context = opentelemetry::sdk::metrics::MeterContextFactory::Create(
//       std::make_unique<opentelemetry::sdk::metrics::ViewRegistry>(),
//       // NOTE : this skips OTel's built in resource detection which is more
//       // confusing than helpful. (The default is {{"service_name",
//       // "unknown_service" }}). And after #14930, this gets copied into our
//       // resource labels. oh god why.
//       opentelemetry::sdk::resource::Resource::GetEmpty());
//   context->AddMetricReader(std::move(reader));
//   std::shared_ptr<opentelemetry::metrics::MeterProvider> provider =
//       opentelemetry::sdk::metrics::MeterProviderFactory::Create(
//           std::move(context));
//
//   m->push_back(std::make_shared<AttemptLatency>("bigtable", "", provider));
//   m->push_back(std::make_shared<OperationLatency>("bigtable", "", provider));
//   m->push_back(std::make_shared<RetryCount>("bigtable", "", provider));
//
//   return m;
// }

Metric::~Metric() = default;

#else

std::shared_ptr<Metrics> MakeMetrics() { return std::make_shared<Metrics>(); }

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
