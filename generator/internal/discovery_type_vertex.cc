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
#include "google/cloud/internal/algorithm.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/log.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "absl/types/optional.h"

// We need to use FieldDescriptor::has_optional_keyword as its "replacement"
// FieldDescriptor::has_presence does not exhibit the same behavior.
#include "google/cloud/internal/disable_deprecation_warnings.inc"

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

auto constexpr kInitialFieldNumber = 1;

absl::optional<std::string> CheckForScalarType(nlohmann::json const& j) {
  std::string type = j.value("type", "");
  if (type == "string") return "string";
  if (type == "boolean") return "bool";
  if (type == "integer") return j.value("format", "int32");
  if (type == "number") return j.value("format", "float");
  return absl::nullopt;
}

}  // namespace

DiscoveryTypeVertex::DiscoveryTypeVertex(
    std::string name, std::string package_name, nlohmann::json json,
    google::protobuf::DescriptorPool const* descriptor_pool)
    : name_(std::move(name)),
      package_name_(std::move(package_name)),
      json_(std::move(json)),
      descriptor_pool_(descriptor_pool) {
  assert(descriptor_pool != nullptr);
}

bool DiscoveryTypeVertex::IsSynthesizedRequestType() const {
  return json_.value("synthesized_request", false);
}

void DiscoveryTypeVertex::AddNeedsTypeName(std::string type_name) {
  needs_.insert(std::move(type_name));
}

void DiscoveryTypeVertex::AddNeededByTypeName(std::string type_name) {
  needed_by_.insert(std::move(type_name));
}

std::string DiscoveryTypeVertex::DetermineIntroducer(
    nlohmann::json const& field) {
  if (field.empty()) return "";
  if (field.value("required", false)) return "";
  if (field.value("type", "notarray") == "array") return "repeated ";
  // Test for map field.
  if (field.value("type", "notobject") == "object" &&
      field.contains("additionalProperties")) {
    return "";
  }
  return "optional ";
}

StatusOr<DiscoveryTypeVertex::TypeInfo>
DiscoveryTypeVertex::DetermineTypeAndSynthesis(nlohmann::json const& v,
                                               std::string const& field_name) {
  nlohmann::json const* properties_for_synthesis = nullptr;
  bool compare_package_name = false;
  bool is_message = false;
  if (v.contains("$ref"))
    return TypeInfo{std::string(v["$ref"]), true, properties_for_synthesis,
                    false, true};
  if (!v.contains("type")) {
    return internal::InvalidArgumentError(
        absl::StrFormat("field: %s has neither $ref nor type.", field_name));
  }

  std::string type = v["type"];
  auto scalar_type = CheckForScalarType(v);
  if (scalar_type) {
    return TypeInfo{*scalar_type, compare_package_name,
                    properties_for_synthesis, false, false};
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
    return TypeInfo{type, compare_package_name, &v, false, true};
  }

  if (type == "object" && v.contains("additionalProperties")) {
    // By discovery doc convention, make this a map with a string key.
    auto const& additional_properties = v["additionalProperties"];
    if (additional_properties.contains("$ref")) {
      type = std::string(additional_properties["$ref"]);
      compare_package_name = true;
    } else if (additional_properties.contains("type")) {
      std::string map_type = additional_properties["type"];
      scalar_type = CheckForScalarType(additional_properties);
      if (scalar_type) {
        map_type = *scalar_type;
      } else if (map_type == "object" &&
                 additional_properties.contains("properties")) {
        map_type = CapitalizeFirstLetter(field_name + "Item");
        properties_for_synthesis = &additional_properties;
        is_message = true;
      } else {
        return internal::InvalidArgumentError(absl::StrFormat(
            "field: %s unknown type: %s for map field.", field_name, map_type));
      }
      type = map_type;
    } else {
      return internal::InvalidArgumentError(absl::StrFormat(
          "field: %s is a map with neither $ref nor type.", field_name));
    }
    return TypeInfo{type, compare_package_name, properties_for_synthesis, true,
                    is_message};
  }

  if (type == "array") {
    if (!v.contains("items")) {
      return internal::InvalidArgumentError(
          absl::StrFormat("field: %s array has no items.", field_name));
    }
    auto const& items = v["items"];
    if (items.contains("$ref")) {
      type = items["$ref"];
      compare_package_name = true;
      is_message = true;
    } else if (items.contains("type")) {
      type = items["type"];
      scalar_type = CheckForScalarType(items);
      if (scalar_type) {
        type = *scalar_type;
      } else if (type == "object" && items.contains("properties")) {
        // Synthesize a nested type for this array.
        type = CapitalizeFirstLetter(field_name + "Item");
        return TypeInfo{type, compare_package_name, &items, false, true};
      } else {
        return internal::InvalidArgumentError(absl::StrFormat(
            "field: %s unknown type: %s for array field.", field_name, type));
      }
    } else {
      return internal::InvalidArgumentError(absl::StrFormat(
          "field: %s is array with items having neither $ref nor type.",
          field_name));
    }
    return TypeInfo{type, compare_package_name, properties_for_synthesis, false,
                    is_message};
  }

  return internal::InvalidArgumentError(
      absl::StrFormat("field: %s has unknown type: %s.", field_name, type));
}

