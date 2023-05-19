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

#include "generator/internal/pagination.h"
#include "generator/internal/codegen_utils.h"
#include "generator/internal/descriptor_utils.h"
#include "google/cloud/log.h"
#include "google/cloud/optional.h"

namespace google {
namespace cloud {
namespace generator_internal {

using ::google::protobuf::Descriptor;
using ::google::protobuf::FieldDescriptor;
using ::google::protobuf::MethodDescriptor;

namespace {

bool FieldExistsAndIsType(Descriptor const& d, std::string const& field_name,
                          FieldDescriptor::Type field_type) {
  FieldDescriptor const* field = d.FindFieldByName(field_name);
  return (field != nullptr && field->type() == field_type);
}

// https://google.aip.dev/client-libraries/4233
google::cloud::optional<std::pair<std::string, Descriptor const*>>
DetermineAIP4233Pagination(MethodDescriptor const& method) {
  Descriptor const* request_message = method.input_type();
  Descriptor const* response_message = method.output_type();

  if (!FieldExistsAndIsType(*request_message, "page_size",
                            FieldDescriptor::TYPE_INT32) ||
      !FieldExistsAndIsType(*request_message, "page_token",
                            FieldDescriptor::TYPE_STRING) ||
      !FieldExistsAndIsType(*response_message, "next_page_token",
                            FieldDescriptor::TYPE_STRING)) {
    return {};
  }

  std::vector<std::tuple<std::string, Descriptor const*, int>>
      repeated_message_fields;
  std::vector<std::pair<std::string, Descriptor const*>> repeated_string_fields;
  for (int i = 0; i < response_message->field_count(); ++i) {
    FieldDescriptor const* field = response_message->field(i);
    if (field->is_repeated() &&
        field->type() == FieldDescriptor::TYPE_MESSAGE) {
      repeated_message_fields.emplace_back(field->name(), field->message_type(),
                                           field->number());
    }
    if (field->is_repeated() && field->type() == FieldDescriptor::TYPE_STRING) {
      repeated_string_fields.emplace_back(field->name(), nullptr);
    }
  }

  if (repeated_message_fields.empty()) {
    // Add exception to AIP-4233 for response types that have exactly one
    // repeated field that is of primitive type string.
    if (repeated_string_fields.size() != 1) return {};
    return repeated_string_fields[0];
  }

  if (repeated_message_fields.size() > 1) {
    auto min_field = std::min_element(
        repeated_message_fields.begin(), repeated_message_fields.end(),
        [](std::tuple<std::string, Descriptor const*, int> const& lhs,
           std::tuple<std::string, Descriptor const*, int> const& rhs) {
          return std::get<2>(lhs) < std::get<2>(rhs);
        });
    int min_field_number = std::get<2>(*min_field);
    if (min_field_number != std::get<2>(repeated_message_fields[0])) {
      GCP_LOG(FATAL) << "Repeated field in paginated response must be first "
                        "appearing and lowest field number: "
                     << method.full_name();
    }
  }
  return std::make_pair(std::get<0>(repeated_message_fields[0]),
                        std::get<1>(repeated_message_fields[0]));
}

// For both the sqladmin and compute proto definitions, the paging conventions
// do not adhere to aip-4233, but the intent is there. If we can make it work,
// add pagination for any such methods.
google::cloud::optional<std::pair<std::string, Descriptor const*>>
DetermineAlternatePagination(MethodDescriptor const& method) {
  Descriptor const* request_message = method.input_type();
  Descriptor const* response_message = method.output_type();
  if (!FieldExistsAndIsType(*request_message, "max_results",
                            FieldDescriptor::TYPE_UINT32) ||
      !FieldExistsAndIsType(*request_message, "page_token",
                            FieldDescriptor::TYPE_STRING) ||
      !FieldExistsAndIsType(*response_message, "next_page_token",
                            FieldDescriptor::TYPE_STRING) ||
      !FieldExistsAndIsType(*response_message, "items",
                            FieldDescriptor::TYPE_MESSAGE)) {
    return {};
  }

  FieldDescriptor const* items = response_message->FindFieldByName("items");
  if (!items->is_repeated()) return {};
  // TODO(#11660): For map<K, V> types we need to add functionality to treat
  // them as a repeated struct which contains two fields corresponding to
  // K and V.
  if (items->is_map()) return {};
  return std::make_pair(items->name(), items->message_type());
}

}  // namespace

google::cloud::optional<std::pair<std::string, Descriptor const*>>
DeterminePagination(MethodDescriptor const& method) {
  auto result = DetermineAIP4233Pagination(method);
  if (result) return result;
  return DetermineAlternatePagination(method);
}

bool IsPaginated(MethodDescriptor const& method) {
  return DeterminePagination(method).has_value();
}

void AssignPaginationMethodVars(
    google::protobuf::MethodDescriptor const& method,
    VarsDictionary& method_vars) {
  if (IsPaginated(method)) {
    auto pagination_info = DeterminePagination(method);
    method_vars["range_output_field_name"] = pagination_info->first;
    // Add exception to AIP-4233 for response types that have exactly one
    // repeated field that is of primitive type string.
    method_vars["range_output_type"] =
        pagination_info->second == nullptr
            ? "std::string"
            : ProtoNameToCppName(pagination_info->second->full_name());
    if (pagination_info->second) {
      method_vars["method_paginated_return_doxygen_link"] =
          FormatDoxygenLink(*pagination_info->second);
    } else {
      method_vars["method_paginated_return_doxygen_link"] = "std::string";
    }
  }
}

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
