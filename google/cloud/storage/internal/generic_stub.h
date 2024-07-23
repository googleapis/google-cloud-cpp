// Copyright 2023 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GENERIC_STUB_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GENERIC_STUB_H

#include "google/cloud/storage/bucket_metadata.h"
#include "google/cloud/storage/client_options.h"
#include "google/cloud/storage/internal/bucket_acl_requests.h"
#include "google/cloud/storage/internal/bucket_requests.h"
#include "google/cloud/storage/internal/default_object_acl_requests.h"
#include "google/cloud/storage/internal/empty_response.h"
#include "google/cloud/storage/internal/hash_validator.h"
#include "google/cloud/storage/internal/hmac_key_requests.h"
#include "google/cloud/storage/internal/notification_requests.h"
#include "google/cloud/storage/internal/object_acl_requests.h"
#include "google/cloud/storage/internal/object_read_source.h"
#include "google/cloud/storage/internal/object_requests.h"
#include "google/cloud/storage/internal/service_account_requests.h"
#include "google/cloud/storage/internal/sign_blob_requests.h"
#include "google/cloud/storage/oauth2/credentials.h"
#include "google/cloud/storage/object_metadata.h"
#include "google/cloud/storage/service_account.h"
#include "google/cloud/internal/rest_context.h"
#include "google/cloud/options.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <memory>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * The interface implemented by the gRPC and HTTP stubs.
 *
 * The GCS client library supports both HTTP and gRPC transports. The HTTP
 * transport precedes the gRPC transport, and we do not want to introduce a
 * dependency on gRPC for existing customers. This means we need an interface
 * implemented by both gRPC and HTTP.
 *
 * Originally `StorageConnection` filled this role. We needed to introduce
 * per-call headers, so it became necessary to create a new class.
 */
class GenericStub {
 public:
  virtual ~GenericStub() = default;

  virtual google::cloud::Options options() const = 0;

  ///@{
  /// @name Bucket resource operations,
  virtual StatusOr<storage::internal::ListBucketsResponse> ListBuckets(
      rest_internal::RestContext&, Options const&,
      storage::internal::ListBucketsRequest const&) = 0;
  virtual StatusOr<storage::BucketMetadata> CreateBucket(
      rest_internal::RestContext&, Options const&,
      storage::internal::CreateBucketRequest const&) = 0;
  virtual StatusOr<storage::BucketMetadata> GetBucketMetadata(
      rest_internal::RestContext&, Options const&,
      storage::internal::GetBucketMetadataRequest const&) = 0;
  virtual StatusOr<storage::internal::EmptyResponse> DeleteBucket(
      rest_internal::RestContext&, Options const&,
      storage::internal::DeleteBucketRequest const&) = 0;
  virtual StatusOr<storage::BucketMetadata> UpdateBucket(
      rest_internal::RestContext&, Options const&,
      storage::internal::UpdateBucketRequest const&) = 0;
  virtual StatusOr<storage::BucketMetadata> PatchBucket(
      rest_internal::RestContext&, Options const&,
      storage::internal::PatchBucketRequest const&) = 0;
  virtual StatusOr<storage::NativeIamPolicy> GetNativeBucketIamPolicy(
      rest_internal::RestContext&, Options const&,
      storage::internal::GetBucketIamPolicyRequest const&) = 0;
  virtual StatusOr<storage::NativeIamPolicy> SetNativeBucketIamPolicy(
      rest_internal::RestContext&, Options const&,
      storage::internal::SetNativeBucketIamPolicyRequest const&) = 0;
  virtual StatusOr<storage::internal::TestBucketIamPermissionsResponse>
  TestBucketIamPermissions(
      rest_internal::RestContext&, Options const&,
      storage::internal::TestBucketIamPermissionsRequest const&) = 0;
  virtual StatusOr<storage::BucketMetadata> LockBucketRetentionPolicy(
      rest_internal::RestContext&, Options const&,
      storage::internal::LockBucketRetentionPolicyRequest const&) = 0;
  ///@}

