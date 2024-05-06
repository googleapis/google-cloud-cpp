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
#include <cctype>

namespace google {
namespace cloud {
namespace otel_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

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

}  // namespace

google::api::Metric ToMetric(
    opentelemetry::sdk::metrics::MetricData const& metric_data,
    opentelemetry::sdk::metrics::PointAttributes const& attributes,
    std::string const& prefix) {
  google::api::Metric proto;
  auto const metric_type = prefix + metric_data.instrument_descriptor.name_;
  proto.set_type(metric_type);

  auto& labels = *proto.mutable_labels();
  for (auto const& kv : attributes) {
    auto key = kv.first;
    // GCM labels match on the regex: R"([a-zA-Z_][a-zA-Z0-9_]*)".
    if (key.empty()) continue;
    if (!std::isalpha(key[0]) && key[0] != '_') {
      GCP_LOG(INFO) << "Dropping metric label which does not start with "
                       "[A-Za-z_]: "
                    << key;
      continue;
    }
    for (auto& c : key) {
      if (!std::isalnum(c)) c = '_';
    }
    labels[std::move(key)] = AsString(kv.second);
  }
  return proto;
}

google::monitoring::v3::TimeInterval ToNonGaugeTimeInterval(
    opentelemetry::sdk::metrics::MetricData const& metric_data) {
  // GCM requires that time intervals for non-GAUGE metrics are at least 1ms
  // long. To achieve this, we override the end value to be at least 1ms after
  // the start value.
  //
  // https://cloud.google.com/monitoring/api/ref_v3/rpc/google.monitoring.v3#timeinterval
  auto end_ts_nanos = (std::max)(
      metric_data.end_ts.time_since_epoch(),
      metric_data.start_ts.time_since_epoch() + std::chrono::milliseconds(1));

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

std::vector<google::monitoring::v3::TimeSeries> ToTimeSeries(
    opentelemetry::sdk::metrics::ResourceMetrics const& data,
    std::string const& prefix) {
  std::vector<google::monitoring::v3::TimeSeries> tss;
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
        *ts->mutable_metric() = ToMetric(metric_data, pda.attributes, prefix);
        tss.push_back(*std::move(ts));
      }
    }
  }
  return tss;
}

std::vector<google::monitoring::v3::CreateTimeSeriesRequest> ToRequests(
    std::string const& project, google::api::MonitoredResource const& mr_proto,
    std::vector<google::monitoring::v3::TimeSeries> tss) {
  std::vector<google::monitoring::v3::CreateTimeSeriesRequest> requests;
  for (auto& ts : tss) {
    if (requests.empty() ||
        requests.back().time_series().size() == kMaxTimeSeriesPerRequest) {
      // If the current request is full (i.e. it has 200 TimeSeries), create a
      // new request.
      requests.emplace_back();
      requests.back().set_name(project);
    }
    *ts.mutable_resource() = mr_proto;
    *requests.back().add_time_series() = std::move(ts);
  }
  return requests;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel_internal
}  // namespace cloud
}  // namespace google
