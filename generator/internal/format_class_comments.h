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

#ifndef GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_FORMAT_CLASS_COMMENTS_H
#define GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_FORMAT_CLASS_COMMENTS_H

#include "google/protobuf/descriptor.h"
#include <string>

namespace google {
namespace cloud {
namespace generator_internal {

/**
 * The function formats class comments based on the pre-existing service
 * comments.
 *
 * The function does not just use service.name() and takes @p service_name in
 * case there exists a service_name_mapping argument that is renaming the
 * service.
 *
 * Additionally, it will replace the comment with the value from
 * service_name_to_comment if it exists. This can map to the original service
 * name or a mapped name.
 */
std::string FormatClassCommentsFromServiceComments(
    google::protobuf::ServiceDescriptor const& service,
    std::string const& service_name,
    absl::optional<std::string> const& replacement_comment);

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_FORMAT_CLASS_COMMENTS_H
