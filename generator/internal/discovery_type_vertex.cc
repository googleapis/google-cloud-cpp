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

#include "generator/internal/discovery_type_vertex.h"
#include "generator/internal/codegen_utils.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/make_status.h"
#include "absl/strings/str_format.h"
#include "absl/types/optional.h"

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

absl::optional<std::string> CheckForScalarType(nlohmann::json const& j) {
  std::string type;
  if (j.contains("type")) type = j["type"];
  if (type == "string") return "string";
  if (type == "boolean") return "bool";
  if (type == "integer") return j.value("format", "int32");
  if (type == "number") return j.value("format", "float");
  return {};
}

}  // namespace

DiscoveryTypeVertex::DiscoveryTypeVertex() : json_("") {}

DiscoveryTypeVertex::DiscoveryTypeVertex(std::string name, nlohmann::json json)
    : name_(std::move(name)), json_(std::move(json)) {}

void DiscoveryTypeVertex::AddNeedsTypeName(std::string type_name) {
  needs_.insert(std::move(type_name));
}

void DiscoveryTypeVertex::AddNeededByTypeName(std::string type_name) {
  needed_by_.insert(std::move(type_name));
}

StatusOr<DiscoveryTypeVertex::TypeInfo>
DiscoveryTypeVertex::DetermineTypeAndSynthesis(nlohmann::json const& v,
                                               std::string const& field_name) {
  nlohmann::json const* properties_for_synthesis = nullptr;
  if (v.contains("$ref"))
    return TypeInfo{std::string(v["$ref"]), properties_for_synthesis, false};
  if (!v.contains("type")) {
    return internal::InvalidArgumentError(
        absl::StrFormat("field: %s has neither $ref nor type.", field_name));
  }

  std::string type = v["type"];
  auto scalar_type = CheckForScalarType(v);
  if (scalar_type) {
    return TypeInfo{*scalar_type, properties_for_synthesis, false};
  }

  if (type == "object" &&
      !(v.contains("properties") || v.contains("additionalProperties"))) {
    return internal::InvalidArgumentError(
        absl::StrFormat("field: %s is type object with neither properties nor "
                        "additionalProperties.",
                        field_name));
  }

  if (type == "object" && v.contains("properties")) {
    // Synthesize nested type for this struct.
    type = CapitalizeFirstLetter(field_name);
    return TypeInfo{type, &v, false};
  }

  if (type == "object" && v.contains("additionalProperties")) {
    // By discovery doc convention, make this a map with a string key.
    auto const& additional_properties = v["additionalProperties"];
    if (additional_properties.contains("$ref")) {
      type = std::string(additional_properties["$ref"]);
    } else if (additional_properties.contains("type")) {
      std::string map_type = additional_properties["type"];
      scalar_type = CheckForScalarType(additional_properties);
      if (scalar_type) {
        map_type = *scalar_type;
      } else if (map_type == "object" &&
                 additional_properties.contains("properties")) {
        map_type = CapitalizeFirstLetter(field_name + "Item");
        properties_for_synthesis = &additional_properties;
      } else {
        return internal::InvalidArgumentError(absl::StrFormat(
            "field: %s unknown type: %s for map field.", field_name, map_type));
      }
      type = map_type;
    } else {
      return internal::InvalidArgumentError(absl::StrFormat(
          "field: %s is a map with neither $ref nor type.", field_name));
    }
    return TypeInfo{type, properties_for_synthesis, true};
  }

  if (type == "array") {
    if (!v.contains("items")) {
      return internal::InvalidArgumentError(
          absl::StrFormat("field: %s array has no items.", field_name));
    }
    auto const& items = v["items"];
    if (items.contains("$ref")) {
      type = items["$ref"];
    } else if (items.contains("type")) {
      type = items["type"];
      scalar_type = CheckForScalarType(items);
      if (scalar_type) {
        type = *scalar_type;
      } else if (type == "object" && items.contains("properties")) {
        // Synthesize a nested type for this array.
        type = CapitalizeFirstLetter(field_name + "Item");
        return TypeInfo{type, &items, false};
      } else {
        return internal::InvalidArgumentError(absl::StrFormat(
            "field: %s unknown type: %s for array field.", field_name, type));
      }
    } else {
      return internal::InvalidArgumentError(absl::StrFormat(
          "field: %s is array with items having neither $ref nor type.",
          field_name));
    }
    return TypeInfo{type, properties_for_synthesis, false};
  }

  return internal::InvalidArgumentError(
      absl::StrFormat("field: %s has unknown type: %s.", field_name, type));
}

std::string DiscoveryTypeVertex::DebugString() const {
  return absl::StrFormat("name: %s; needs: %s; needed_by: %s", name_,
                         absl::StrJoin(needs_, ","),
                         absl::StrJoin(needed_by_, ","));
}

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
