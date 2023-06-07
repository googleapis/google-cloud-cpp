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

#include "google/cloud/bigquery/v2/minimal/internal/json_utils.h"

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

void FromJson(std::chrono::milliseconds& field, nlohmann::json const& j,
              char const* name) {
  auto const l = j.find(name);
  if (l == j.end()) return;
  std::int64_t m;
  l->get_to(m);
  field = std::chrono::milliseconds(m);
}

void ToJson(std::chrono::milliseconds const& field, nlohmann::json& j,
            char const* name) {
  j[name] =
      std::chrono::duration_cast<std::chrono::milliseconds>(field).count();
}

void FromJson(std::chrono::hours& field, nlohmann::json const& j,
              char const* name) {
  auto const l = j.find(name);
  if (l == j.end()) return;
  std::int64_t m;
  l->get_to(m);
  field = std::chrono::hours(m);
}

void ToJson(std::chrono::hours const& field, nlohmann::json& j,
            char const* name) {
  j[name] = std::chrono::duration_cast<std::chrono::hours>(field).count();
}

void FromJson(std::chrono::system_clock::time_point& field,
              nlohmann::json const& j, char const* name) {
  auto const l = j.find(name);
  if (l == j.end()) return;
  std::int64_t m;
  l->get_to(m);
  field =
      std::chrono::system_clock::from_time_t(0) + std::chrono::milliseconds(m);
}

void ToJson(std::chrono::system_clock::time_point const& field,
            nlohmann::json& j, char const* name) {
  j[name] = std::chrono::duration_cast<std::chrono::milliseconds>(
                field.time_since_epoch())
                .count();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
