// Copyright 2022 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_PATCH_BUILDER_DETAILS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_PATCH_BUILDER_DETAILS_H

#include "google/cloud/storage/version.h"
#include <nlohmann/json.hpp>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
class BucketAccessControlPatchBuilder;
class BucketMetadataPatchBuilder;
class ObjectAccessControlPatchBuilder;
class ObjectMetadataPatchBuilder;

namespace internal {

class PatchBuilder;

/**
 * Get a JSON patch from a *PatchBuilder.
 *
 * The PatchBuilder class cannot expose any API returning `nlohmann::json`
 * because we do not want to expose the `nlohmann::json` in any of our public
 * headers. However, in the implementation of GCS+gRPC we do need access to the
 * JSON patch object.  We use a couple of forward declarations and some
 * `friend`s to achieve this.
 */
struct PatchBuilderDetails {
  static nlohmann::json const& GetPatch(
      storage::BucketAccessControlPatchBuilder const& patch);
  static nlohmann::json const& GetPatch(
      storage::BucketMetadataPatchBuilder const& patch);
  static nlohmann::json const& GetLabelsSubPatch(
      storage::BucketMetadataPatchBuilder const& patch);
  static nlohmann::json const& GetPatch(
      storage::ObjectAccessControlPatchBuilder const& patch);
  static nlohmann::json const& GetPatch(
      storage::ObjectMetadataPatchBuilder const& patch);
  static nlohmann::json const& GetMetadataSubPatch(
      storage::ObjectMetadataPatchBuilder const& patch);
  static nlohmann::json const& GetPatch(PatchBuilder const& patch);
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_PATCH_BUILDER_DETAILS_H
