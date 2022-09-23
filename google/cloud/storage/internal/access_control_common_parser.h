// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ACCESS_CONTROL_COMMON_PARSER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ACCESS_CONTROL_COMMON_PARSER_H

#include "google/cloud/storage/internal/access_control_common.h"
#include "google/cloud/status.h"
#include <nlohmann/json.hpp>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

// TODO(#9897) - remove this class and any references to it
struct GOOGLE_CLOUD_CPP_DEPRECATED(
    "This class will be removed shortly after 2023-06-01")
    AccessControlCommonParser {
  GOOGLE_CLOUD_CPP_DEPRECATED(
      "This function will be removed shortly after 2023-06-01")
  static Status FromJson(AccessControlCommon& result,
                         nlohmann::json const& json);
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ACCESS_CONTROL_COMMON_PARSER_H
