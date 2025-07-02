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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JSON_UTILS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JSON_UTILS_H

#include "google/cloud/version.h"
#include <nlohmann/json.hpp>
#include <chrono>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::int64_t GetNumberFromJson(nlohmann::json const& j, char const* name);

void FromJson(std::chrono::milliseconds& field, nlohmann::json const& j,
              char const* name);

void ToJson(std::chrono::milliseconds const& field, nlohmann::json& j,
            char const* name);
void ToIntJson(std::chrono::milliseconds const& field, nlohmann::json& j,
               char const* name);

void FromJson(std::chrono::system_clock::time_point& field,
              nlohmann::json const& j, char const* name);

void ToJson(std::chrono::system_clock::time_point const& field,
            nlohmann::json& j, char const* name);

void FromJson(std::chrono::hours& field, nlohmann::json const& j,
              char const* name);

void ToJson(std::chrono::hours const& field, nlohmann::json& j,
            char const* name);

// Removes not needed keys and empty arrays and objects from the json
// payload.
nlohmann::json RemoveJsonKeysAndEmptyFields(
    std::string const& json_payload, std::vector<std::string> const& keys = {});

// Suppress recursive clang-tidy warnings
//
// NOLINTBEGIN(misc-no-recursion)
template <typename ResponseType>
bool SafeGetTo(ResponseType& value, nlohmann::json const& j,
               std::string const& key) {
  auto i = j.find(key);
  if (i != j.end()) {
    // BQ sends null type values which crashes get_to() so check for null.
    if (!i->is_null()) {
      i->get_to(value);
    }
    return true;
  }
  return false;
}

template <typename T>
bool SafeGetTo(std::shared_ptr<T>& value, nlohmann::json const& j,
               std::string const& key) {
  auto i = j.find(key);
  if (i == j.end()) return false;
  // BQ sends null type values which crashes get_to() so check for null.
  if (!i->is_null()) {
    if (value == nullptr) {
      value = std::make_shared<T>();
    }
    i->get_to(*value);
  }
  return true;
}

template <typename C, typename T, typename R>
void SafeGetTo(nlohmann::json const& j, std::string const& key, R& (C::*f)(T) &,
               C& obj) {
  auto i = j.find(key);
  if (i != j.end()) {
    (obj.*f)(i->get<T>());
  }
}

// Same as SafeGetTo but also returns if value was null.
template <typename ResponseType>
bool SafeGetToWithNullable(ResponseType& value, bool& is_null,
                           nlohmann::json const& j, std::string const& key) {
  auto i = j.find(key);
  is_null = false;
  if (i != j.end()) {
    // BQ sends null type values which crashes get_to() so check for null.
    if (!i->is_null()) {
      // JSON value for the field, from the server, can be anything.
      // In 80% cases its a string value at which point we retrieve
      // the string value.
      // For any other values, (like nested Arrays or objects) we just give back
      // a string to be parsed by the caller. The library has no idea what to
      // expect in the value field it can be anything based on what the client
      // sends hence its not feasible to parse here. This is best done on the
      // client side based on the client-usecase and column schema returned by
      // the server for each of these values.
      if (i->type() == nlohmann::json::value_t::string) {
        i->get_to(value);
      } else {
        value = i->dump();
      }
    } else {
      is_null = true;
    }
    return true;
  }
  return false;
}
// NOLINTEND(misc-no-recursion)

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JSON_UTILS_H
