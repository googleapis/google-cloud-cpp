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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_REST_CLIENT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_REST_CLIENT_H

#include "google/cloud/storage/internal/raw_client.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/internal/rest_client.h"
#include <memory>
#include <mutex>
#include <string>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * Implements the low-level RPCs to Google Cloud Storage using a combination of
 * the google-cloud-cpp::rest-internal library and GCS bespoke libcurl wrappers.
 *
 * Over time, this will migrate from 100% libcurl wrappers to 100% REST library.
 */
class RestClient : public RawClient,
                   public std::enable_shared_from_this<RestClient> {
 public:
  static std::shared_ptr<RestClient> Create(Options options);
  static std::shared_ptr<RestClient> Create(
      Options options,
      std::shared_ptr<google::cloud::rest_internal::RestClient>
          storage_rest_client,
      std::shared_ptr<google::cloud::rest_internal::RestClient>
          iam_rest_client);

  static Options ResolveStorageAuthority(Options const& options);
  static Options ResolveIamAuthority(Options const& options);

  RestClient(RestClient const& rhs) = delete;
  RestClient(RestClient&& rhs) = delete;
  RestClient& operator=(RestClient const& rhs) = delete;
  RestClient& operator=(RestClient&& rhs) = delete;

  ClientOptions const& client_options() const override;
  Options options() const override;

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
  StatusOr<NativeIamPolicy> GetNativeBucketIamPolicy(
      GetBucketIamPolicyRequest const& request) override;
  StatusOr<NativeIamPolicy> SetNativeBucketIamPolicy(
      SetNativeBucketIamPolicyRequest const& request) override;
  StatusOr<TestBucketIamPermissionsResponse> TestBucketIamPermissions(
      TestBucketIamPermissionsRequest const& request) override;
  StatusOr<BucketMetadata> LockBucketRetentionPolicy(
      LockBucketRetentionPolicyRequest const& request) override;

  StatusOr<ObjectMetadata> InsertObjectMedia(
      InsertObjectMediaRequest const& request) override;
  StatusOr<ObjectMetadata> GetObjectMetadata(
      GetObjectMetadataRequest const& request) override;
  StatusOr<std::unique_ptr<ObjectReadSource>> ReadObject(
      ReadObjectRangeRequest const&) override;
  StatusOr<ListObjectsResponse> ListObjects(
      ListObjectsRequest const& request) override;
  StatusOr<EmptyResponse> DeleteObject(
      DeleteObjectRequest const& request) override;
  StatusOr<ObjectMetadata> UpdateObject(
      UpdateObjectRequest const& request) override;
  StatusOr<ObjectMetadata> PatchObject(
      PatchObjectRequest const& request) override;
  StatusOr<ObjectMetadata> ComposeObject(
      ComposeObjectRequest const& request) override;

  StatusOr<CreateResumableUploadResponse> CreateResumableUpload(
      ResumableUploadRequest const& request) override;
  StatusOr<QueryResumableUploadResponse> QueryResumableUpload(
      QueryResumableUploadRequest const& request) override;
  StatusOr<EmptyResponse> DeleteResumableUpload(
      DeleteResumableUploadRequest const& request) override;
  StatusOr<QueryResumableUploadResponse> UploadChunk(
      UploadChunkRequest const& request) override;

  StatusOr<ListBucketAclResponse> ListBucketAcl(
      ListBucketAclRequest const& request) override;
  StatusOr<ObjectMetadata> CopyObject(
      CopyObjectRequest const& request) override;
  StatusOr<BucketAccessControl> CreateBucketAcl(
      CreateBucketAclRequest const&) override;
  StatusOr<BucketAccessControl> GetBucketAcl(
      GetBucketAclRequest const&) override;
  StatusOr<EmptyResponse> DeleteBucketAcl(
      DeleteBucketAclRequest const&) override;
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
  StatusOr<RewriteObjectResponse> RewriteObject(
      RewriteObjectRequest const&) override;

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

 protected:
  explicit RestClient(
      std::shared_ptr<google::cloud::rest_internal::RestClient>
          storage_rest_client,
      std::shared_ptr<google::cloud::rest_internal::RestClient> iam_rest_client,
      Options options);

 private:
  StatusOr<ObjectMetadata> InsertObjectMediaMultipart(
      InsertObjectMediaRequest const& request);

  StatusOr<ObjectMetadata> InsertObjectMediaXml(
      InsertObjectMediaRequest const& request);

  StatusOr<ObjectMetadata> InsertObjectMediaSimple(
      InsertObjectMediaRequest const& request);

  std::string MakeBoundary();
  StatusOr<std::unique_ptr<ObjectReadSource>> ReadObjectXml(
      ReadObjectRangeRequest const& request);

  std::shared_ptr<google::cloud::rest_internal::RestClient>
      storage_rest_client_;
  std::shared_ptr<google::cloud::rest_internal::RestClient> iam_rest_client_;
  bool const xml_enabled_;
  std::mutex mu_;
  google::cloud::internal::DefaultPRNG generator_;  // GUARDED_BY(mu_);
  google::cloud::Options options_;
  ClientOptions backwards_compatibility_options_;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_REST_CLIENT_H
