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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_LOGGING_STUB_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_LOGGING_STUB_H

#include "google/cloud/storage/internal/generic_stub.h"
#include "google/cloud/storage/version.h"
#include <memory>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
/**
 * A decorator for `StorageConnection` that logs each operation.
 */
class LoggingStub : public storage_internal::GenericStub {
 public:
  explicit LoggingStub(std::unique_ptr<storage_internal::GenericStub> stub);
  ~LoggingStub() override = default;

  Options options() const override;

  StatusOr<ListBucketsResponse> ListBuckets(
      rest_internal::RestContext&, Options const&,
      ListBucketsRequest const& request) override;
  StatusOr<BucketMetadata> CreateBucket(
      rest_internal::RestContext&, Options const&,
      CreateBucketRequest const& request) override;
  StatusOr<BucketMetadata> GetBucketMetadata(
      rest_internal::RestContext&, Options const&,
      GetBucketMetadataRequest const& request) override;
  StatusOr<EmptyResponse> DeleteBucket(rest_internal::RestContext&,
                                       Options const&,
                                       DeleteBucketRequest const&) override;
  StatusOr<BucketMetadata> UpdateBucket(
      rest_internal::RestContext&, Options const&,
      UpdateBucketRequest const& request) override;
  StatusOr<BucketMetadata> PatchBucket(
      rest_internal::RestContext&, Options const&,
      PatchBucketRequest const& request) override;
  StatusOr<NativeIamPolicy> GetNativeBucketIamPolicy(
      rest_internal::RestContext&, Options const&,
      GetBucketIamPolicyRequest const& request) override;
  StatusOr<NativeIamPolicy> SetNativeBucketIamPolicy(
      rest_internal::RestContext&, Options const&,
      SetNativeBucketIamPolicyRequest const& request) override;
  StatusOr<TestBucketIamPermissionsResponse> TestBucketIamPermissions(
      rest_internal::RestContext&, Options const&,
      TestBucketIamPermissionsRequest const& request) override;
  StatusOr<BucketMetadata> LockBucketRetentionPolicy(
      rest_internal::RestContext&, Options const&,
      LockBucketRetentionPolicyRequest const& request) override;

  StatusOr<ObjectMetadata> InsertObjectMedia(
      rest_internal::RestContext&, Options const&,
      InsertObjectMediaRequest const& request) override;
  StatusOr<ObjectMetadata> CopyObject(
      rest_internal::RestContext&, Options const&,
      CopyObjectRequest const& request) override;
  StatusOr<ObjectMetadata> GetObjectMetadata(
      rest_internal::RestContext&, Options const&,
      GetObjectMetadataRequest const& request) override;
  StatusOr<std::unique_ptr<ObjectReadSource>> ReadObject(
      rest_internal::RestContext&, Options const&,
      ReadObjectRangeRequest const&) override;
  StatusOr<ListObjectsResponse> ListObjects(rest_internal::RestContext&,
                                            Options const&,
                                            ListObjectsRequest const&) override;
  StatusOr<EmptyResponse> DeleteObject(rest_internal::RestContext&,
                                       Options const&,
                                       DeleteObjectRequest const&) override;
  StatusOr<ObjectMetadata> UpdateObject(
      rest_internal::RestContext&, Options const&,
      UpdateObjectRequest const& request) override;
  StatusOr<ObjectMetadata> PatchObject(
      rest_internal::RestContext&, Options const&,
      PatchObjectRequest const& request) override;
  StatusOr<ObjectMetadata> ComposeObject(
      rest_internal::RestContext&, Options const&,
      ComposeObjectRequest const& request) override;
  StatusOr<RewriteObjectResponse> RewriteObject(
      rest_internal::RestContext&, Options const&,
      RewriteObjectRequest const&) override;
  StatusOr<ObjectMetadata> RestoreObject(
      rest_internal::RestContext&, Options const&,
      RestoreObjectRequest const& request) override;

  StatusOr<CreateResumableUploadResponse> CreateResumableUpload(
      rest_internal::RestContext&, Options const&,
      ResumableUploadRequest const& request) override;
  StatusOr<QueryResumableUploadResponse> QueryResumableUpload(
      rest_internal::RestContext&, Options const&,
      QueryResumableUploadRequest const& request) override;
  StatusOr<EmptyResponse> DeleteResumableUpload(
      rest_internal::RestContext&, Options const&,
      DeleteResumableUploadRequest const& request) override;
  StatusOr<QueryResumableUploadResponse> UploadChunk(
      rest_internal::RestContext&, Options const&,
      UploadChunkRequest const& request) override;

