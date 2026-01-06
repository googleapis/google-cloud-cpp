// Copyright 2026 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ENDPOINT_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ENDPOINT_OPTIONS_H

#include "google/cloud/bigtable/options.h"
#include <chrono>
#include <string>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * The endpoint for data operations.
 *
 * @deprecated Please use `google::cloud::EndpointOption` instead.
 */
struct DataEndpointOption {
  using Type = std::string;
};

/**
 * The endpoint for table admin operations.
 *
 * @deprecated Please use `google::cloud::EndpointOption` instead.
 */
struct AdminEndpointOption {
  using Type = std::string;
};

/**
 * The endpoint for instance admin operations.
 *
 * In most scenarios this should have the same value as `AdminEndpointOption`.
 * The most common exception is testing, where the emulator for instance admin
 * operations may be different than the emulator for admin and data operations.
 *
 * @deprecated Please use `google::cloud::EndpointOption` instead.
 */
struct InstanceAdminEndpointOption {
  using Type = std::string;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ENDPOINT_OPTIONS_H