auto constexpr kMaxRecursionDepth = 32;
// NOLINTNEXTLINE(misc-no-recursion)
StatusOr<std::string> DiscoveryTypeVertex::FormatMessage(
    std::map<std::string, DiscoveryTypeVertex> const& types,
    std::string const& name, std::string const& qualified_name,
    std::string const& file_package_name, nlohmann::json const& json,
    int indent_level) const {
  if (indent_level > kMaxRecursionDepth) {
    GCP_LOG(FATAL) << __func__ << " exceeded kMaxRecursionDepth";
  }
  std::string indent(indent_level * 2, ' ');
  auto properties = FormatProperties(types, name, qualified_name,
                                     file_package_name, json, indent_level + 1);
  if (!properties) return std::move(properties).status();
  std::vector<std::string> message_name_parts =
      absl::StrSplit(name, absl::ByChar('.'));
  std::string reserved_numbers;
  if (!properties->reserved_numbers.empty()) {
    reserved_numbers =
        absl::StrCat(indent, "  reserved ",
                     absl::StrJoin(properties->reserved_numbers, ", "), ";\n");
  }
  return absl::StrCat(
      indent, absl::StrFormat("message %s {\n", message_name_parts.back()),
      reserved_numbers, absl::StrJoin(properties->lines, "\n\n"), "\n", indent,
      "}");
}

DiscoveryTypeVertex::MessageProperties
DiscoveryTypeVertex::DetermineReservedAndMaxFieldNumbers(
    google::protobuf::Descriptor const& message_descriptor) {
  MessageProperties message_properties{{}, {}, 0};
  for (auto r = 0; r != message_descriptor.reserved_range_count(); ++r) {
    auto const* reserved_range = message_descriptor.reserved_range(r);
    for (int j = reserved_range->start; j < reserved_range->end; ++j) {
      message_properties.reserved_numbers.insert(j);
    }
    message_properties.next_available_field_number =
        std::max(message_properties.next_available_field_number,
                 message_descriptor.reserved_range(r)->end);
  }
  for (auto i = 0; i != message_descriptor.field_count(); ++i) {
    message_properties.next_available_field_number =
        std::max(message_properties.next_available_field_number,
                 message_descriptor.field(i)->number() + 1);
  }
  return message_properties;
}

