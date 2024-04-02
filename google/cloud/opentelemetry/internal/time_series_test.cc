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
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace otel_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::Pair;
using ::testing::UnorderedElementsAre;

TEST(ToMetric, Simple) {
  opentelemetry::sdk::metrics::MetricData md;
  md.instrument_descriptor.name_ = "test";

  opentelemetry::sdk::metrics::PointAttributes attributes = {
      {"key", "value"}, {"a key-with.bad/regex", "value2"}};

  auto metric = ToMetric(md, attributes);

  EXPECT_EQ(metric.type(), "workload.googleapis.com/test");
  EXPECT_THAT(metric.labels(),
              UnorderedElementsAre(Pair("key", "value"),
                                   Pair("a_key_with_bad_regex", "value2")));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel_internal
}  // namespace cloud
}  // namespace google
