// Copyright 2018 Google LLC
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

#include "google/cloud/storage/idempotency_policy.h"

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
std::unique_ptr<IdempotencyPolicy> AlwaysRetryIdempotencyPolicy::clone() const {
  return std::make_unique<AlwaysRetryIdempotencyPolicy>(*this);
}

bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::ListBucketsRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::CreateBucketRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::GetBucketMetadataRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::DeleteBucketRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::UpdateBucketRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::PatchBucketRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::GetBucketIamPolicyRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::SetNativeBucketIamPolicyRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::TestBucketIamPermissionsRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::LockBucketRetentionPolicyRequest const&) const {
  return true;
}

bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::InsertObjectMediaRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::CopyObjectRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::GetObjectMetadataRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::ReadObjectRangeRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::ListObjectsRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::DeleteObjectRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::UpdateObjectRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::PatchObjectRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::ComposeObjectRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::RewriteObjectRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::ResumableUploadRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::UploadChunkRequest const&) const {
  return true;
}

bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::ListBucketAclRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::CreateBucketAclRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::DeleteBucketAclRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::GetBucketAclRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::UpdateBucketAclRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::PatchBucketAclRequest const&) const {
  return true;
}

bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::ListObjectAclRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::CreateObjectAclRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::DeleteObjectAclRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::GetObjectAclRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::UpdateObjectAclRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::PatchObjectAclRequest const&) const {
  return true;
}

bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::ListDefaultObjectAclRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::CreateDefaultObjectAclRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::DeleteDefaultObjectAclRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::GetDefaultObjectAclRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::UpdateDefaultObjectAclRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::PatchDefaultObjectAclRequest const&) const {
  return true;
}

bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::GetProjectServiceAccountRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::ListHmacKeysRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::CreateHmacKeyRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::DeleteHmacKeyRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::GetHmacKeyRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::UpdateHmacKeyRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::SignBlobRequest const&) const {
  return true;
}

bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::ListNotificationsRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::CreateNotificationRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::GetNotificationRequest const&) const {
  return true;
}
bool AlwaysRetryIdempotencyPolicy::IsIdempotent(
    internal::DeleteNotificationRequest const&) const {
  return true;
}

std::unique_ptr<IdempotencyPolicy> StrictIdempotencyPolicy::clone() const {
  return std::make_unique<StrictIdempotencyPolicy>(*this);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
