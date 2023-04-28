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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_TABLE_PARTITION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_TABLE_PARTITION_H

#include "google/cloud/version.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <string>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// Disabling clang-tidy here as the namespace is needed for using the
// NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT.
using namespace nlohmann::literals;  // NOLINT

struct TimePartitioning {
  std::string type;
  std::chrono::milliseconds expiration_time;
  std::string field;
};

struct Range {
  std::string start;
  std::string end;
  std::string interval;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Range, start, end, interval);

struct RangePartitioning {
  std::string field;
  Range range;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(RangePartitioning, field,
                                                range);

struct Clustering {
  std::vector<std::string> fields;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Clustering, fields);

inline void to_json(nlohmann::json& j, TimePartitioning const& t) {
  j = nlohmann::json{
      {"type", t.type},
      {"field", t.field},
      {"expiration_time",
       std::chrono::duration_cast<std::chrono::milliseconds>(t.expiration_time)
           .count()}};
}

inline void from_json(nlohmann::json const& j, TimePartitioning& t) {
  if (j.contains("type")) j.at("type").get_to(t.type);
  if (j.contains("field")) j.at("field").get_to(t.field);
  if (j.contains("expiration_time")) {
    std::int64_t millis;
    j.at("expiration_time").get_to(millis);
    t.expiration_time = std::chrono::milliseconds(millis);
  }
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_TABLE_PARTITION_H
