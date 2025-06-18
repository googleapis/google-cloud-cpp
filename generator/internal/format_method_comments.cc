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
#include "generator/internal/longrunning.h"
#include "generator/internal/pagination.h"
#include "generator/internal/predicate_utils.h"
#include "generator/internal/resolve_comment_references.h"
#include "generator/internal/resolve_method_return.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/absl_str_replace_quiet.h"
#include "google/cloud/log.h"
#include "absl/strings/string_view.h"
#include "google/longrunning/operations.pb.h"
#include <google/protobuf/descriptor.h>
#include <cstddef>
#include <iterator>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

// Disable clang-format formatting in the generated comments. Some comments
// include long lines that should not be broken by newlines. For example,
// they may start a code span (something in backticks). It is also easier to
// generate markdown reference-style links (i.e. `[foo]: real link`) if the
// formatter is not breaking these across multiple lines.
auto constexpr kMethodCommentsPrefix = R"""(  // clang-format off
  ///
  ///)""";

auto constexpr kMethodCommentsSuffix = R"""(  ///
  // clang-format on
)""";

auto constexpr kDeprecationComment = R"""( @deprecated This RPC is deprecated.
  ///
  ///)""";

ProtoDefinitionLocation Location(google::protobuf::Descriptor const& d) {
  google::protobuf::SourceLocation loc;
  d.GetSourceLocation(&loc);
  return ProtoDefinitionLocation{std::string{d.file()->name()},
                                 loc.start_line + 1};
}

std::string ReturnCommentString(
    google::protobuf::MethodDescriptor const& method) {
  if (IsLongrunningOperation(method)) {
    return R"""(  /// @return A [`future`] that becomes satisfied when the LRO
  ///     ([Long Running Operation]) completes or the polling policy in effect
  ///     for this call is exhausted. The future is satisfied with an error if
  ///     the LRO completes with an error or the polling policy is exhausted.
  ///     In this case the [`StatusOr`] returned by the future contains the
  ///     error. If the LRO completes successfully the value of the future
  ///     contains the LRO's result. For this RPC the result is a
  ///     [$longrunning_deduced_response_message_type$] proto message.
  ///     The C++ class representing this message is created by Protobuf, using
  ///     the [Protobuf mapping rules].
)""";
  }
  if (IsBidirStreaming(method)) {
    return absl::StrFormat(
        R"""(  /// @return An object representing the bidirectional streaming
  ///     RPC. Applications can send multiple request messages and receive
  ///     multiple response messages through this API. Bidirectional streaming
  ///     RPCs can impose restrictions on the sequence of request and response
  ///     messages. Please consult the service documentation for details.
  ///     The request message type ([%s]) and response messages
  ///     ([%s]) are mapped to C++ classes using the
  ///     [Protobuf mapping rules].
)""",
        method.input_type()->full_name(), method.output_type()->full_name());
  }
  if (IsPaginated(method)) {
    auto const info = DeterminePagination(method);
    if (info->range_output_type == nullptr) {
      return R"""(  /// @return a [StreamRange](@ref google::cloud::StreamRange)
  ///     to iterate of the results. See the documentation of this type for
  ///     details. In brief, this class has `begin()` and `end()` member
  ///     functions returning a iterator class meeting the
  ///     [input iterator requirements]. The value type for this iterator is a
  ///     [`StatusOr`] as the iteration may fail even after some values are
  ///     retrieved successfully, for example, if there is a network disconnect.
  ///     An empty set of results does not indicate an error, it indicates
  ///     that there are no resources meeting the request criteria.
  ///     On a successful iteration the `StatusOr<T>` contains a
  ///     [`std::string`].
)""";
    }
    return absl::StrFormat(
        R"""(  /// @return a [StreamRange](@ref google::cloud::StreamRange)
  ///     to iterate of the results. See the documentation of this type for
  ///     details. In brief, this class has `begin()` and `end()` member
  ///     functions returning a iterator class meeting the
  ///     [input iterator requirements]. The value type for this iterator is a
  ///     [`StatusOr`] as the iteration may fail even after some values are
  ///     retrieved successfully, for example, if there is a network disconnect.
  ///     An empty set of results does not indicate an error, it indicates
  ///     that there are no resources meeting the request criteria.
  ///     On a successful iteration the `StatusOr<T>` contains elements of type
  ///     [%s], or rather,
  ///     the C++ class generated by Protobuf from that type. Please consult the
  ///     Protobuf documentation for details on the [Protobuf mapping rules].
)""",
        info->range_output_type->full_name());
  }
  if (IsResponseTypeEmpty(method)) {
    return R"""(  /// @return a [`Status`] object. If the request failed, the
  ///     status contains the details of the failure.
)""";
  }
  return absl::StrFormat(
      R"""(  /// @return the result of the RPC. The response message type
  ///     ([%s])
  ///     is mapped to a C++ class using the [Protobuf mapping rules].
  ///     If the request fails, the [`StatusOr`] contains the error details.
)""",
      method.output_type()->full_name());
}

auto constexpr kTrailerBeginning =
    R"""(  ///
  /// [Protobuf mapping rules]: https://protobuf.dev/reference/cpp/cpp-generated/
  /// [input iterator requirements]: https://en.cppreference.com/w/cpp/named_req/InputIterator
)""";
auto constexpr kTrailerGRPCLRO =
    R"""(  /// [Long Running Operation]: https://google.aip.dev/151
)""";

auto constexpr kTrailerComputeLRO =
    R"""(  /// [Long Running Operation]: http://cloud/compute/docs/api/how-tos/api-requests-responses#handling_api_responses
)""";

