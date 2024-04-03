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
#include "google/cloud/log.h"
#include <opentelemetry/common/attribute_value.h>
#include <cctype>

namespace google {
namespace cloud {
namespace otel_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

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

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel_internal
}  // namespace cloud
}  // namespace google
