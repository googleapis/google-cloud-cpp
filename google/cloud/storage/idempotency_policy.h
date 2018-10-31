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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_IDEMPOTENCY_POLICY_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_IDEMPOTENCY_POLICY_H_

#include "google/cloud/storage/internal/raw_client.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
/**
 * Define the interface for the idempotency policy.
 *
 * The idempotency policy controls which requests are treated as idempotent and
 * therefore safe to retry on a transient failure. Non-idempotent operations can
 * result in data loss. For example, consider `DeleteObject()`, if this
 * operation is called without pre-conditions retrying this operation may delete
 * more than one version of an object, which may not have the desired effect.
 * Even operations that "add" data can result in data loss, consider
 * `InsertObjectMedia()`, if called without pre-conditions retrying this
 * operation will insert multiple new versions, possibly deleting old data if
 * the bucket is configured to keep only N versions of each object.
 *
 * Some applications are designed to handle duplicate requests without data
 * loss, or the library may be used in an environment where the risk of data
 * loss due to duplicate requests is negligible or zero.
 *
 * This policy allows application developers to control the behavior of the
 * library with respect to retrying non-idempotent operations. Application
 * developers can configure the library to only retry operations that are known
 * to be idempotent (that is, they will succeed only once). Application may also
 * configure the library to retry all operations, regardless of whether the
 * operations are idempotent or not.
 */
class IdempotencyPolicy {
 public:
  virtual ~IdempotencyPolicy() = default;

  /// Create a new copy of this object.
  virtual std::unique_ptr<IdempotencyPolicy> clone() const = 0;

  //@{
  /// @name Bucket resource operations
  virtual bool IsIdempotent(
      internal::ListBucketsRequest const& request) const = 0;
  virtual bool IsIdempotent(
      internal::CreateBucketRequest const& request) const = 0;
  virtual bool IsIdempotent(
      internal::GetBucketMetadataRequest const& request) const = 0;
  virtual bool IsIdempotent(
      internal::DeleteBucketRequest const& request) const = 0;
  virtual bool IsIdempotent(
      internal::UpdateBucketRequest const& request) const = 0;
  virtual bool IsIdempotent(
      internal::PatchBucketRequest const& request) const = 0;
  virtual bool IsIdempotent(
      internal::GetBucketIamPolicyRequest const& request) const = 0;
  virtual bool IsIdempotent(
      internal::SetBucketIamPolicyRequest const& request) const = 0;
  virtual bool IsIdempotent(
      internal::TestBucketIamPermissionsRequest const& request) const = 0;
  //@}

  //@{
  /// @name Object resource operations
  virtual bool IsIdempotent(
      internal::InsertObjectMediaRequest const& request) const = 0;
  virtual bool IsIdempotent(
      internal::CopyObjectRequest const& request) const = 0;
  virtual bool IsIdempotent(
      internal::GetObjectMetadataRequest const& request) const = 0;
  virtual bool IsIdempotent(
      internal::ReadObjectRangeRequest const& request) const = 0;
  virtual bool IsIdempotent(
      internal::InsertObjectStreamingRequest const& request) const = 0;
  virtual bool IsIdempotent(
      internal::ListObjectsRequest const& request) const = 0;
  virtual bool IsIdempotent(
      internal::DeleteObjectRequest const& request) const = 0;
  virtual bool IsIdempotent(
      internal::UpdateObjectRequest const& request) const = 0;
  virtual bool IsIdempotent(
      internal::PatchObjectRequest const& request) const = 0;
  virtual bool IsIdempotent(
      internal::ComposeObjectRequest const& request) const = 0;
  virtual bool IsIdempotent(
      internal::RewriteObjectRequest const& request) const = 0;
  //@}

  //@{
  /// @name BucketAccessControls resource operations
  virtual bool IsIdempotent(
      internal::ListBucketAclRequest const& request) const = 0;
  virtual bool IsIdempotent(
      internal::CreateBucketAclRequest const& request) const = 0;
  virtual bool IsIdempotent(
      internal::DeleteBucketAclRequest const& request) const = 0;
  virtual bool IsIdempotent(
      internal::GetBucketAclRequest const& request) const = 0;
  virtual bool IsIdempotent(
      internal::UpdateBucketAclRequest const& request) const = 0;
  virtual bool IsIdempotent(
      internal::PatchBucketAclRequest const& request) const = 0;
  //@}

  //@{
  /// @name ObjectAccessControls operations
  virtual bool IsIdempotent(
      internal::ListObjectAclRequest const& request) const = 0;
  virtual bool IsIdempotent(
      internal::CreateObjectAclRequest const& request) const = 0;
  virtual bool IsIdempotent(
      internal::DeleteObjectAclRequest const& request) const = 0;
  virtual bool IsIdempotent(
      internal::GetObjectAclRequest const& request) const = 0;
  virtual bool IsIdempotent(
      internal::UpdateObjectAclRequest const& request) const = 0;
  virtual bool IsIdempotent(
      internal::PatchObjectAclRequest const& request) const = 0;
  //@}

  //@{
  /// @name DefaultObjectAccessControls operations.
  virtual bool IsIdempotent(
      internal::ListDefaultObjectAclRequest const& request) const = 0;
  virtual bool IsIdempotent(
      internal::CreateDefaultObjectAclRequest const& request) const = 0;
  virtual bool IsIdempotent(
      internal::DeleteDefaultObjectAclRequest const& request) const = 0;
  virtual bool IsIdempotent(
      internal::GetDefaultObjectAclRequest const& request) const = 0;
  virtual bool IsIdempotent(
      internal::UpdateDefaultObjectAclRequest const& request) const = 0;
  virtual bool IsIdempotent(
      internal::PatchDefaultObjectAclRequest const& request) const = 0;
  //@}