  StatusOr<ListBucketAclResponse> ListBucketAcl(
      rest_internal::RestContext&, Options const&,
      ListBucketAclRequest const& request) override;
  StatusOr<BucketAccessControl> CreateBucketAcl(
      rest_internal::RestContext&, Options const&,
      CreateBucketAclRequest const&) override;
  StatusOr<EmptyResponse> DeleteBucketAcl(
      rest_internal::RestContext&, Options const&,
      DeleteBucketAclRequest const&) override;
  StatusOr<BucketAccessControl> GetBucketAcl(
      rest_internal::RestContext&, Options const&,
      GetBucketAclRequest const&) override;
  StatusOr<BucketAccessControl> UpdateBucketAcl(
      rest_internal::RestContext&, Options const&,
      UpdateBucketAclRequest const&) override;
  StatusOr<BucketAccessControl> PatchBucketAcl(
      rest_internal::RestContext&, Options const&,
      PatchBucketAclRequest const&) override;

  StatusOr<ListObjectAclResponse> ListObjectAcl(
      rest_internal::RestContext&, Options const&,
      ListObjectAclRequest const& request) override;
  StatusOr<ObjectAccessControl> CreateObjectAcl(
      rest_internal::RestContext&, Options const&,
      CreateObjectAclRequest const&) override;
  StatusOr<EmptyResponse> DeleteObjectAcl(
      rest_internal::RestContext&, Options const&,
      DeleteObjectAclRequest const&) override;
  StatusOr<ObjectAccessControl> GetObjectAcl(
      rest_internal::RestContext&, Options const&,
      GetObjectAclRequest const&) override;
  StatusOr<ObjectAccessControl> UpdateObjectAcl(
      rest_internal::RestContext&, Options const&,
      UpdateObjectAclRequest const&) override;
  StatusOr<ObjectAccessControl> PatchObjectAcl(
      rest_internal::RestContext&, Options const&,
      PatchObjectAclRequest const&) override;

  StatusOr<ListDefaultObjectAclResponse> ListDefaultObjectAcl(
      rest_internal::RestContext&, Options const&,
      ListDefaultObjectAclRequest const& request) override;
  StatusOr<ObjectAccessControl> CreateDefaultObjectAcl(
      rest_internal::RestContext&, Options const&,
      CreateDefaultObjectAclRequest const&) override;
  StatusOr<EmptyResponse> DeleteDefaultObjectAcl(
      rest_internal::RestContext&, Options const&,
      DeleteDefaultObjectAclRequest const&) override;
  StatusOr<ObjectAccessControl> GetDefaultObjectAcl(
      rest_internal::RestContext&, Options const&,
      GetDefaultObjectAclRequest const&) override;
  StatusOr<ObjectAccessControl> UpdateDefaultObjectAcl(
      rest_internal::RestContext&, Options const&,
      UpdateDefaultObjectAclRequest const&) override;
  StatusOr<ObjectAccessControl> PatchDefaultObjectAcl(
      rest_internal::RestContext&, Options const&,
      PatchDefaultObjectAclRequest const&) override;

  StatusOr<ServiceAccount> GetServiceAccount(
      rest_internal::RestContext&, Options const&,
      GetProjectServiceAccountRequest const&) override;
  StatusOr<ListHmacKeysResponse> ListHmacKeys(
      rest_internal::RestContext&, Options const&,
      ListHmacKeysRequest const&) override;
  StatusOr<CreateHmacKeyResponse> CreateHmacKey(
      rest_internal::RestContext&, Options const&,
      CreateHmacKeyRequest const&) override;
  StatusOr<EmptyResponse> DeleteHmacKey(rest_internal::RestContext&,
                                        Options const&,
                                        DeleteHmacKeyRequest const&) override;
  StatusOr<HmacKeyMetadata> GetHmacKey(rest_internal::RestContext&,
                                       Options const&,
                                       GetHmacKeyRequest const&) override;
  StatusOr<HmacKeyMetadata> UpdateHmacKey(rest_internal::RestContext&,
                                          Options const&,
                                          UpdateHmacKeyRequest const&) override;
  StatusOr<SignBlobResponse> SignBlob(rest_internal::RestContext&,
                                      Options const&,
                                      SignBlobRequest const&) override;

  StatusOr<ListNotificationsResponse> ListNotifications(
      rest_internal::RestContext&, Options const&,
      ListNotificationsRequest const&) override;
  StatusOr<NotificationMetadata> CreateNotification(
      rest_internal::RestContext&, Options const&,
      CreateNotificationRequest const&) override;
  StatusOr<NotificationMetadata> GetNotification(
      rest_internal::RestContext&, Options const&,
      GetNotificationRequest const&) override;
  StatusOr<EmptyResponse> DeleteNotification(
      rest_internal::RestContext&, Options const&,
      DeleteNotificationRequest const&) override;

  std::vector<std::string> InspectStackStructure() const override;

 private:
  std::unique_ptr<storage_internal::GenericStub> stub_;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_LOGGING_STUB_H
