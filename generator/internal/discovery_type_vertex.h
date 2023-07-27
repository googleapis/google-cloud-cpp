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
#include <google/protobuf/descriptor.h>
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
  // descriptor_pool should never be NULL, and the constructor will assert if it
  // is.
  DiscoveryTypeVertex(std::string name, std::string package_name,
                      nlohmann::json json,
                      google::protobuf::DescriptorPool const* descriptor_pool);

  std::string const& name() const { return name_; }
  std::string const& package_name() const { return package_name_; }
  nlohmann::json const& json() const { return json_; }

  bool IsSynthesizedRequestType() const;

  // Adds edge to this vertex for a type name that exists as a field in this
  // type.
  void AddNeedsTypeName(std::string type_name);

  // Adds edge to this vertex for a type name that contains this type as a
  // field.
  void AddNeededByTypeName(std::string type_name);

  // Returns "optional ", "repeated ", or an empty string depending on the
  // field type.
  static std::string DetermineIntroducer(nlohmann::json const& field);

  // TODO(#12225): Consider changing is_map and is_message from bool to enum.
  // Possibly combine the two fields into one field of an enum.
  struct TypeInfo {
    std::string name;
    bool compare_package_name;
    // Non-owning pointer to the properties JSON block of the type to be
    // synthesized.
    nlohmann::json const* properties;
    bool is_map;
    bool is_message;
  };

  // Determines the type of the field and if a definition of that nested type
  // needs to be defined in the message.
  // Returns a pair containing the name of the type and possibly the json
  // that defines the type.
  static StatusOr<TypeInfo> DetermineTypeAndSynthesis(
      nlohmann::json const& v, std::string const& field_name);

  struct MessageProperties {
    std::vector<std::string> lines;
    std::set<int> reserved_numbers;
    int next_available_field_number;
  };

  // Examines the message Descriptor to determine the reserved field numbers
  // and the next available field number based on the currently used and/or
  // reserved field numbers.
  static MessageProperties DetermineReservedAndMaxFieldNumbers(
      google::protobuf::Descriptor const& message_descriptor);

  // Formats the properties of the json into proto message fields.
  StatusOr<MessageProperties> FormatProperties(
      std::map<std::string, DiscoveryTypeVertex> const& types,
      std::string const& message_name,
      std::string const& qualified_message_name,
      std::string const& file_package_name, nlohmann::json const& json,
      int indent_level) const;

  StatusOr<std::string> FormatMessage(
      std::map<std::string, DiscoveryTypeVertex> const& types,
      std::string const& name, std::string const& qualified_name,
      std::string const& package_name, nlohmann::json const& json,
      int indent_level) const;

  // Formats any field options as indicated by the field_json.
  static std::string FormatFieldOptions(std::string const& field_name,
                                        nlohmann::json const& field_json);

  // Determines the correct field_number to use for the specified field.
  static StatusOr<int> GetFieldNumber(
      google::protobuf::Descriptor const* message_descriptor,
      std::string const& field_name, std::string const& field_type,
      int candidate_field_number);

  // Emits the protobuf message definition for this type.
  StatusOr<std::string> JsonToProtobufMessage(
      std::map<std::string, DiscoveryTypeVertex> const& types,
      std::string const& file_package_name) const;

  std::string DebugString() const;

 private:
  Status UpdateTypeNames(
      std::map<std::string, DiscoveryTypeVertex> const& types,
      TypeInfo const& type_and_synthesize, std::string& type_name,
      std::string& qualified_type_name) const;

  std::string name_;
  std::string package_name_;
  nlohmann::json json_;
  google::protobuf::DescriptorPool const* const descriptor_pool_;
  std::set<std::string> needs_;
  std::set<std::string> needed_by_;
};

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_DISCOVERY_TYPE_VERTEX_H