  //@{
  virtual bool IsIdempotent(
      internal::GetProjectServiceAccountRequest const& request) const = 0;
  //@}

  //@{
  virtual bool IsIdempotent(
      internal::ListNotificationsRequest const& request) const = 0;
  virtual bool IsIdempotent(
      internal::CreateNotificationRequest const& request) const = 0;
  virtual bool IsIdempotent(
      internal::GetNotificationRequest const& request) const = 0;
  virtual bool IsIdempotent(
      internal::DeleteNotificationRequest const& request) const = 0;
  //@}
};

/**
 * A IdempotencyPolicy that always retries all requests.
 */
class AlwaysRetryIdempotencyPolicy : public IdempotencyPolicy {
 public:
  AlwaysRetryIdempotencyPolicy() = default;

  std::unique_ptr<IdempotencyPolicy> clone() const override;

  //@{
  /// @name Bucket resource operations
  bool IsIdempotent(internal::ListBucketsRequest const& request) const override;
  bool IsIdempotent(
      internal::CreateBucketRequest const& request) const override;
  bool IsIdempotent(
      internal::GetBucketMetadataRequest const& request) const override;
  bool IsIdempotent(
      internal::DeleteBucketRequest const& request) const override;
  bool IsIdempotent(
      internal::UpdateBucketRequest const& request) const override;
  bool IsIdempotent(internal::PatchBucketRequest const& request) const override;
  bool IsIdempotent(
      internal::GetBucketIamPolicyRequest const& request) const override;
  bool IsIdempotent(
      internal::SetBucketIamPolicyRequest const& request) const override;
  bool IsIdempotent(
      internal::TestBucketIamPermissionsRequest const& request) const override;
  //@}

  //@{
  /// @name Object resource operations
  bool IsIdempotent(
      internal::InsertObjectMediaRequest const& request) const override;
  bool IsIdempotent(internal::CopyObjectRequest const& request) const override;
  bool IsIdempotent(
      internal::GetObjectMetadataRequest const& request) const override;
  bool IsIdempotent(
      internal::ReadObjectRangeRequest const& request) const override;
  bool IsIdempotent(
      internal::InsertObjectStreamingRequest const& request) const override;
  bool IsIdempotent(internal::ListObjectsRequest const& request) const override;
  bool IsIdempotent(
      internal::DeleteObjectRequest const& request) const override;
  bool IsIdempotent(
      internal::UpdateObjectRequest const& request) const override;
  bool IsIdempotent(internal::PatchObjectRequest const& request) const override;
  bool IsIdempotent(
      internal::ComposeObjectRequest const& request) const override;
  bool IsIdempotent(
      internal::RewriteObjectRequest const& request) const override;
  //@}

  //@{
  /// @name BucketAccessControls resource operations
  bool IsIdempotent(
      internal::ListBucketAclRequest const& request) const override;
  bool IsIdempotent(
      internal::CreateBucketAclRequest const& request) const override;
  bool IsIdempotent(
      internal::DeleteBucketAclRequest const& request) const override;
  bool IsIdempotent(
      internal::GetBucketAclRequest const& request) const override;
  bool IsIdempotent(
      internal::UpdateBucketAclRequest const& request) const override;
  bool IsIdempotent(
      internal::PatchBucketAclRequest const& request) const override;
  //@}

  //@{
  /// @name ObjectAccessControls operations
  bool IsIdempotent(
      internal::ListObjectAclRequest const& request) const override;
  bool IsIdempotent(
      internal::CreateObjectAclRequest const& request) const override;
  bool IsIdempotent(
      internal::DeleteObjectAclRequest const& request) const override;
  bool IsIdempotent(
      internal::GetObjectAclRequest const& request) const override;
  bool IsIdempotent(
      internal::UpdateObjectAclRequest const& request) const override;
  bool IsIdempotent(
      internal::PatchObjectAclRequest const& request) const override;
  //@}

  //@{
  /// @name DefaultObjectAccessControls operations.
  bool IsIdempotent(
      internal::ListDefaultObjectAclRequest const& request) const override;
  bool IsIdempotent(
      internal::CreateDefaultObjectAclRequest const& request) const override;
  bool IsIdempotent(
      internal::DeleteDefaultObjectAclRequest const& request) const override;
  bool IsIdempotent(
      internal::GetDefaultObjectAclRequest const& request) const override;
  bool IsIdempotent(
      internal::UpdateDefaultObjectAclRequest const& request) const override;
  bool IsIdempotent(
      internal::PatchDefaultObjectAclRequest const& request) const override;
  //@}

  //@{
  bool IsIdempotent(
      internal::GetProjectServiceAccountRequest const& request) const override;
  //@}

  //@{
  bool IsIdempotent(
      internal::ListNotificationsRequest const& request) const override;
  bool IsIdempotent(
      internal::CreateNotificationRequest const& request) const override;
  bool IsIdempotent(
      internal::GetNotificationRequest const& request) const override;
  bool IsIdempotent(
      internal::DeleteNotificationRequest const& request) const override;
  //@}
};

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_IDEMPOTENCY_POLICY_H_
