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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_RAW_CLIENT_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_RAW_CLIENT_H_

#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include "google/cloud/storage/bucket_metadata.h"
#include "google/cloud/storage/client_options.h"
#include "google/cloud/storage/internal/bucket_acl_requests.h"
#include "google/cloud/storage/internal/bucket_requests.h"
#include "google/cloud/storage/internal/default_object_acl_requests.h"
#include "google/cloud/storage/internal/empty_response.h"
#include "google/cloud/storage/internal/hmac_key_requests.h"
#include "google/cloud/storage/internal/notification_requests.h"
#include "google/cloud/storage/internal/object_acl_requests.h"
#include "google/cloud/storage/internal/object_requests.h"
#include "google/cloud/storage/internal/object_streambuf.h"
#include "google/cloud/storage/internal/resumable_upload_session.h"
#include "google/cloud/storage/internal/service_account_requests.h"
#include "google/cloud/storage/internal/sign_blob_requests.h"
#include "google/cloud/storage/oauth2/credentials.h"
#include "google/cloud/storage/object_metadata.h"
#include "google/cloud/storage/service_account.h"
#include "google/cloud/storage/version.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/**
 * Defines the interface used to communicate with Google Cloud Storage.
 */
class RawClient {
 public:
  virtual ~RawClient() = default;

  virtual ClientOptions const& client_options() const = 0;

  //@{
  /// @name Bucket resource operations
  virtual StatusOr<ListBucketsResponse> ListBuckets(
      ListBucketsRequest const& request) = 0;
  virtual StatusOr<BucketMetadata> CreateBucket(CreateBucketRequest const&) = 0;
  virtual StatusOr<BucketMetadata> GetBucketMetadata(
      GetBucketMetadataRequest const& request) = 0;
  virtual StatusOr<EmptyResponse> DeleteBucket(
      DeleteBucketRequest const& request) = 0;
  virtual StatusOr<BucketMetadata> UpdateBucket(UpdateBucketRequest const&) = 0;
  virtual StatusOr<BucketMetadata> PatchBucket(
      PatchBucketRequest const& request) = 0;
  virtual StatusOr<IamPolicy> GetBucketIamPolicy(
      GetBucketIamPolicyRequest const& request) = 0;
  virtual StatusOr<IamPolicy> SetBucketIamPolicy(
      SetBucketIamPolicyRequest const& request) = 0;
  virtual StatusOr<TestBucketIamPermissionsResponse> TestBucketIamPermissions(
      TestBucketIamPermissionsRequest const& request) = 0;
  virtual StatusOr<BucketMetadata> LockBucketRetentionPolicy(
      LockBucketRetentionPolicyRequest const& request) = 0;
  //@}

  //@{
  /// @name Object resource operations
  virtual StatusOr<ObjectMetadata> InsertObjectMedia(
      InsertObjectMediaRequest const&) = 0;
  virtual StatusOr<ObjectMetadata> CopyObject(CopyObjectRequest const&) = 0;
  virtual StatusOr<ObjectMetadata> GetObjectMetadata(
      GetObjectMetadataRequest const& request) = 0;
  virtual StatusOr<std::unique_ptr<ObjectReadStreambuf>> ReadObject(
      ReadObjectRangeRequest const&) = 0;
  virtual StatusOr<std::unique_ptr<ObjectWriteStreambuf>> WriteObject(
      InsertObjectStreamingRequest const&) = 0;
  virtual StatusOr<ListObjectsResponse> ListObjects(
      ListObjectsRequest const&) = 0;
  virtual StatusOr<EmptyResponse> DeleteObject(DeleteObjectRequest const&) = 0;
  virtual StatusOr<ObjectMetadata> UpdateObject(UpdateObjectRequest const&) = 0;
  virtual StatusOr<ObjectMetadata> PatchObject(PatchObjectRequest const&) = 0;
  virtual StatusOr<ObjectMetadata> ComposeObject(
      ComposeObjectRequest const&) = 0;
  virtual StatusOr<RewriteObjectResponse> RewriteObject(
      RewriteObjectRequest const&) = 0;
  virtual StatusOr<std::unique_ptr<ResumableUploadSession>>
  CreateResumableSession(ResumableUploadRequest const& request) = 0;
  virtual StatusOr<std::unique_ptr<ResumableUploadSession>>
  RestoreResumableSession(std::string const& session_id) = 0;
  //@}

