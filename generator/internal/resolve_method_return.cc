// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "generator/internal/resolve_method_return.h"
#include "generator/internal/longrunning.h"
#include "generator/internal/pagination.h"
#include "google/longrunning/operations.pb.h"
#include <google/protobuf/descriptor.h>
#include <string>
#include <utility>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

ProtoDefinitionLocation Location(google::protobuf::Descriptor const& d) {
  google::protobuf::SourceLocation loc;
  d.GetSourceLocation(&loc);
  return ProtoDefinitionLocation{std::string{d.file()->name()},
                                 loc.start_line + 1};
}

}  // namespace

absl::optional<std::pair<std::string, ProtoDefinitionLocation>>
ResolveMethodReturn(google::protobuf::MethodDescriptor const& method) {
  auto const* message = method.output_type();
  if (!message) return absl::nullopt;
  // There is no need to document the return type, the generated code treats
  // this as `void`.
  if (message->full_name() == "google.protobuf.Empty") return absl::nullopt;

  if (IsPaginated(method)) {
    auto info = DeterminePagination(method);
    // For string pagination we return nothing, there is no need to link the
    // definition of the `std::string` type.
    if (!info->range_output_type) return absl::nullopt;
    return std::make_pair(std::string{info->range_output_type->full_name()},
                          Location(*info->range_output_type));
  }

  if (IsLongrunningOperation(method)) {
    auto info =
        method.options().GetExtension(google::longrunning::operation_info);
    auto const name = info.response_type() == "google.protobuf.Empty"
                          ? info.metadata_type()
                          : info.response_type();
    message = method.file()->pool()->FindMessageTypeByName(name);
    if (!message) {
      // Qualify the name and try again
      auto const fqname = std::string{method.file()->package()} + "." + name;
      message = method.file()->pool()->FindMessageTypeByName(fqname);
      if (!message) return absl::nullopt;
    }
    return std::make_pair(std::string{message->full_name()},
                          Location(*message));
  }

  return std::make_pair(std::string{message->full_name()}, Location(*message));
}

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
