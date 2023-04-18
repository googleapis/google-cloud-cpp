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

#ifndef GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_PAGINATION_H
#define GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_PAGINATION_H

#include "google/cloud/optional.h"
#include <google/protobuf/descriptor.h>
#include <string>
#include <utility>

namespace google {
namespace cloud {
namespace generator_internal {

/**
 * Determines if the given method meets the criteria for pagination.
 *
 * https://google.aip.dev/client-libraries/4233
 */
bool IsPaginated(google::protobuf::MethodDescriptor const& method);

/**
 * If method meets pagination criteria, provides paginated field type and field
 * name.
 *
 * https://google.aip.dev/client-libraries/4233
 */
google::cloud::optional<
    std::pair<std::string, google::protobuf::Descriptor const*>>
DeterminePagination(google::protobuf::MethodDescriptor const& method);

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_PAGINATION_H
