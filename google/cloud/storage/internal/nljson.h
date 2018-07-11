// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_NLJSON_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_NLJSON_H_

#include "google/cloud/storage/version.h"

/**
 * @file
 *
 * Include the nlohmann/json headers but renaming the namespace.
 *
 * We use the excellent nlohmann/json library to parse JSON in this client.
 * However, we do not want to create dependency conflicts where our version of
 * the code clashes with a different version that the user may have included.
 * We always include the library through this header, and rename its top-level
 * namespace to avoid conflicts. Because nlohmann/json is a header-only library
 * no further action is needed.
 *
 * @see https://github.com/nlohmann/json.git
 */

#define nlohmann google_cloud_storage_internal_nlohmann_3_1_2
#include <nlohmann/json.hpp>
#undef nlohmann

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace nl = ::google_cloud_storage_internal_nlohmann_3_1_2;
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_NLJSON_H_
