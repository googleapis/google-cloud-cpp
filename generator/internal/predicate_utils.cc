// Copyright 2020 Google LLC
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

#include "generator/internal/predicate_utils.h"
#include "google/cloud/log.h"
#include "google/cloud/optional.h"
#include <string>

namespace google {
namespace cloud {
namespace generator_internal {

using ::google::protobuf::Descriptor;
using ::google::protobuf::FieldDescriptor;
using ::google::protobuf::MethodDescriptor;

// https://google.aip.dev/client-libraries/4233
google::cloud::optional<std::pair<std::string, std::string>>
DeterminePagination(google::protobuf::MethodDescriptor const& method) {
  std::string paginated_type;
  Descriptor const* request_message = method.input_type();
  FieldDescriptor const* page_size =
      request_message->FindFieldByName("page_size");
  if (!page_size || page_size->type() != FieldDescriptor::TYPE_INT32) return {};
  FieldDescriptor const* page_token =
      request_message->FindFieldByName("page_token");
  if (!page_token || page_token->type() != FieldDescriptor::TYPE_STRING)
    return {};

  Descriptor const* response_message = method.output_type();
  FieldDescriptor const* next_page_token =
      response_message->FindFieldByName("next_page_token");
  if (!next_page_token ||
      next_page_token->type() != FieldDescriptor::TYPE_STRING)
    return {};

  std::vector<std::tuple<std::string, Descriptor const*, int>>
      repeated_message_fields;
  for (int i = 0; i < response_message->field_count(); ++i) {
    FieldDescriptor const* field = response_message->field(i);
    if (field->is_repeated() &&
        field->type() == FieldDescriptor::TYPE_MESSAGE) {
      repeated_message_fields.emplace_back(std::make_tuple(
          field->name(), field->message_type(), field->number()));
    }
  }

  if (repeated_message_fields.empty()) return {};

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
                     << method.full_name() << std::endl;
      std::exit(1);
    }
  }
  return std::make_pair(std::get<0>(repeated_message_fields[0]),
                        std::get<1>(repeated_message_fields[0])->full_name());
}

bool IsPaginated(google::protobuf::MethodDescriptor const& method) {
  return DeterminePagination(method).has_value();
}

bool IsNonStreaming(google::protobuf::MethodDescriptor const& method) {
  return !method.client_streaming() && !method.server_streaming();
}

bool IsLongrunningOperation(google::protobuf::MethodDescriptor const& method) {
  return method.output_type()->full_name() == "google.longrunning.Operation";
}

bool IsResponseTypeEmpty(google::protobuf::MethodDescriptor const& method) {
  return method.output_type()->full_name() == "google.protobuf.Empty";
}

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
