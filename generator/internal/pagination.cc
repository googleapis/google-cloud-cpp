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
#include "generator/internal/doxygen.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
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

// Checks that the `field_name` has a message type of any of the names in
// `message_names`.
bool FieldExistsAndIsMessage(Descriptor const& d, std::string const& field_name,
                             std::vector<std::string> const& message_names) {
  FieldDescriptor const* field = d.FindFieldByName(field_name);
  if (field == nullptr) return false;
  if (field->type() != FieldDescriptor::TYPE_MESSAGE) return false;
  google::protobuf::Descriptor const* descriptor = field->message_type();
  if (descriptor == nullptr) return false;
  if (std::any_of(message_names.begin(), message_names.end(),
                  [descriptor](auto message_name) {
                    return message_name == descriptor->full_name();
                  })) {
    return true;
  };
  return false;
}

// https://google.aip.dev/client-libraries/4233
google::cloud::optional<PaginationInfo> DetermineAIP4233Pagination(
    MethodDescriptor const& method) {
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
    return PaginationInfo{
        repeated_string_fields[0].first, repeated_string_fields[0].second, {}};
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
  return PaginationInfo{std::get<0>(repeated_message_fields[0]),
                        std::get<1>(repeated_message_fields[0]),
                        {}};
}

// For both the sqladmin and compute proto definitions, the paging conventions
// do not adhere to aip-4233, but the intent is there. If we can make it work,
// add pagination for any such methods.
google::cloud::optional<PaginationInfo> DetermineAlternatePagination(
    MethodDescriptor const& method) {
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

  if (items->is_map()) {
    return PaginationInfo{std::string{items->name()},
                          items->message_type()->map_value()->message_type(),
                          items->message_type()->map_key()};
  }
  return PaginationInfo{std::string{items->name()}, items->message_type(), {}};
}

// For the BigQuery v2 proto definitions, the paging conventions
// do not adhere to aip-4233 for the following rpcs:
//   - JobService.ListJobs
//   - JobService.GetQueryResults
//   - TableService.ListTables
//   - DatasetService.ListDatasets
//   - ModelService.ListModels
//   - TableDataService.List
// This method adds custom code to handle these cases.
google::cloud::optional<PaginationInfo> DetermineBigQueryPagination(
    MethodDescriptor const& method) {
  Descriptor const* request_message = method.input_type();
  Descriptor const* response_message = method.output_type();

  if (!(FieldExistsAndIsMessage(
            *request_message, "max_results",
            {"google.protobuf.UInt32Value", "google.protobuf.Int32Value"}) ||
        FieldExistsAndIsType(*request_message, "max_results",
                             FieldDescriptor::TYPE_UINT32)) ||
      !FieldExistsAndIsType(*request_message, "page_token",
                            FieldDescriptor::TYPE_STRING) ||
      !FieldExistsAndIsType(*response_message, "next_page_token",
                            FieldDescriptor::TYPE_STRING)) {
    return {};
  }

  std::unordered_map<std::string, std::string> items_map = {
      {"jobs", "google.cloud.bigquery.v2.ListFormatJob"},
      {"datasets", "google.cloud.bigquery.v2.ListFormatDataset"},
      {"models", "google.cloud.bigquery.v2.Model"},
      {"rows", "google.protobuf.Struct"},
      {"tables", "google.cloud.bigquery.v2.ListFormatTable"}};
  for (auto const& kv : items_map) {
    auto field_name = kv.first;
    auto field_type_message_name = kv.second;
    if (FieldExistsAndIsMessage(*response_message, field_name,
                                {field_type_message_name})) {
      FieldDescriptor const* items =
          response_message->FindFieldByName(field_name);
      if (!items->is_repeated()) return {};
      return PaginationInfo{
          std::string{items->name()}, items->message_type(), {}};
    }
  }

  return {};
}

// For the BigQuery v2 proto definitions, the paging conventions
// do not adhere to aip-4233 for the following rpcs:
//   - JobService.ListJobs
//   - JobService.GetQueryResults
//   - TableService.ListTables
//   - DatasetService.ListDatasets
//   - ModelService.ListModels
//   - TableDataService.List
// This method adds custom code to handle these cases.
google::cloud::optional<PaginationInfo> DetermineBigQueryPagination(
    MethodDescriptor const& method) {
  Descriptor const* request_message = method.input_type();
  Descriptor const* response_message = method.output_type();

  if (!(FieldExistsAndIsMessage(
            *request_message, "max_results",
            {"google.protobuf.UInt32Value", "google.protobuf.Int32Value"}) ||
        FieldExistsAndIsType(*request_message, "max_results",
                             FieldDescriptor::TYPE_UINT32)) ||
      !FieldExistsAndIsType(*request_message, "page_token",
                            FieldDescriptor::TYPE_STRING) ||
      !FieldExistsAndIsType(*response_message, "next_page_token",
                            FieldDescriptor::TYPE_STRING)) {
    return {};
  }

  std::unordered_map<std::string, std::string> items_map = {
      {"jobs", "google.cloud.bigquery.v2.ListFormatJob"},
      {"datasets", "google.cloud.bigquery.v2.ListFormatDataset"},
      {"models", "google.cloud.bigquery.v2.Model"},
      {"rows", "google.protobuf.Struct"},
      {"tables", "google.cloud.bigquery.v2.ListFormatTable"}};
  for (auto const& kv : items_map) {
    auto field_name = kv.first;
    auto field_type_message_name = kv.second;
    if (FieldExistsAndIsMessage(*response_message, field_name,
                                {field_type_message_name})) {
      FieldDescriptor const* items =
          response_message->FindFieldByName(field_name);
      if (!items->is_repeated()) return {};
      return PaginationInfo{
          std::string{items->name()}, items->message_type(), {}};
    }
  }

  return {};
}

}  // namespace

google::cloud::optional<PaginationInfo> DeterminePagination(
    MethodDescriptor const& method) {
  auto result = DetermineAIP4233Pagination(method);
  if (result) return result;
  result = DetermineAlternatePagination(method);
  if (result) return result;
  return DetermineBigQueryPagination(method);
}

bool IsPaginated(MethodDescriptor const& method) {
  return DeterminePagination(method).has_value();
}

void AssignPaginationMethodVars(
    google::protobuf::MethodDescriptor const& method,
    VarsDictionary& method_vars) {
  if (!IsPaginated(method)) return;
  auto pagination_info = DeterminePagination(method);
  method_vars["range_output_field_name"] =
      pagination_info->range_output_field_name;
  // Add exception to AIP-4233 for response types that have exactly one
  // repeated field that is of primitive type string.
  if (pagination_info->range_output_type == nullptr) {
    method_vars["range_output_type"] = "std::string";
    method_vars["method_paginated_return_doxygen_link"] = "std::string";
  } else {
    if (pagination_info->range_output_map_key_type.has_value()) {
      method_vars["range_output_type"] = absl::StrCat(
          "std::pair<",
          CppTypeToString(pagination_info->range_output_map_key_type.value()),
          ", ",
          ProtoNameToCppName(pagination_info->range_output_type->full_name()),
          ">");
    } else {
      method_vars["range_output_type"] =
          ProtoNameToCppName(pagination_info->range_output_type->full_name());
    }
    method_vars["method_paginated_return_doxygen_link"] =
        FormatDoxygenLink(*pagination_info->range_output_type);
  }
}

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
