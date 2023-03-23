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

#ifndef GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_DISCOVERY_TYPE_VERTEX_H
#define GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_DISCOVERY_TYPE_VERTEX_H

#include "google/cloud/status_or.h"
#include <nlohmann/json.hpp>
#include <set>
#include <string>

namespace google {
namespace cloud {
namespace generator_internal {

// This class represents a type found in the Discovery Document, either
// explicitly defined as a `schema`, or synthesized from a `method` `parameters`
// and `request`.
// It functions as a graph vertex belonging to two different, but
// related, graphs representing composition dependencies for other types that
// this type depends on and for other types that depend on this type. Both sets
// of edges are unidirectional, and both of the resulting graphs are acyclic.
class DiscoveryTypeVertex {
 public:
  DiscoveryTypeVertex();
  DiscoveryTypeVertex(std::string name, nlohmann::json json);

  std::string const& name() const { return name_; }
  nlohmann::json const& json() const { return json_; }

  // Adds edge to this vertex for a type name that exists as a field in this
  // type.
  void AddNeedsTypeName(std::string type_name);

  // Adds edge to this vertex for a type name that contains this type as a
  // field.
  void AddNeededByTypeName(std::string type_name);

  struct TypeInfo {
    std::string name;
    // Non-owning pointer to the properties JSON block of the type to be
    // synthesized.
    nlohmann::json const* properties;
    bool is_map;
  };
  // Determines the type of the field and if a definition of that nested type
  // needs to be defined in the message.
  // Returns a pair containing the name of the type and possibly the json
  // that defines the type.
  static StatusOr<TypeInfo> DetermineTypeAndSynthesis(
      nlohmann::json const& v, std::string const& field_name);

  std::string DebugString() const;

 private:
  std::string name_;
  nlohmann::json json_;
  std::set<std::string> needs_;
  std::set<std::string> needed_by_;
};

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_DISCOVERY_TYPE_VERTEX_H