  ///@{
  /// @name Object resource operations.
  virtual StatusOr<storage::ObjectMetadata> InsertObjectMedia(
      rest_internal::RestContext&, Options const&,
      storage::internal::InsertObjectMediaRequest const&) = 0;
  virtual StatusOr<storage::ObjectMetadata> CopyObject(
      rest_internal::RestContext&, Options const&,
      storage::internal::CopyObjectRequest const&) = 0;
  virtual StatusOr<storage::ObjectMetadata> GetObjectMetadata(
      rest_internal::RestContext&, Options const&,
      storage::internal::GetObjectMetadataRequest const&) = 0;
  virtual StatusOr<std::unique_ptr<storage::internal::ObjectReadSource>>
  ReadObject(rest_internal::RestContext&, Options const&,
             storage::internal::ReadObjectRangeRequest const&) = 0;
  virtual StatusOr<storage::internal::ListObjectsResponse> ListObjects(
      rest_internal::RestContext&, Options const&,
      storage::internal::ListObjectsRequest const&) = 0;
  virtual StatusOr<storage::internal::EmptyResponse> DeleteObject(
      rest_internal::RestContext&, Options const&,
      storage::internal::DeleteObjectRequest const&) = 0;
  virtual StatusOr<storage::ObjectMetadata> UpdateObject(
      rest_internal::RestContext&, Options const&,
      storage::internal::UpdateObjectRequest const&) = 0;
  virtual StatusOr<storage::ObjectMetadata> PatchObject(
      rest_internal::RestContext&, Options const&,
      storage::internal::PatchObjectRequest const&) = 0;
  virtual StatusOr<storage::ObjectMetadata> ComposeObject(
      rest_internal::RestContext&, Options const&,
      storage::internal::ComposeObjectRequest const&) = 0;
  virtual StatusOr<storage::internal::RewriteObjectResponse> RewriteObject(
      rest_internal::RestContext&, Options const&,
      storage::internal::RewriteObjectRequest const&) = 0;

  virtual StatusOr<storage::internal::CreateResumableUploadResponse>
  CreateResumableUpload(rest_internal::RestContext&, Options const&,
                        storage::internal::ResumableUploadRequest const&) = 0;
  virtual StatusOr<storage::internal::QueryResumableUploadResponse>
  QueryResumableUpload(
      rest_internal::RestContext&, Options const&,
      storage::internal::QueryResumableUploadRequest const&) = 0;
  virtual StatusOr<storage::internal::EmptyResponse> DeleteResumableUpload(
      rest_internal::RestContext&, Options const&,
      storage::internal::DeleteResumableUploadRequest const&) = 0;
  virtual StatusOr<storage::internal::QueryResumableUploadResponse> UploadChunk(
      rest_internal::RestContext&, Options const&,
      storage::internal::UploadChunkRequest const&) = 0;
  ///@}

  ///@{
  /// @name BucketAccessControls resource operations.
  virtual StatusOr<storage::internal::ListBucketAclResponse> ListBucketAcl(
      rest_internal::RestContext&, Options const&,
      storage::internal::ListBucketAclRequest const&) = 0;
  virtual StatusOr<storage::BucketAccessControl> CreateBucketAcl(
      rest_internal::RestContext&, Options const&,
      storage::internal::CreateBucketAclRequest const&) = 0;
  virtual StatusOr<storage::internal::EmptyResponse> DeleteBucketAcl(
      rest_internal::RestContext&, Options const&,
      storage::internal::DeleteBucketAclRequest const&) = 0;
  virtual StatusOr<storage::BucketAccessControl> GetBucketAcl(
      rest_internal::RestContext&, Options const&,
      storage::internal::GetBucketAclRequest const&) = 0;
  virtual StatusOr<storage::BucketAccessControl> UpdateBucketAcl(
      rest_internal::RestContext&, Options const&,
      storage::internal::UpdateBucketAclRequest const&) = 0;
  virtual StatusOr<storage::BucketAccessControl> PatchBucketAcl(
      rest_internal::RestContext&, Options const&,
      storage::internal::PatchBucketAclRequest const&) = 0;
  ///@}

  ///@{
  /// @name ObjectAccessControls operations.
  virtual StatusOr<storage::internal::ListObjectAclResponse> ListObjectAcl(
      rest_internal::RestContext&, Options const&,
      storage::internal::ListObjectAclRequest const&) = 0;
  virtual StatusOr<storage::ObjectAccessControl> CreateObjectAcl(
      rest_internal::RestContext&, Options const&,
      storage::internal::CreateObjectAclRequest const&) = 0;
  virtual StatusOr<storage::internal::EmptyResponse> DeleteObjectAcl(
      rest_internal::RestContext&, Options const&,
      storage::internal::DeleteObjectAclRequest const&) = 0;
  virtual StatusOr<storage::ObjectAccessControl> GetObjectAcl(
      rest_internal::RestContext&, Options const&,
      storage::internal::GetObjectAclRequest const&) = 0;
  virtual StatusOr<storage::ObjectAccessControl> UpdateObjectAcl(
      rest_internal::RestContext&, Options const&,
      storage::internal::UpdateObjectAclRequest const&) = 0;
  virtual StatusOr<storage::ObjectAccessControl> PatchObjectAcl(
      rest_internal::RestContext&, Options const&,
      storage::internal::PatchObjectAclRequest const&) = 0;
  ///@}

