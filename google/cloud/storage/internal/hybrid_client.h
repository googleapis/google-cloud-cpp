// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HYBRID_CLIENT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HYBRID_CLIENT_H

#include "google/cloud/storage/internal/raw_client.h"
#include "google/cloud/storage/version.h"
#include <string>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class HybridClient : public storage::internal::RawClient {
 public:
  static std::shared_ptr<RawClient> Create(Options const& options);
  ~HybridClient() override = default;

  storage::ClientOptions const& client_options() const override;
  Options options() const override;

  StatusOr<storage::internal::ListBucketsResponse> ListBuckets(
      storage::internal::ListBucketsRequest const& request) override;
  StatusOr<storage::BucketMetadata> CreateBucket(
      storage::internal::CreateBucketRequest const& request) override;
  StatusOr<storage::BucketMetadata> GetBucketMetadata(
      storage::internal::GetBucketMetadataRequest const& request) override;
  StatusOr<storage::internal::EmptyResponse> DeleteBucket(
      storage::internal::DeleteBucketRequest const&) override;
  StatusOr<storage::BucketMetadata> UpdateBucket(
      storage::internal::UpdateBucketRequest const& request) override;
  StatusOr<storage::BucketMetadata> PatchBucket(
      storage::internal::PatchBucketRequest const& request) override;
  StatusOr<storage::NativeIamPolicy> GetNativeBucketIamPolicy(
      storage::internal::GetBucketIamPolicyRequest const& request) override;
  StatusOr<storage::NativeIamPolicy> SetNativeBucketIamPolicy(
      storage::internal::SetNativeBucketIamPolicyRequest const& request)
      override;
  StatusOr<storage::internal::TestBucketIamPermissionsResponse>
  TestBucketIamPermissions(
      storage::internal::TestBucketIamPermissionsRequest const& request)
      override;
  StatusOr<storage::BucketMetadata> LockBucketRetentionPolicy(
      storage::internal::LockBucketRetentionPolicyRequest const& request)
      override;

  StatusOr<storage::ObjectMetadata> InsertObjectMedia(
      storage::internal::InsertObjectMediaRequest const& request) override;
  StatusOr<storage::ObjectMetadata> CopyObject(
      storage::internal::CopyObjectRequest const& request) override;
  StatusOr<storage::ObjectMetadata> GetObjectMetadata(
      storage::internal::GetObjectMetadataRequest const& request) override;

  StatusOr<std::unique_ptr<storage::internal::ObjectReadSource>> ReadObject(
      storage::internal::ReadObjectRangeRequest const&) override;

  StatusOr<storage::internal::ListObjectsResponse> ListObjects(
      storage::internal::ListObjectsRequest const&) override;
  StatusOr<storage::internal::EmptyResponse> DeleteObject(
      storage::internal::DeleteObjectRequest const&) override;
  StatusOr<storage::ObjectMetadata> UpdateObject(
      storage::internal::UpdateObjectRequest const& request) override;
  StatusOr<storage::ObjectMetadata> PatchObject(
      storage::internal::PatchObjectRequest const& request) override;
  StatusOr<storage::ObjectMetadata> ComposeObject(
      storage::internal::ComposeObjectRequest const& request) override;
  StatusOr<storage::internal::RewriteObjectResponse> RewriteObject(
      storage::internal::RewriteObjectRequest const&) override;

  StatusOr<storage::internal::CreateResumableUploadResponse>
  CreateResumableUpload(
      storage::internal::ResumableUploadRequest const& request) override;
  StatusOr<storage::internal::QueryResumableUploadResponse>
  QueryResumableUpload(
      storage::internal::QueryResumableUploadRequest const& request) override;
  StatusOr<storage::internal::EmptyResponse> DeleteResumableUpload(
      storage::internal::DeleteResumableUploadRequest const& request) override;
  StatusOr<storage::internal::QueryResumableUploadResponse> UploadChunk(
      storage::internal::UploadChunkRequest const& request) override;

  StatusOr<storage::internal::ListBucketAclResponse> ListBucketAcl(
      storage::internal::ListBucketAclRequest const& request) override;
  StatusOr<storage::BucketAccessControl> CreateBucketAcl(
      storage::internal::CreateBucketAclRequest const&) override;
  StatusOr<storage::internal::EmptyResponse> DeleteBucketAcl(
      storage::internal::DeleteBucketAclRequest const&) override;
  StatusOr<storage::BucketAccessControl> GetBucketAcl(
      storage::internal::GetBucketAclRequest const&) override;
  StatusOr<storage::BucketAccessControl> UpdateBucketAcl(
      storage::internal::UpdateBucketAclRequest const&) override;
  StatusOr<storage::BucketAccessControl> PatchBucketAcl(
      storage::internal::PatchBucketAclRequest const&) override;

  StatusOr<storage::internal::ListObjectAclResponse> ListObjectAcl(
      storage::internal::ListObjectAclRequest const& request) override;
  StatusOr<storage::ObjectAccessControl> CreateObjectAcl(
      storage::internal::CreateObjectAclRequest const&) override;
  StatusOr<storage::internal::EmptyResponse> DeleteObjectAcl(
      storage::internal::DeleteObjectAclRequest const&) override;
  StatusOr<storage::ObjectAccessControl> GetObjectAcl(
      storage::internal::GetObjectAclRequest const&) override;
  StatusOr<storage::ObjectAccessControl> UpdateObjectAcl(
      storage::internal::UpdateObjectAclRequest const&) override;
  StatusOr<storage::ObjectAccessControl> PatchObjectAcl(
      storage::internal::PatchObjectAclRequest const&) override;

  StatusOr<storage::internal::ListDefaultObjectAclResponse>
  ListDefaultObjectAcl(
      storage::internal::ListDefaultObjectAclRequest const& request) override;
  StatusOr<storage::ObjectAccessControl> CreateDefaultObjectAcl(
      storage::internal::CreateDefaultObjectAclRequest const&) override;
  StatusOr<storage::internal::EmptyResponse> DeleteDefaultObjectAcl(
      storage::internal::DeleteDefaultObjectAclRequest const&) override;
  StatusOr<storage::ObjectAccessControl> GetDefaultObjectAcl(
      storage::internal::GetDefaultObjectAclRequest const&) override;
  StatusOr<storage::ObjectAccessControl> UpdateDefaultObjectAcl(
      storage::internal::UpdateDefaultObjectAclRequest const&) override;
  StatusOr<storage::ObjectAccessControl> PatchDefaultObjectAcl(
      storage::internal::PatchDefaultObjectAclRequest const&) override;

  StatusOr<storage::ServiceAccount> GetServiceAccount(
      storage::internal::GetProjectServiceAccountRequest const&) override;
  StatusOr<storage::internal::ListHmacKeysResponse> ListHmacKeys(
      storage::internal::ListHmacKeysRequest const&) override;
  StatusOr<storage::internal::CreateHmacKeyResponse> CreateHmacKey(
      storage::internal::CreateHmacKeyRequest const&) override;
  StatusOr<storage::internal::EmptyResponse> DeleteHmacKey(
      storage::internal::DeleteHmacKeyRequest const&) override;
  StatusOr<storage::HmacKeyMetadata> GetHmacKey(
      storage::internal::GetHmacKeyRequest const&) override;
  StatusOr<storage::HmacKeyMetadata> UpdateHmacKey(
      storage::internal::UpdateHmacKeyRequest const&) override;
  StatusOr<storage::internal::SignBlobResponse> SignBlob(
      storage::internal::SignBlobRequest const&) override;

  StatusOr<storage::internal::ListNotificationsResponse> ListNotifications(
      storage::internal::ListNotificationsRequest const&) override;
  StatusOr<storage::NotificationMetadata> CreateNotification(
      storage::internal::CreateNotificationRequest const&) override;
  StatusOr<storage::NotificationMetadata> GetNotification(
      storage::internal::GetNotificationRequest const&) override;
  StatusOr<storage::internal::EmptyResponse> DeleteNotification(
      storage::internal::DeleteNotificationRequest const&) override;

 private:
  explicit HybridClient(Options const& options);

  std::shared_ptr<RawClient> grpc_;
  std::shared_ptr<RawClient> rest_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HYBRID_CLIENT_H
