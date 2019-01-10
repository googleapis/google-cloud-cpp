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

#include "google/cloud/storage/idempotency_policy.h"
#include "google/cloud/internal/make_unique.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {

std::unique_ptr<IdempotencyPolicy> AlwaysRetryIdempotencyPolicy::clone() const {
  return google::cloud::internal::make_unique<AlwaysRetryIdempotencyPolicy>(
      *this);
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
    internal::SetBucketIamPolicyRequest const&) const {
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
    internal::InsertObjectStreamingRequest const&) const {
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
  return google::cloud::internal::make_unique<StrictIdempotencyPolicy>(*this);
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::ListBucketsRequest const&) const {
  // Read operations are always idempotent.
  return true;
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::CreateBucketRequest const&) const {
  // Creating a bucket is idempotent because you cannot create a new version
  // of a bucket, it succeeds only once.
  return true;
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::GetBucketMetadataRequest const&) const {
  // Read operations are always idempotent.
  return true;
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::DeleteBucketRequest const& request) const {
  return (request.HasOption<IfMatchEtag>() or
          request.HasOption<IfMetagenerationMatch>());
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::UpdateBucketRequest const& request) const {
  return (request.HasOption<IfMatchEtag>() or
          request.HasOption<IfMetagenerationMatch>());
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::PatchBucketRequest const& request) const {
  return (request.HasOption<IfMatchEtag>() or
          request.HasOption<IfMetagenerationMatch>());
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::GetBucketIamPolicyRequest const&) const {
  return true;
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::SetBucketIamPolicyRequest const& request) const {
  return request.HasOption<IfMatchEtag>();
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::TestBucketIamPermissionsRequest const&) const {
  return true;
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::LockBucketRetentionPolicyRequest const&) const {
  // This request type always requires a metageneration pre-condition.
  return true;
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::InsertObjectMediaRequest const& request) const {
  return request.HasOption<IfGenerationMatch>();
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::CopyObjectRequest const& request) const {
  // Only the pre-conditions on the destination matter. If they are not set, it
  // is possible for the request to succeed more than once, even if the source
  // pre-conditions are set. If they are set, the operation can only succeed
  // once, but the results may be different.
  return request.HasOption<IfGenerationMatch>();
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::GetObjectMetadataRequest const&) const {
  return true;
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::ReadObjectRangeRequest const&) const {
  return true;
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::InsertObjectStreamingRequest const& request) const {
  return request.HasOption<IfGenerationMatch>();
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::ListObjectsRequest const&) const {
  return true;
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::DeleteObjectRequest const& request) const {
  return request.HasOption<Generation>() or
         request.HasOption<IfGenerationMatch>();
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::UpdateObjectRequest const& request) const {
  return request.HasOption<IfMatchEtag>() or
         request.HasOption<IfMetagenerationMatch>();
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::PatchObjectRequest const& request) const {
  return request.HasOption<IfMatchEtag>() or
         request.HasOption<IfMetagenerationMatch>();
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::ComposeObjectRequest const& request) const {
  // Only the pre-conditions on the destination matter. If they are not set, it
  // is possible for the request to succeed more than once, even if the source
  // pre-conditions are set. If they are set, the operation can only succeed
  // once, but the results may be different.
  return request.HasOption<IfGenerationMatch>();
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::RewriteObjectRequest const& request) const {
  // Only the pre-conditions on the destination matter. If they are not set, it
  // is possible for the request to succeed more than once, even if the source
  // pre-conditions are set. If they are set, the operation can only succeed
  // once, but the results may be different.
  return request.HasOption<IfGenerationMatch>();
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::ResumableUploadRequest const& request) const {
  // Only the pre-conditions on the destination matter. If they are not set, it
  // is possible for the request to succeed more than once, even if the source
  // pre-conditions are set. If they are set, the operation can only succeed
  // once, but the results may be different.
  return request.HasOption<IfGenerationMatch>();
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::UploadChunkRequest const&) const {
  return true;
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::ListBucketAclRequest const&) const {
  return true;
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::CreateBucketAclRequest const& request) const {
  return request.HasOption<IfMatchEtag>();
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::DeleteBucketAclRequest const& request) const {
  return request.HasOption<IfMatchEtag>();
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::GetBucketAclRequest const&) const {
  return true;
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::UpdateBucketAclRequest const& request) const {
  return request.HasOption<IfMatchEtag>();
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::PatchBucketAclRequest const& request) const {
  return request.HasOption<IfMatchEtag>();
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::ListObjectAclRequest const&) const {
  return true;
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::CreateObjectAclRequest const& request) const {
  return request.HasOption<IfMatchEtag>();
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::DeleteObjectAclRequest const& request) const {
  return request.HasOption<IfMatchEtag>();
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::GetObjectAclRequest const&) const {
  return true;
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::UpdateObjectAclRequest const& request) const {
  return request.HasOption<IfMatchEtag>();
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::PatchObjectAclRequest const& request) const {
  return request.HasOption<IfMatchEtag>();
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::ListDefaultObjectAclRequest const&) const {
  return true;
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::CreateDefaultObjectAclRequest const& request) const {
  return request.HasOption<IfMatchEtag>();
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::DeleteDefaultObjectAclRequest const& request) const {
  return request.HasOption<IfMatchEtag>();
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::GetDefaultObjectAclRequest const&) const {
  return true;
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::UpdateDefaultObjectAclRequest const& request) const {
  return request.HasOption<IfMatchEtag>();
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::PatchDefaultObjectAclRequest const& request) const {
  return request.HasOption<IfMatchEtag>();
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::GetProjectServiceAccountRequest const&) const {
  return true;
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::ListNotificationsRequest const&) const {
  return true;
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::CreateNotificationRequest const&) const {
  return true;
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::GetNotificationRequest const&) const {
  return true;
}

bool StrictIdempotencyPolicy::IsIdempotent(
    internal::DeleteNotificationRequest const&) const {
  return true;
}

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