  //@{
  /// @name BucketAccessControls resource operations
  virtual StatusOr<ListBucketAclResponse> ListBucketAcl(
      ListBucketAclRequest const&) = 0;
  virtual StatusOr<BucketAccessControl> CreateBucketAcl(
      CreateBucketAclRequest const&) = 0;
  virtual StatusOr<EmptyResponse> DeleteBucketAcl(
      DeleteBucketAclRequest const&) = 0;
  virtual StatusOr<BucketAccessControl> GetBucketAcl(
      GetBucketAclRequest const&) = 0;
  virtual StatusOr<BucketAccessControl> UpdateBucketAcl(
      UpdateBucketAclRequest const&) = 0;
  virtual StatusOr<BucketAccessControl> PatchBucketAcl(
      PatchBucketAclRequest const&) = 0;
  //@}

  //@{
  /// @name ObjectAccessControls operations
  virtual StatusOr<ListObjectAclResponse> ListObjectAcl(
      ListObjectAclRequest const&) = 0;
  virtual StatusOr<ObjectAccessControl> CreateObjectAcl(
      CreateObjectAclRequest const&) = 0;
  virtual StatusOr<EmptyResponse> DeleteObjectAcl(
      DeleteObjectAclRequest const&) = 0;
  virtual StatusOr<ObjectAccessControl> GetObjectAcl(
      GetObjectAclRequest const&) = 0;
  virtual StatusOr<ObjectAccessControl> UpdateObjectAcl(
      UpdateObjectAclRequest const&) = 0;
  virtual StatusOr<ObjectAccessControl> PatchObjectAcl(
      PatchObjectAclRequest const&) = 0;
  //@}

  //@{
  /// @name DefaultObjectAccessControls operations.
  virtual StatusOr<ListDefaultObjectAclResponse> ListDefaultObjectAcl(
      ListDefaultObjectAclRequest const&) = 0;
  virtual StatusOr<ObjectAccessControl> CreateDefaultObjectAcl(
      CreateDefaultObjectAclRequest const&) = 0;
  virtual StatusOr<EmptyResponse> DeleteDefaultObjectAcl(
      DeleteDefaultObjectAclRequest const&) = 0;
  virtual StatusOr<ObjectAccessControl> GetDefaultObjectAcl(
      GetDefaultObjectAclRequest const&) = 0;
  virtual StatusOr<ObjectAccessControl> UpdateDefaultObjectAcl(
      UpdateDefaultObjectAclRequest const&) = 0;
  virtual StatusOr<ObjectAccessControl> PatchDefaultObjectAcl(
      PatchDefaultObjectAclRequest const&) = 0;
  //@}

  //@{
  virtual StatusOr<ServiceAccount> GetServiceAccount(
      GetProjectServiceAccountRequest const&) = 0;
  virtual StatusOr<ListHmacKeysResponse> ListHmacKeys(
      ListHmacKeysRequest const&) = 0;
  virtual StatusOr<CreateHmacKeyResponse> CreateHmacKey(
      CreateHmacKeyRequest const&) = 0;
  virtual StatusOr<EmptyResponse> DeleteHmacKey(
      DeleteHmacKeyRequest const&) = 0;
  virtual StatusOr<HmacKeyMetadata> GetHmacKey(GetHmacKeyRequest const&) = 0;
  virtual StatusOr<HmacKeyMetadata> UpdateHmacKey(
      UpdateHmacKeyRequest const&) = 0;
  virtual StatusOr<SignBlobResponse> SignBlob(SignBlobRequest const&) = 0;
  //@}

  //@{
  virtual StatusOr<ListNotificationsResponse> ListNotifications(
      ListNotificationsRequest const&) = 0;
  virtual StatusOr<NotificationMetadata> CreateNotification(
      CreateNotificationRequest const&) = 0;
  virtual StatusOr<NotificationMetadata> GetNotification(
      GetNotificationRequest const&) = 0;
  virtual StatusOr<EmptyResponse> DeleteNotification(
      DeleteNotificationRequest const&) = 0;
  //@}
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_RAW_CLIENT_H_
