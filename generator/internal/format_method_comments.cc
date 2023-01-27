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

#include "generator/internal/format_method_comments.h"
#include "generator/internal/codegen_utils.h"
#include "generator/internal/descriptor_utils.h"
#include "generator/internal/predicate_utils.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/absl_str_replace_quiet.h"
#include "google/cloud/log.h"
#include <google/longrunning/operations.pb.h>
#include <google/protobuf/descriptor.h>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

std::string DefineReferenceLink(
    google::protobuf::Descriptor const& message_type) {
  google::protobuf::SourceLocation loc;
  message_type.GetSourceLocation(&loc);
  std::string output_type_proto_file_name = message_type.file()->name();
  return absl::StrCat("@googleapis_reference_link{",
                      output_type_proto_file_name, "#L", loc.start_line + 1,
                      "}");
}

}  // namespace

std::string FormatMethodComments(
    google::protobuf::MethodDescriptor const& method,
    std::string const& variable_parameter_comments) {
  google::protobuf::SourceLocation method_source_location;
  if (!method.GetSourceLocation(&method_source_location) ||
      method_source_location.leading_comments.empty()) {
    GCP_LOG(FATAL) << __FILE__ << ":" << __LINE__ << ": " << method.full_name()
                   << " no leading_comments to format";
  }
  std::string doxygen_formatted_function_comments =
      absl::StrReplaceAll(method_source_location.leading_comments,
                          {{"\n", "\n  ///"}, {"$", "$$"}});

  auto const options_comment = std::string{
      R"""(  /// @param opts Optional. Override the class-level options, such as retry and
  ///     backoff policies.
)"""};

  std::string return_comment_string;
  if (IsLongrunningOperation(method)) {
    return_comment_string =
        "  /// @return $method_longrunning_deduced_return_doxygen_link$\n";
  } else if (IsBidirStreaming(method)) {
    return_comment_string =
        "  /// @return A bidirectional streaming interface with request "
        "(write) type: " +
        FormatDoxygenLink(*method.input_type()) +
        " and response (read) type: $method_return_doxygen_link$\n";
  } else if (!IsResponseTypeEmpty(method) && !IsPaginated(method)) {
    return_comment_string = "  /// @return $method_return_doxygen_link$\n";
  } else if (IsPaginated(method)) {
    return_comment_string =
        "  /// @return $method_paginated_return_doxygen_link$\n";
  }

  std::string trailer = "  ///\n";
  trailer += "  /// [" + method.input_type()->full_name() + "]: ";
  trailer += DefineReferenceLink(*method.input_type()) + "\n";
  if (IsLongrunningOperation(method)) {
    auto const info =
        method.options().GetExtension(google::longrunning::operation_info);
    auto const response_type_name =
        info.response_type() == "google.protobuf.Empty" ? info.metadata_type()
                                                        : info.response_type();
    auto const& pool = *method.file()->pool();
    auto const* message = pool.FindMessageTypeByName(response_type_name);
    if (message == nullptr) {
      message = pool.FindMessageTypeByName(method.file()->package() + "." +
                                           response_type_name);
    }
    if (message == nullptr) {
      absl::StrAppend(&trailer, "  /// [", response_type_name,
                      "]: ", ProtoNameToCppName(response_type_name), "\n");
    } else {
      absl::StrAppend(&trailer, "  /// [", message->full_name(),
                      "]: ", DefineReferenceLink(*message), "\n");
    }

  } else if (IsPaginated(method)) {
    auto info = DeterminePagination(method);
    if (info->second != nullptr) {
      trailer += "  /// [" + info->second->full_name() + "]: ";
      trailer += DefineReferenceLink(*info->second) + "\n";
    }
  } else if (!IsResponseTypeEmpty(method)) {
    trailer += "  /// [" + method.output_type()->full_name() + "]: ";
    trailer += DefineReferenceLink(*method.output_type()) + "\n";
  }

  trailer += "  ///\n";
  return absl::StrCat("  ///\n  ///", doxygen_formatted_function_comments, "\n",
                      variable_parameter_comments, options_comment,
                      return_comment_string, trailer);
}

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
