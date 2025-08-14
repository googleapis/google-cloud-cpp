// Copyright 2024 Google LLC
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

#include "google/cloud/opentelemetry/internal/time_series.h"
#include "google/cloud/opentelemetry/internal/monitored_resource.h"
#include "google/cloud/internal/absl_str_replace_quiet.h"
#include "google/cloud/internal/time_utils.h"
#include "google/cloud/log.h"
#include <opentelemetry/common/attribute_value.h>
#include <opentelemetry/sdk/metrics/data/metric_data.h>
#include <opentelemetry/sdk/metrics/export/metric_producer.h>
#include <opentelemetry/sdk/resource/semantic_conventions.h>
#include <cctype>

namespace google {
namespace cloud {
namespace otel_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

namespace sc = opentelemetry::sdk::resource::SemanticConventions;

google::protobuf::Timestamp ToProtoTimestamp(
    opentelemetry::common::SystemTimestamp ts) {
  return internal::ToProtoTimestamp(
      absl::FromUnixNanos(ts.time_since_epoch().count()));
}

google::monitoring::v3::TypedValue ToValue(
    opentelemetry::sdk::metrics::ValueType value) {
  google::monitoring::v3::TypedValue proto;
  if (absl::holds_alternative<double>(value)) {
    proto.set_double_value(absl::get<double>(value));
  } else {
    proto.set_int64_value(absl::get<std::int64_t>(value));
  }
  return proto;
}

google::api::MetricDescriptor::ValueType ToValueType(
    opentelemetry::sdk::metrics::InstrumentValueType value_type) {
  switch (value_type) {
    case opentelemetry::sdk::metrics::InstrumentValueType::kInt:
    case opentelemetry::sdk::metrics::InstrumentValueType::kLong:
      return google::api::MetricDescriptor::INT64;
    case opentelemetry::sdk::metrics::InstrumentValueType::kFloat:
    case opentelemetry::sdk::metrics::InstrumentValueType::kDouble:
      return google::api::MetricDescriptor::DOUBLE;
  }
  return google::api::MetricDescriptor::INT64;
}

double AsDouble(opentelemetry::sdk::metrics::ValueType const& v) {
  return absl::holds_alternative<double>(v)
             ? absl::get<double>(v)
             : static_cast<double>(absl::get<std::int64_t>(v));
}

std::vector<google::monitoring::v3::CreateTimeSeriesRequest> ToRequestsHelper(
    std::string const& project,
    absl::optional<google::api::MonitoredResource> const& mr_proto,
    std::vector<google::monitoring::v3::TimeSeries> tss) {
  std::vector<google::monitoring::v3::CreateTimeSeriesRequest> requests;
  for (auto& ts : tss) {
    if (requests.empty() ||
        requests.back().time_series().size() == kMaxTimeSeriesPerRequest) {
      // If the current request has hit the maximum number of TimeSeries, create
      // a new request.
      requests.emplace_back();
      requests.back().set_name(project);
    }
    if (mr_proto) {
      *ts.mutable_resource() = *mr_proto;
    }
    *requests.back().add_time_series() = std::move(ts);
  }
  return requests;
}

void ToTimeSeriesHelper(
    opentelemetry::sdk::metrics::ResourceMetrics const& data,
    std::function<
        void(opentelemetry::sdk::metrics::MetricData const& metric_data,
             opentelemetry::sdk::metrics::PointDataAttributes const& pda,
             google::monitoring::v3::TimeSeries ts)> const& ts_collector_fn) {
  for (auto const& scope_metric : data.scope_metric_data_) {
    for (auto const& metric_data : scope_metric.metric_data_) {
      for (auto const& pda : metric_data.point_data_attr_) {
        struct Visitor {
          opentelemetry::sdk::metrics::MetricData const& metric_data;

          absl::optional<google::monitoring::v3::TimeSeries> operator()(
              opentelemetry::sdk::metrics::SumPointData const& point) {
            return ToTimeSeries(metric_data, point);
          }
          absl::optional<google::monitoring::v3::TimeSeries> operator()(
              opentelemetry::sdk::metrics::LastValuePointData const& point) {
            return ToTimeSeries(metric_data, point);
          }
          absl::optional<google::monitoring::v3::TimeSeries> operator()(
              opentelemetry::sdk::metrics::HistogramPointData const& point) {
            return ToTimeSeries(metric_data, point);
          }
          absl::optional<google::monitoring::v3::TimeSeries> operator()(
              opentelemetry::sdk::metrics::DropPointData const&) {
            return absl::nullopt;
          }
        };
        auto ts = absl::visit(Visitor{metric_data}, pda.point_data);
        if (!ts) continue;
        ts->set_unit(metric_data.instrument_descriptor.unit_);
        ts_collector_fn(metric_data, pda, *std::move(ts));
      }
    }
  }
}

}  // namespace

google::api::Metric ToMetric(
    opentelemetry::sdk::metrics::MetricData const& metric_data,
    opentelemetry::sdk::metrics::PointAttributes const& attributes,
    opentelemetry::sdk::resource::Resource const* resource,
    std::function<std::string(std::string)> const& name_formatter,
    ResourceFilterDataFn const& resource_filter_fn) {
  auto add_label = [&resource_filter_fn](auto& labels, auto key,
                                         auto const& value) {
    // GCM labels match on the regex: R"([a-zA-Z_][a-zA-Z0-9_]*)".
    if (key.empty()) return;
    if (!std::isalpha(key[0]) && key[0] != '_') {
      GCP_LOG(INFO) << "Dropping metric label which does not start with "
                       "[A-Za-z_]: "
                    << key;
      return;
    }
    for (auto& c : key) {
      if (!std::isalnum(c)) c = '_';
    }
    if (resource_filter_fn && resource_filter_fn(key)) return;
    labels[std::move(key)] = AsString(value);
  };

  google::api::Metric proto;
  proto.set_type(name_formatter(metric_data.instrument_descriptor.name_));

  auto& labels = *proto.mutable_labels();
  if (resource) {
    // Copy several well-known labels from the resource into the metric, if they
    // exist.
    //
    // This avoids duplicate timeseries when multiple instances of a service are
    // running on a single monitored resource, for example running multiple
    // service processes on a single GCE VM.
    auto const& ra = resource->GetAttributes().GetAttributes();
    for (std::string key : {
             sc::kServiceName,
             sc::kServiceNamespace,
             sc::kServiceInstanceId,
         }) {
      auto it = ra.find(std::move(key));
      if (it != ra.end()) add_label(labels, it->first, it->second);
    }
  }
  for (auto const& kv : attributes) {
    add_label(labels, kv.first, kv.second);
  }
  return proto;
}

google::api::Metric ToMetric(
    opentelemetry::sdk::metrics::MetricData const& metric_data,
    opentelemetry::sdk::metrics::PointAttributes const& attributes,
    opentelemetry::sdk::resource::Resource const* resource,
    std::function<std::string(std::string)> const& name_formatter) {
  return ToMetric(metric_data, attributes, resource, name_formatter, nullptr);
}

google::monitoring::v3::TimeInterval ToNonGaugeTimeInterval(
    opentelemetry::sdk::metrics::MetricData const& metric_data) {
  // GCM requires that time intervals for non-GAUGE metrics are at least 1ms
  // long. To achieve this, we override the end value to be at least 1ms after
  // the start value.
  //
  // https://cloud.google.com/monitoring/api/ref_v3/rpc/google.monitoring.v3#timeinterval
  auto end_ts_nanos = (std::max)(metric_data.end_ts.time_since_epoch(),
                                 metric_data.start_ts.time_since_epoch() +
                                     std::chrono::milliseconds(1));

  google::monitoring::v3::TimeInterval proto;
  *proto.mutable_start_time() = ToProtoTimestamp(metric_data.start_ts);
  *proto.mutable_end_time() =
      internal::ToProtoTimestamp(absl::FromUnixNanos(end_ts_nanos.count()));
  return proto;
}

google::monitoring::v3::TimeSeries ToTimeSeries(
    opentelemetry::sdk::metrics::MetricData const& metric_data,
    opentelemetry::sdk::metrics::SumPointData const& sum_data) {
  google::monitoring::v3::TimeSeries ts;
  ts.set_metric_kind(google::api::MetricDescriptor::CUMULATIVE);
  ts.set_value_type(ToValueType(metric_data.instrument_descriptor.value_type_));

  auto& p = *ts.add_points();
  *p.mutable_interval() = ToNonGaugeTimeInterval(metric_data);
  *p.mutable_value() = ToValue(sum_data.value_);
  return ts;
}

google::monitoring::v3::TimeSeries ToTimeSeries(
    opentelemetry::sdk::metrics::MetricData const& metric_data,
    opentelemetry::sdk::metrics::LastValuePointData const& gauge_data) {
  google::monitoring::v3::TimeSeries ts;
  ts.set_metric_kind(google::api::MetricDescriptor::GAUGE);
  ts.set_value_type(ToValueType(metric_data.instrument_descriptor.value_type_));

  auto& p = *ts.add_points();
  // Note that the start timestamp is omitted for gauge metrics.
  *p.mutable_interval()->mutable_end_time() =
      ToProtoTimestamp(metric_data.end_ts);
  *p.mutable_value() = ToValue(gauge_data.value_);
  return ts;
}

google::monitoring::v3::TimeSeries ToTimeSeries(
    opentelemetry::sdk::metrics::MetricData const& metric_data,
    opentelemetry::sdk::metrics::HistogramPointData const& histogram_data) {
  google::monitoring::v3::TimeSeries ts;
  ts.set_metric_kind(google::api::MetricDescriptor::CUMULATIVE);
  ts.set_value_type(google::api::MetricDescriptor::DISTRIBUTION);

  auto& p = *ts.add_points();
  *p.mutable_interval() = ToNonGaugeTimeInterval(metric_data);
  auto& d = *p.mutable_value()->mutable_distribution_value();
  d.set_count(histogram_data.count_);
  if (histogram_data.count_ > 0) {
    d.set_mean(AsDouble(histogram_data.sum_) /
               static_cast<double>(histogram_data.count_));
  }
  auto& bounds =
      *d.mutable_bucket_options()->mutable_explicit_buckets()->mutable_bounds();
  bounds.Reserve(static_cast<int>(histogram_data.boundaries_.size()));
  for (auto d : histogram_data.boundaries_) bounds.Add(d);
  auto& counts = *d.mutable_bucket_counts();
  counts.Reserve(static_cast<int>(histogram_data.counts_.size()));
  for (auto c : histogram_data.counts_) counts.Add(c);
  return ts;
}

google::api::MonitoredResource ToMonitoredResource(
    opentelemetry::sdk::metrics::ResourceMetrics const& data,
    absl::optional<google::api::MonitoredResource> const& mr_proto) {
  if (mr_proto) return *mr_proto;
  google::api::MonitoredResource resource;
  if (data.resource_) {
    auto mr = ToMonitoredResource(data.resource_->GetAttributes());
    resource.set_type(std::move(mr.type));
    for (auto& label : mr.labels) {
      (*resource.mutable_labels())[std::move(label.first)] =
          std::move(label.second);
    }
  }
  return resource;
}

bool IsEmptyTimeSeries(
    opentelemetry::sdk::metrics::ResourceMetrics const& data) {
  for (auto const& scope_metric : data.scope_metric_data_) {
    for (auto const& metric_data : scope_metric.metric_data_) {
      if (!metric_data.point_data_attr_.empty()) return false;
    }
  }
  return true;
}

std::vector<google::monitoring::v3::TimeSeries> ToTimeSeries(
    opentelemetry::sdk::metrics::ResourceMetrics const& data,
    std::function<std::string(std::string)> const& metrics_name_formatter) {
  std::vector<google::monitoring::v3::TimeSeries> tss;
  auto collector =
      [&](opentelemetry::sdk::metrics::MetricData const& metric_data,
          opentelemetry::sdk::metrics::PointDataAttributes const& pda,
          google::monitoring::v3::TimeSeries ts) mutable {
        *ts.mutable_metric() =
            ToMetric(metric_data, pda.attributes, data.resource_,
                     metrics_name_formatter, nullptr);
        tss.push_back(std::move(ts));
      };
  ToTimeSeriesHelper(data, collector);
  return tss;
}

std::unordered_map<std::string, std::vector<google::monitoring::v3::TimeSeries>>
ToTimeSeriesWithResources(
    opentelemetry::sdk::metrics::ResourceMetrics const& data,
    std::function<std::string(std::string)> const& metrics_name_formatter,
    ResourceFilterDataFn const& resource_filter_fn,
    MonitoredResourceFromDataFn const& dynamic_resource_fn) {
  std::unordered_map<std::string,
                     std::vector<google::monitoring::v3::TimeSeries>>
      tss_map;
  auto collector =
      [&](opentelemetry::sdk::metrics::MetricData const& metric_data,
          opentelemetry::sdk::metrics::PointDataAttributes const& pda,
          google::monitoring::v3::TimeSeries ts) mutable {
        *ts.mutable_metric() =
            ToMetric(metric_data, pda.attributes, data.resource_,
                     metrics_name_formatter, resource_filter_fn);
        auto p = dynamic_resource_fn(pda);
        *ts.mutable_resource() = std::move(p.second);
        tss_map[p.first].push_back(std::move(ts));
      };
  ToTimeSeriesHelper(data, collector);
  return tss_map;
}

std::vector<google::monitoring::v3::CreateTimeSeriesRequest> ToRequests(
    std::string const& project, google::api::MonitoredResource const& mr_proto,
    std::vector<google::monitoring::v3::TimeSeries> tss) {
  return ToRequestsHelper(project, mr_proto, std::move(tss));
}

std::vector<google::monitoring::v3::CreateTimeSeriesRequest> ToRequests(
    std::string const& project,
    std::vector<google::monitoring::v3::TimeSeries> tss) {
  return ToRequestsHelper(project, absl::nullopt, std::move(tss));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel_internal
}  // namespace cloud
}  // namespace google
