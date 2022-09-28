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

#include "google/cloud/storage/internal/patch_builder_details.h"
#include "google/cloud/storage/bucket_access_control.h"
#include "google/cloud/storage/bucket_metadata.h"
#include "google/cloud/storage/object_access_control.h"
#include "google/cloud/storage/object_metadata.h"

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

nlohmann::json const& PatchBuilderDetails::GetPatch(
    storage::BucketAccessControlPatchBuilder const& patch) {
  return GetPatch(patch.impl_);
}

nlohmann::json const& PatchBuilderDetails::GetPatch(
    storage::BucketMetadataPatchBuilder const& patch) {
  return GetPatch(patch.impl_);
}

nlohmann::json const& PatchBuilderDetails::GetLabelsSubPatch(
    storage::BucketMetadataPatchBuilder const& patch) {
  static auto const* const kEmpty = [] { return new nlohmann::json({}); }();
  if (!patch.labels_subpatch_dirty_) return *kEmpty;
  return GetPatch(patch.labels_subpatch_);
}

nlohmann::json const& PatchBuilderDetails::GetPatch(
    storage::ObjectAccessControlPatchBuilder const& patch) {
  return GetPatch(patch.impl_);
}

nlohmann::json const& PatchBuilderDetails::GetPatch(
    storage::ObjectMetadataPatchBuilder const& patch) {
  return GetPatch(patch.impl_);
}

nlohmann::json const& PatchBuilderDetails::GetMetadataSubPatch(
    storage::ObjectMetadataPatchBuilder const& patch) {
  static auto const* const kEmpty = [] { return new nlohmann::json({}); }();
  if (!patch.metadata_subpatch_dirty_) return *kEmpty;
  return GetPatch(patch.metadata_subpatch_);
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
