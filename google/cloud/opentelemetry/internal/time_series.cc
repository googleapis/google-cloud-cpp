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
}

}  // namespace

google::api::Metric ToMetric(
    opentelemetry::sdk::metrics::MetricData const& metric_data,
    opentelemetry::sdk::metrics::PointAttributes const& attributes) {
  google::api::Metric proto;
  auto const metric_type =
      "workload.googleapis.com/" + metric_data.instrument_descriptor.name_;
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

google::monitoring::v3::TimeInterval ToNonEmptyTimeInterval(
    opentelemetry::sdk::metrics::MetricData const& metric_data) {
  // GCM requires that time intervals are non-empty. To achieve this, we
  // override the end value to be at least 1ms after the start value.
  // https://github.com/GoogleCloudPlatform/opentelemetry-operations-go/blob/babed4870546b78cee69606726961cfd20cbea42/exporter/metric/metric.go#L604-L609
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
  ts.set_unit(metric_data.instrument_descriptor.unit_);
  ts.set_metric_kind(google::api::MetricDescriptor::CUMULATIVE);
  ts.set_value_type(ToValueType(metric_data.instrument_descriptor.value_type_));

  auto& p = *ts.add_points();
  *p.mutable_interval() = ToNonEmptyTimeInterval(metric_data);
  *p.mutable_value() = ToValue(sum_data.value_);
  return ts;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel_internal
}  // namespace cloud
}  // namespace google
