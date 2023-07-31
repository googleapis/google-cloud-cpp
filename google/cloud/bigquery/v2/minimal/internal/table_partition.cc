// Copyright 2023 Google LLC
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

#include "google/cloud/bigquery/v2/minimal/internal/table_partition.h"
#include "google/cloud/bigquery/v2/minimal/internal/json_utils.h"
#include "google/cloud/internal/debug_string.h"
#include "google/cloud/internal/format_time_point.h"

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::string TimePartitioning::DebugString(absl::string_view name,
                                          TracingOptions const& options,
                                          int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("type", type)
      .Field("expiration_time", expiration_time)
      .StringField("field", field)
      .Build();
}

std::string Range::DebugString(absl::string_view name,
                               TracingOptions const& options,
                               int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("start", start)
      .StringField("end", end)
      .StringField("interval", interval)
      .Build();
}

std::string RangePartitioning::DebugString(absl::string_view name,
                                           TracingOptions const& options,
                                           int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("field", field)
      .SubMessage("range", range)
      .Build();
}

std::string Clustering::DebugString(absl::string_view name,
                                    TracingOptions const& options,
                                    int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .Field("fields", fields)
      .Build();
}

void to_json(nlohmann::json& j, TimePartitioning const& t) {
  j = nlohmann::json{{"type", t.type}, {"field", t.field}};

  ToJson(t.expiration_time, j, "expirationTime");
}
void from_json(nlohmann::json const& j, TimePartitioning& t) {
  SafeGetTo(t.type, j, "type");
  SafeGetTo(t.field, j, "field");

  FromJson(t.expiration_time, j, "expirationTime");
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
