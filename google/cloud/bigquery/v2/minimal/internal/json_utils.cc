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

std::int64_t GetNumberFromJson(nlohmann::json const& j, char const* name) {
  std::int64_t m = -1;
  auto const l = j.find(name);
  if (l == j.end()) return m;
  if (l->is_string()) {
    std::string s;
    l->get_to(s);
    char* end;
    m = std::strtoll(s.c_str(), &end, 10);
  } else {
    l->get_to(m);
  }
  return m;
}

void FromJson(std::chrono::milliseconds& field, nlohmann::json const& j,
              char const* name) {
  auto const m = GetNumberFromJson(j, name);
  if (m >= 0) {
    field = std::chrono::milliseconds(m);
  }
}

void ToJson(std::chrono::milliseconds const& field, nlohmann::json& j,
            char const* name) {
  auto m = static_cast<std::int64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(field).count());

  j[name] = std::to_string(m);
}

void ToIntJson(std::chrono::milliseconds const& field, nlohmann::json& j,
               char const* name) {
  auto m = static_cast<std::int64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(field).count());

  j[name] = m;
}

void FromJson(std::chrono::hours& field, nlohmann::json const& j,
              char const* name) {
  auto const m = GetNumberFromJson(j, name);
  if (m >= 0) {
    field = std::chrono::hours(m);
  }
}

void ToJson(std::chrono::hours const& field, nlohmann::json& j,
            char const* name) {
  auto m = static_cast<std::int64_t>(
      std::chrono::duration_cast<std::chrono::hours>(field).count());

  j[name] = std::to_string(m);
}

void FromJson(std::chrono::system_clock::time_point& field,
              nlohmann::json const& j, char const* name) {
  auto const m = GetNumberFromJson(j, name);
  if (m >= 0) {
    field = std::chrono::system_clock::from_time_t(0) +
            std::chrono::milliseconds(m);
  }
}

void ToJson(std::chrono::system_clock::time_point const& field,
            nlohmann::json& j, char const* name) {
  auto m = static_cast<std::int64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          field - std::chrono::system_clock::from_time_t(0))
          .count());

  j[name] = std::to_string(m);
}

nlohmann::json RemoveJsonKeysAndEmptyFields(
    std::string const& json_payload, std::vector<std::string> const& keys) {
  nlohmann::json::parser_callback_t remove_empty_call_back =
      [keys](int depth, nlohmann::json::parse_event_t event,
             nlohmann::json& parsed) {
        (void)depth;

        if (event == nlohmann::json::parse_event_t::key) {
          auto const discard = std::any_of(
              keys.begin(), keys.end(),
              [&parsed](std::string const& key) { return parsed == key; });
          return !discard;
        }
        if (event == nlohmann::json::parse_event_t::object_end) {
          return parsed != nullptr && !parsed.empty();
        }
        return true;
      };
  return nlohmann::json::parse(json_payload, remove_empty_call_back, false);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