  ///@{
  /// @name DefaultObjectAccessControls operations.
  virtual StatusOr<storage::internal::ListDefaultObjectAclResponse>
  ListDefaultObjectAcl(
      rest_internal::RestContext&, Options const&,
      storage::internal::ListDefaultObjectAclRequest const&) = 0;
  virtual StatusOr<storage::ObjectAccessControl> CreateDefaultObjectAcl(
      rest_internal::RestContext&, Options const&,
      storage::internal::CreateDefaultObjectAclRequest const&) = 0;
  virtual StatusOr<storage::internal::EmptyResponse> DeleteDefaultObjectAcl(
      rest_internal::RestContext&, Options const&,
      storage::internal::DeleteDefaultObjectAclRequest const&) = 0;
  virtual StatusOr<storage::ObjectAccessControl> GetDefaultObjectAcl(
      rest_internal::RestContext&, Options const&,
      storage::internal::GetDefaultObjectAclRequest const&) = 0;
  virtual StatusOr<storage::ObjectAccessControl> UpdateDefaultObjectAcl(
      rest_internal::RestContext&, Options const&,
      storage::internal::UpdateDefaultObjectAclRequest const&) = 0;
  virtual StatusOr<storage::ObjectAccessControl> PatchDefaultObjectAcl(
      rest_internal::RestContext&, Options const&,
      storage::internal::PatchDefaultObjectAclRequest const&) = 0;
  ///@}

  ///@{
  /// @name ServiceAccount (include HMAC Key) operations.
  virtual StatusOr<storage::ServiceAccount> GetServiceAccount(
      rest_internal::RestContext&, Options const&,
      storage::internal::GetProjectServiceAccountRequest const&) = 0;
  virtual StatusOr<storage::internal::ListHmacKeysResponse> ListHmacKeys(
      rest_internal::RestContext&, Options const&,
      storage::internal::ListHmacKeysRequest const&) = 0;
  virtual StatusOr<storage::internal::CreateHmacKeyResponse> CreateHmacKey(
      rest_internal::RestContext&, Options const&,
      storage::internal::CreateHmacKeyRequest const&) = 0;
  virtual StatusOr<storage::internal::EmptyResponse> DeleteHmacKey(
      rest_internal::RestContext&, Options const&,
      storage::internal::DeleteHmacKeyRequest const&) = 0;
  virtual StatusOr<storage::HmacKeyMetadata> GetHmacKey(
      rest_internal::RestContext&, Options const&,
      storage::internal::GetHmacKeyRequest const&) = 0;
  virtual StatusOr<storage::HmacKeyMetadata> UpdateHmacKey(
      rest_internal::RestContext&, Options const&,
      storage::internal::UpdateHmacKeyRequest const&) = 0;
  virtual StatusOr<storage::internal::SignBlobResponse> SignBlob(
      rest_internal::RestContext&, Options const&,
      storage::internal::SignBlobRequest const&) = 0;
  ///@}

  ///@{
  /// @name Notification resource operations.
  virtual StatusOr<storage::internal::ListNotificationsResponse>
  ListNotifications(rest_internal::RestContext&, Options const&,
                    storage::internal::ListNotificationsRequest const&) = 0;
  virtual StatusOr<storage::NotificationMetadata> CreateNotification(
      rest_internal::RestContext&, Options const&,
      storage::internal::CreateNotificationRequest const&) = 0;
  virtual StatusOr<storage::NotificationMetadata> GetNotification(
      rest_internal::RestContext&, Options const&,
      storage::internal::GetNotificationRequest const&) = 0;
  virtual StatusOr<storage::internal::EmptyResponse> DeleteNotification(
      rest_internal::RestContext&, Options const&,
      storage::internal::DeleteNotificationRequest const&) = 0;
  ///@}

  // Test-only. Returns the names of the decorator stack elements.
  virtual std::vector<std::string> InspectStackStructure() const = 0;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GENERIC_STUB_H