// TODO(#12234): Refactor this function into multiple smaller functions to
// reduce cognitive burden.
StatusOr<DiscoveryTypeVertex::MessageProperties>
DiscoveryTypeVertex::FormatProperties(  // NOLINT(misc-no-recursion)
    std::map<std::string, DiscoveryTypeVertex> const& types,
    std::string const& message_name, std::string const& qualified_message_name,
    std::string const& file_package_name, nlohmann::json const& json,
    int indent_level) const {
  if (indent_level > kMaxRecursionDepth) {
    GCP_LOG(FATAL) << __func__ << " exceeded kMaxRecursionDepth";
  }

  MessageProperties message_properties{{}, {}, kInitialFieldNumber};
  auto const* message_descriptor =
      descriptor_pool_->FindMessageTypeByName(qualified_message_name);
  if (message_descriptor != nullptr) {
    message_properties =
        DetermineReservedAndMaxFieldNumbers(*message_descriptor);
  }

  std::string indent(indent_level * 2, ' ');
  std::set<std::string> current_field_names;
  auto const& properties = json.find("properties");
  for (auto p = properties->begin(); p != properties->end(); ++p) {
    auto const& field = p.value();
    std::string field_name = field.value("id", p.key());
    auto type_and_synthesize = DetermineTypeAndSynthesis(field, field_name);
    if (!type_and_synthesize) return std::move(type_and_synthesize).status();

    std::string type_name = type_and_synthesize->name;
    std::string qualified_type_name = type_and_synthesize->name;
    if (type_and_synthesize->is_message) {
      qualified_type_name =
          absl::StrCat(qualified_message_name, ".", type_name);
    }

    if (type_and_synthesize->properties) {
      auto result = FormatMessage(
          types, absl::StrCat(message_name, ".", type_name),
          absl::StrCat(qualified_message_name, ".", type_name),
          file_package_name, *type_and_synthesize->properties, indent_level);
      if (!result) return std::move(result).status();
      message_properties.lines.push_back(*std::move(result));
    }

    std::string description;
    if (field.contains("description")) {
      description = absl::StrCat(
          FormatCommentBlock(std::string(field["description"]), indent_level),
          "\n");
    }

    if (field.contains("enum")) {
      std::vector<std::pair<std::string, std::string>> enum_comments;
      for (unsigned int i = 0; i < field["enum"].size(); ++i) {
        if (field.contains("enumDescriptions") &&
            i < field["enumDescriptions"].size()) {
          enum_comments.emplace_back(std::string(field["enum"][i]),
                                     std::string(field["enumDescriptions"][i]));
        } else {
          enum_comments.emplace_back(std::string(field["enum"][i]),
                                     std::string());
        }
      }
      absl::StrAppend(&description,
                      FormatCommentKeyValueList(enum_comments, indent_level),
                      "\n");
    }

    field_name = CamelCaseToSnakeCase(field_name);
    current_field_names.insert(field_name);
    if (type_and_synthesize->compare_package_name) {
      auto iter = types.find(type_and_synthesize->name);
      if (iter == types.end()) {
        return internal::InvalidArgumentError(absl::StrFormat(
            "unable to find type=%s", type_and_synthesize->name));
      }
      if (iter->second.package_name() != package_name_) {
        type_name = absl::StrCat(iter->second.package_name(), ".", type_name);
        qualified_type_name = type_name;
      } else {
        qualified_type_name = absl::StrCat(package_name(), ".", type_name);
      }
    }

    if (type_and_synthesize->is_map) {
      type_name = absl::StrFormat("map<string, %s>", type_name);
      qualified_type_name =
          absl::StrFormat("map<string, %s>", qualified_type_name);
    }

    std::string const introducer = DetermineIntroducer(field);
    auto field_number_status = GetFieldNumber(
        message_descriptor, field_name,
        (introducer.empty() ? qualified_type_name
                            : absl::StrCat(introducer, qualified_type_name)),
        message_properties.next_available_field_number);
    if (!field_number_status) return std::move(field_number_status).status();
    message_properties.lines.push_back(
        absl::StrFormat("%s%s%s%s %s = %d%s;", description, indent, introducer,
                        type_name, field_name, *field_number_status,
                        FormatFieldOptions(field_name, field)));
    if (*field_number_status ==
        message_properties.next_available_field_number) {
      ++message_properties.next_available_field_number;
    }
  }

  // Identify field numbers of deleted fields.
  if (message_descriptor != nullptr) {
    for (auto i = 0; i != message_descriptor->field_count(); ++i) {
      auto const* field_descriptor = message_descriptor->field(i);
      if (!internal::Contains(current_field_names, field_descriptor->name())) {
        message_properties.reserved_numbers.insert(field_descriptor->number());
      }
    }
  }

  return message_properties;
}

