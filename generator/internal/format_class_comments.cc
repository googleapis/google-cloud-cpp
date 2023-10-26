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

#include "generator/internal/format_class_comments.h"
#include "generator/internal/resolve_comment_references.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/absl_str_replace_quiet.h"
#include "google/cloud/log.h"
#include "absl/strings/strip.h"

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

auto constexpr kFixedClientComment = R"""(
///
/// @par Equality
///
/// Instances of this class created via copy-construction or copy-assignment
/// always compare equal. Instances created with equal
/// `std::shared_ptr<*Connection>` objects compare equal. Objects that compare
/// equal share the same underlying resources.
///
/// @par Performance
///
/// Creating a new instance of this class is a relatively expensive operation,
/// new objects establish new connections to the service. In contrast,
/// copy-construction, move-construction, and the corresponding assignment
/// operations are relatively efficient as the copies share all underlying
/// resources.
///
/// @par Thread Safety
///
/// Concurrent access to different instances of this class, even if they compare
/// equal, is guaranteed to work. Two or more threads operating on the same
/// instance of this class is not guaranteed to work. Since copy-construction
/// and move-construction is a relatively efficient operation, consider using
/// such a copy when using this class from multiple threads.
///)""";

}  // namespace

std::string FormatClassCommentsFromServiceComments(
    google::protobuf::ServiceDescriptor const& service,
    std::string const& service_name,
    absl::optional<std::string> const& replacement_comment) {
  google::protobuf::SourceLocation service_source_location;
  std::string formatted_comments;
  // Use the service descriptor to populate the service_source_location.
  if (!service.GetSourceLocation(&service_source_location) ||
      service_source_location.leading_comments.empty()) {
    GCP_LOG(INFO) << __FILE__ << ":" << __LINE__ << ": " << service.full_name()
                  << " no leading_comments to format";
    formatted_comments = absl::StrCat(" ", service_name, "Client");
  } else {
    service_source_location.leading_comments = replacement_comment.value_or(
        std::move(service_source_location.leading_comments));
    formatted_comments = absl::StrReplaceAll(
        absl::StripSuffix(service_source_location.leading_comments, "\n"),
        {{"\n\n", "\n///\n/// "},
         {"\n", "\n/// "},
         // This uses a relative link, but we do not know how to resolve
         // those.  Convert them back to an absolute link.
         {"[groups](#google.monitoring.v3.Group)",
          "[groups][google.monitoring.v3.Group]"},
         {service.name(), service_name}});
  }

  auto const references =
      ResolveCommentReferences(formatted_comments, *service.file()->pool());
  auto trailer = std::string{};
  for (auto const& kv : references) {
    absl::StrAppend(&trailer, "\n/// [", kv.first,
                    "]: @googleapis_reference_link{", kv.second.filename, "#L",
                    kv.second.lineno, "}");
  }
  if (!trailer.empty()) trailer += "\n///";

  auto doxygen_formatted_comments = absl::StrCat("///\n///", formatted_comments,
                                                 kFixedClientComment, trailer);
  return absl::StrReplaceAll(doxygen_formatted_comments, {{"///  ", "/// "}});
}

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