auto constexpr kTrailerEnding =
    R"""(  /// [`std::string`]: https://en.cppreference.com/w/cpp/string/basic_string
  /// [`future`]: @ref google::cloud::future
  /// [`StatusOr`]: @ref google::cloud::StatusOr
  /// [`Status`]: @ref google::cloud::Status
)""";

// std::map::merge() is not available until C++17.
std::map<std::string, ProtoDefinitionLocation> Merge(
    std::map<std::string, ProtoDefinitionLocation> preferred,
    std::map<std::string, ProtoDefinitionLocation> alternatives) {
  preferred.insert(std::make_move_iterator(alternatives.begin()),
                   std::make_move_iterator(alternatives.end()));
  return preferred;
}

// Apply substitutions to the comments snarfed from the proto file for
// RPC methods. This is mostly for the benefit of Doxygen, but is also
// to fix mismatched quotes, etc.
struct MethodCommentSubstitution {
  absl::string_view before;
  absl::string_view after;
  std::size_t uses = 0;
};

auto constexpr kDialogflowEsConversationsProto = R"""(
 `create_time_epoch_microseconds >
 [first item's create_time of previous request]` and empty page_token.)""";

auto constexpr kDialogflowEsConversationsCpp = R"""(
 `create_time_epoch_microseconds > [first item's create_time of previous request]`
 and empty page_token.)""";

MethodCommentSubstitution substitutions[] = {
    // From google/logging/v2/logging_config.proto
    {R"""(Gets a view on a log bucket..)""",
     R"""(Gets a view on a log bucket.)"""},

    // From google/dialogflow/v2/conversation.proto
    {kDialogflowEsConversationsProto, kDialogflowEsConversationsCpp},

    // Add Doxygen-style comments
    {"\n", "\n  ///"},

};

}  // namespace

std::string FormatMethodComments(
    google::protobuf::MethodDescriptor const& method,
    std::string const& variable_parameter_comments,
    bool is_discovery_document_proto) {
  google::protobuf::SourceLocation method_source_location;
  if (!method.GetSourceLocation(&method_source_location) ||
      method_source_location.leading_comments.empty()) {
    GCP_LOG(FATAL) << __FILE__ << ":" << __LINE__ << ": " << method.full_name()
                   << " no leading_comments to format";
  }
  std::string doxygen_formatted_function_comments =
      method_source_location.leading_comments;
  for (auto& sub : substitutions) {
    sub.uses += absl::StrReplaceAll({{sub.before, sub.after}},
                                    &doxygen_formatted_function_comments);
  }

  auto const options_comment = std::string{
      R"""(  /// @param opts Optional. Override the class-level options, such as retry and
  ///     backoff policies.
)"""};

  auto const return_comment_string = ReturnCommentString(method);

  auto references =
      Merge(ResolveCommentReferences(method_source_location.leading_comments,
                                     *method.file()->pool()),
            ResolveCommentReferences(variable_parameter_comments,
                                     *method.file()->pool()));
  references.emplace(method.input_type()->full_name(),
                     Location(*method.input_type()));
  auto method_return = ResolveMethodReturn(method);
  if (method_return) references.insert(*std::move(method_return));

  std::string lro_link;
  if (IsGRPCLongrunningOperation(method)) {
    lro_link = kTrailerGRPCLRO;
  } else if (IsHttpLongrunningOperation(method)) {
    lro_link = kTrailerComputeLRO;
  }

  auto trailer =
      absl::StrCat(kTrailerBeginning, std::move(lro_link), kTrailerEnding);

  for (auto const& kv : references) {
    absl::StrAppend(
        &trailer, "  /// [", kv.first,
        (is_discovery_document_proto ? "]: @cloud_cpp_reference_link{"
                                     : "]: @googleapis_reference_link{"),
        kv.second.filename, "#L", kv.second.lineno, "}\n");
  }

  std::string const deprecation_comment =
      method.options().has_deprecated() && method.options().deprecated()
          ? kDeprecationComment
          : "";

  return absl::StrCat(kMethodCommentsPrefix, deprecation_comment,
                      doxygen_formatted_function_comments, "\n",
                      variable_parameter_comments, options_comment,
                      return_comment_string, trailer, kMethodCommentsSuffix);
}

bool CheckMethodCommentSubstitutions() {
  bool all_substitutions_used = true;
  for (auto& sub : substitutions) {
    if (sub.uses == 0) {
      GCP_LOG(ERROR) << "Method comment substitution went unused ("
                     << sub.before << ")";
      all_substitutions_used = false;
    }
  }
  return all_substitutions_used;
}

std::string FormatStartMethodComments(bool is_method_deprecated) {
  auto constexpr kCommentBody = R"""( @copybrief $method_name$
  ///
  /// Specifying the [`NoAwaitTag`] immediately returns the
  /// [`$longrunning_operation_type$`] that corresponds to the Long Running
  /// Operation that has been started. No polling for operation status occurs.
  ///
  /// [`NoAwaitTag`]: @ref google::cloud::NoAwaitTag
)""";

  std::string const deprecation_comment =
      is_method_deprecated ? kDeprecationComment : "";

  return absl::StrCat(kMethodCommentsPrefix, deprecation_comment, kCommentBody,
                      kMethodCommentsSuffix);
}

std::string FormatAwaitMethodComments(bool is_method_deprecated) {
  auto constexpr kCommentBody = R"""( @copybrief $method_name$
  ///
  /// This method accepts a `$longrunning_operation_type$` that corresponds
  /// to a previously started Long Running Operation (LRO) and polls the status
  /// of the LRO in the background.
)""";

  std::string const deprecation_comment =
      is_method_deprecated ? kDeprecationComment : "";

  return absl::StrCat(kMethodCommentsPrefix, deprecation_comment, kCommentBody,
                      kMethodCommentsSuffix);
}

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
