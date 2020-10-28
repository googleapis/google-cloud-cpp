// Copyright 2020 Google LLC
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

#include "google/cloud/storage/internal/object_access_control_parser.h"
#include "google/cloud/storage/internal/access_control_common_parser.h"
#include "google/cloud/storage/internal/common_metadata_parser.h"
#include <nlohmann/json.hpp>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
StatusOr<ObjectAccessControl> ObjectAccessControlParser::FromJson(
    nlohmann::json const& json) {
  if (!json.is_object()) {
    return Status(StatusCode::kInvalidArgument, __func__);
  }
  ObjectAccessControl result{};
  auto status = internal::AccessControlCommonParser::FromJson(result, json);
  if (!status.ok()) {
    return status;
  }
  result.generation_ = internal::ParseLongField(json, "generation");
  result.object_ = json.value("object", "");
  return result;
}

StatusOr<ObjectAccessControl> ObjectAccessControlParser::FromString(
    std::string const& payload) {
  auto json = nlohmann::json::parse(payload, nullptr, false);
  return FromJson(json);
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