std::string DiscoveryTypeVertex::FormatFieldOptions(
    std::string const& field_name, nlohmann::json const& field_json) {
  std::vector<std::pair<std::string, std::string>> field_options;
  if (field_json.value("required", false)) {
    field_options.emplace_back("google.api.field_behavior", "REQUIRED");
  }

  if (field_json.value("operation_request_field", false)) {
    field_options.emplace_back("google.cloud.operation_request_field",
                               absl::StrCat("\"", field_name, "\""));
  }
  if (field_json.value("is_resource", false)) {
    field_options.emplace_back("json_name", "__json_request_body");
  }

  if (!field_options.empty()) {
    auto formatter = [](std::string* s,
                        std::pair<std::string, std::string> const& p) {
      if (p.first == "json_name") {
        *s += absl::StrFormat("%s=\"%s\"", p.first, p.second);
      } else {
        *s += absl::StrFormat("(%s) = %s", p.first, p.second);
      }
    };
    return absl::StrCat(" [", absl::StrJoin(field_options, ",", formatter),
                        "]");
  }

  return {};
}

StatusOr<int> DiscoveryTypeVertex::GetFieldNumber(
    google::protobuf::Descriptor const* message_descriptor,
    std::string const& field_name, std::string const& field_type,
    int candidate_field_number) {
  if (message_descriptor != nullptr) {
    auto qualified_type_name = [&](google::protobuf::FieldDescriptor const& f) {
      if (f.type() == google::protobuf::FieldDescriptor::TYPE_MESSAGE) {
        return f.message_type()->full_name();
      }
      return std::string(f.type_name());
    };

    // Keep the field number the same for existing fields.
    for (auto i = 0; i != message_descriptor->field_count(); ++i) {
      auto const* field_descriptor = message_descriptor->field(i);
      std::string type_name;
      if (field_descriptor->is_map()) {
        // Currently, all map types in discovery protos use a string key.
        type_name = absl::StrFormat(
            "map<string, %s>",
            qualified_type_name(
                *field_descriptor->message_type()->map_value()));
      } else {
        if (field_descriptor->is_repeated()) {
          type_name =
              absl::StrCat("repeated ", qualified_type_name(*field_descriptor));
        } else if (field_descriptor->has_optional_keyword()) {
          type_name =
              absl::StrCat("optional ", qualified_type_name(*field_descriptor));
        } else {
          type_name = qualified_type_name(*field_descriptor);
        }
      }

      if (field_descriptor->name() == field_name && type_name == field_type) {
        return field_descriptor->number();
      }

      if (field_descriptor->name() == field_name && type_name != field_type) {
        // Existing field type has changed, this is a breaking change.
        return internal::InvalidArgumentError(
            absl::StrFormat("Message: %s has field: %s whose type has changed "
                            "from: %s to: %s\n",
                            message_descriptor->full_name(), field_name,
                            type_name, field_type));
      }
    }
  }
  return candidate_field_number;
}

StatusOr<std::string> DiscoveryTypeVertex::JsonToProtobufMessage(
    std::map<std::string, DiscoveryTypeVertex> const& types,
    std::string const& file_package_name) const {
  int indent_level = 0;
  std::string proto;
  if (json_.contains("description")) {
    absl::StrAppend(
        &proto,
        FormatCommentBlock(std::string(json_["description"]), indent_level),
        "\n");
  }
  auto message =
      FormatMessage(types, name_, absl::StrCat(file_package_name, ".", name_),
                    file_package_name, json_, indent_level);
  if (!message) return std::move(message).status();
  absl::StrAppend(&proto, *message, "\n");
  return proto;
}

std::string DiscoveryTypeVertex::DebugString() const {
  return absl::StrFormat("name: %s; needs: %s; needed_by: %s", name_,
                         absl::StrJoin(needs_, ","),
                         absl::StrJoin(needed_by_, ","));
}

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
