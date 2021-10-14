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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_RETRY_CLIENT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_RETRY_CLIENT_H

#include "google/cloud/storage/idempotency_policy.h"
#include "google/cloud/storage/internal/raw_client.h"
#include "google/cloud/storage/internal/resumable_upload_session.h"
#include "google/cloud/storage/retry_policy.h"
#include "google/cloud/storage/version.h"
#include <string>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
/**
 * Decorates a `RawClient` to retry each operation.
 */
class RetryClient : public RawClient,
                    public std::enable_shared_from_this<RetryClient> {
 public:
  explicit RetryClient(std::shared_ptr<RawClient> client,
                       Options const& options);

  ~RetryClient() override = default;

  ClientOptions const& client_options() const override;

  StatusOr<ListBucketsResponse> ListBuckets(
      ListBucketsRequest const& request) override;
  StatusOr<BucketMetadata> CreateBucket(
      CreateBucketRequest const& request) override;
  StatusOr<BucketMetadata> GetBucketMetadata(
      GetBucketMetadataRequest const& request) override;
  StatusOr<EmptyResponse> DeleteBucket(DeleteBucketRequest const&) override;
  StatusOr<BucketMetadata> UpdateBucket(
      UpdateBucketRequest const& request) override;
  StatusOr<BucketMetadata> PatchBucket(
      PatchBucketRequest const& request) override;
  StatusOr<IamPolicy> GetBucketIamPolicy(
      GetBucketIamPolicyRequest const& request) override;
  StatusOr<NativeIamPolicy> GetNativeBucketIamPolicy(
      GetBucketIamPolicyRequest const& request) override;
  StatusOr<IamPolicy> SetBucketIamPolicy(
      SetBucketIamPolicyRequest const& request) override;
  StatusOr<NativeIamPolicy> SetNativeBucketIamPolicy(
      SetNativeBucketIamPolicyRequest const& request) override;
  StatusOr<TestBucketIamPermissionsResponse> TestBucketIamPermissions(
      TestBucketIamPermissionsRequest const& request) override;
  StatusOr<BucketMetadata> LockBucketRetentionPolicy(
      LockBucketRetentionPolicyRequest const& request) override;

  StatusOr<ObjectMetadata> InsertObjectMedia(
      InsertObjectMediaRequest const& request) override;
  StatusOr<ObjectMetadata> CopyObject(
      CopyObjectRequest const& request) override;
  StatusOr<ObjectMetadata> GetObjectMetadata(
      GetObjectMetadataRequest const& request) override;

  /// Call ReadObject() but do not wrap the result in a RetryObjectReadSource.
  StatusOr<std::unique_ptr<ObjectReadSource>> ReadObjectNotWrapped(
      ReadObjectRangeRequest const&, RetryPolicy&, BackoffPolicy&);
  StatusOr<std::unique_ptr<ObjectReadSource>> ReadObject(
      ReadObjectRangeRequest const&) override;

  StatusOr<ListObjectsResponse> ListObjects(ListObjectsRequest const&) override;
  StatusOr<EmptyResponse> DeleteObject(DeleteObjectRequest const&) override;
  StatusOr<ObjectMetadata> UpdateObject(
      UpdateObjectRequest const& request) override;
  StatusOr<ObjectMetadata> PatchObject(
      PatchObjectRequest const& request) override;
  StatusOr<ObjectMetadata> ComposeObject(
      ComposeObjectRequest const& request) override;
  StatusOr<RewriteObjectResponse> RewriteObject(
      RewriteObjectRequest const&) override;
  StatusOr<std::unique_ptr<ResumableUploadSession>> CreateResumableSession(
      ResumableUploadRequest const& request) override;
  StatusOr<EmptyResponse> DeleteResumableUpload(
      DeleteResumableUploadRequest const& request) override;

  StatusOr<ListBucketAclResponse> ListBucketAcl(
      ListBucketAclRequest const& request) override;
  StatusOr<BucketAccessControl> CreateBucketAcl(
      CreateBucketAclRequest const&) override;
  StatusOr<EmptyResponse> DeleteBucketAcl(
      DeleteBucketAclRequest const&) override;
  StatusOr<BucketAccessControl> GetBucketAcl(
      GetBucketAclRequest const&) override;
  StatusOr<BucketAccessControl> UpdateBucketAcl(
      UpdateBucketAclRequest const&) override;
  StatusOr<BucketAccessControl> PatchBucketAcl(
      PatchBucketAclRequest const&) override;

  StatusOr<ListObjectAclResponse> ListObjectAcl(
      ListObjectAclRequest const& request) override;
  StatusOr<ObjectAccessControl> CreateObjectAcl(
      CreateObjectAclRequest const&) override;
  StatusOr<EmptyResponse> DeleteObjectAcl(
      DeleteObjectAclRequest const&) override;
  StatusOr<ObjectAccessControl> GetObjectAcl(
      GetObjectAclRequest const&) override;
  StatusOr<ObjectAccessControl> UpdateObjectAcl(
      UpdateObjectAclRequest const&) override;
  StatusOr<ObjectAccessControl> PatchObjectAcl(
      PatchObjectAclRequest const&) override;

  StatusOr<ListDefaultObjectAclResponse> ListDefaultObjectAcl(
      ListDefaultObjectAclRequest const& request) override;
  StatusOr<ObjectAccessControl> CreateDefaultObjectAcl(
      CreateDefaultObjectAclRequest const&) override;
  StatusOr<EmptyResponse> DeleteDefaultObjectAcl(
      DeleteDefaultObjectAclRequest const&) override;
  StatusOr<ObjectAccessControl> GetDefaultObjectAcl(
      GetDefaultObjectAclRequest const&) override;
  StatusOr<ObjectAccessControl> UpdateDefaultObjectAcl(
      UpdateDefaultObjectAclRequest const&) override;
  StatusOr<ObjectAccessControl> PatchDefaultObjectAcl(
      PatchDefaultObjectAclRequest const&) override;

  StatusOr<ServiceAccount> GetServiceAccount(
      GetProjectServiceAccountRequest const&) override;
  StatusOr<ListHmacKeysResponse> ListHmacKeys(
      ListHmacKeysRequest const&) override;
  StatusOr<CreateHmacKeyResponse> CreateHmacKey(
      CreateHmacKeyRequest const&) override;
  StatusOr<EmptyResponse> DeleteHmacKey(DeleteHmacKeyRequest const&) override;
  StatusOr<HmacKeyMetadata> GetHmacKey(GetHmacKeyRequest const&) override;
  StatusOr<HmacKeyMetadata> UpdateHmacKey(UpdateHmacKeyRequest const&) override;
  StatusOr<SignBlobResponse> SignBlob(SignBlobRequest const&) override;

  StatusOr<ListNotificationsResponse> ListNotifications(
      ListNotificationsRequest const&) override;
  StatusOr<NotificationMetadata> CreateNotification(
      CreateNotificationRequest const&) override;
  StatusOr<NotificationMetadata> GetNotification(
      GetNotificationRequest const&) override;
  StatusOr<EmptyResponse> DeleteNotification(
      DeleteNotificationRequest const&) override;

  std::shared_ptr<RawClient> client() const { return client_; }

 private:
  std::shared_ptr<RawClient> client_;
  std::shared_ptr<RetryPolicy const> retry_policy_prototype_;
  std::shared_ptr<BackoffPolicy const> backoff_policy_prototype_;
  std::shared_ptr<IdempotencyPolicy const> idempotency_policy_;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_RETRY_CLIENT_H
