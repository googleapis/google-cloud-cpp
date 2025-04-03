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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_REST_STUB_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_REST_STUB_H

#include "google/cloud/storage/internal/generic_stub.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/internal/rest_client.h"
#include "google/cloud/storage/internal/rest/request_builder.h"
#include <memory>
#include <mutex>
#include <string>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

Status AddCustomHeaders(Options const& options, RestRequestBuilder& builder);

/**
 * Implements the low-level RPCs to Google Cloud Storage using a combination of
 * the google-cloud-cpp::rest-internal library and GCS bespoke libcurl wrappers.
 *
 * Over time, this will migrate from 100% libcurl wrappers to 100% REST library.
 */
class RestStub : public storage_internal::GenericStub {
 public:
  explicit RestStub(Options options);
  explicit RestStub(Options options,
                    std::shared_ptr<google::cloud::rest_internal::RestClient>
                        storage_rest_client,
                    std::shared_ptr<google::cloud::rest_internal::RestClient>
                        iam_rest_client);

  static Options ResolveStorageAuthority(Options const& options);
  static Options ResolveIamAuthority(Options const& options);

  RestStub(RestStub const& rhs) = delete;
  RestStub(RestStub&& rhs) = delete;
  RestStub& operator=(RestStub const& rhs) = delete;
  RestStub& operator=(RestStub&& rhs) = delete;

  Options options() const override;

  StatusOr<ListBucketsResponse> ListBuckets(
      rest_internal::RestContext& context, Options const& options,
      ListBucketsRequest const& request) override;
  StatusOr<BucketMetadata> CreateBucket(
      rest_internal::RestContext& context, Options const& options,
      CreateBucketRequest const& request) override;
  StatusOr<BucketMetadata> GetBucketMetadata(
      rest_internal::RestContext& context, Options const& options,
      GetBucketMetadataRequest const& request) override;
  StatusOr<EmptyResponse> DeleteBucket(rest_internal::RestContext& context,
                                       Options const& options,
                                       DeleteBucketRequest const&) override;
  StatusOr<BucketMetadata> UpdateBucket(
      rest_internal::RestContext& context, Options const& options,
      UpdateBucketRequest const& request) override;
  StatusOr<BucketMetadata> PatchBucket(
      rest_internal::RestContext& context, Options const& options,
      PatchBucketRequest const& request) override;
  StatusOr<NativeIamPolicy> GetNativeBucketIamPolicy(
      rest_internal::RestContext& context, Options const& options,
      GetBucketIamPolicyRequest const& request) override;
  StatusOr<NativeIamPolicy> SetNativeBucketIamPolicy(
      rest_internal::RestContext& context, Options const& options,
      SetNativeBucketIamPolicyRequest const& request) override;
  StatusOr<TestBucketIamPermissionsResponse> TestBucketIamPermissions(
      rest_internal::RestContext& context, Options const& options,
      TestBucketIamPermissionsRequest const& request) override;
  StatusOr<BucketMetadata> LockBucketRetentionPolicy(
      rest_internal::RestContext& context, Options const& options,
      LockBucketRetentionPolicyRequest const& request) override;

  StatusOr<ObjectMetadata> InsertObjectMedia(
      rest_internal::RestContext& context, Options const& options,
      InsertObjectMediaRequest const& request) override;
  StatusOr<ObjectMetadata> GetObjectMetadata(
      rest_internal::RestContext& context, Options const& options,
      GetObjectMetadataRequest const& request) override;
  StatusOr<std::unique_ptr<ObjectReadSource>> ReadObject(
      rest_internal::RestContext& context, Options const& options,
      ReadObjectRangeRequest const&) override;
  StatusOr<ListObjectsResponse> ListObjects(
      rest_internal::RestContext& context, Options const& options,
      ListObjectsRequest const& request) override;
  StatusOr<EmptyResponse> DeleteObject(
      rest_internal::RestContext& context, Options const& options,
      DeleteObjectRequest const& request) override;
  StatusOr<ObjectMetadata> UpdateObject(
      rest_internal::RestContext& context, Options const& options,
      UpdateObjectRequest const& request) override;
  StatusOr<ObjectMetadata> MoveObject(
      rest_internal::RestContext& context, Options const& options,
      MoveObjectRequest const& request) override;
  StatusOr<ObjectMetadata> PatchObject(
      rest_internal::RestContext& context, Options const& options,
      PatchObjectRequest const& request) override;
  StatusOr<ObjectMetadata> ComposeObject(
      rest_internal::RestContext& context, Options const& options,
      ComposeObjectRequest const& request) override;

  StatusOr<CreateResumableUploadResponse> CreateResumableUpload(
      rest_internal::RestContext& context, Options const& options,
      ResumableUploadRequest const& request) override;
  StatusOr<QueryResumableUploadResponse> QueryResumableUpload(
      rest_internal::RestContext& context, Options const& options,
      QueryResumableUploadRequest const& request) override;
  StatusOr<EmptyResponse> DeleteResumableUpload(
      rest_internal::RestContext& context, Options const& options,
      DeleteResumableUploadRequest const& request) override;
  StatusOr<QueryResumableUploadResponse> UploadChunk(
      rest_internal::RestContext& context, Options const& options,
      UploadChunkRequest const& request) override;

  StatusOr<ListBucketAclResponse> ListBucketAcl(
      rest_internal::RestContext& context, Options const& options,
      ListBucketAclRequest const& request) override;
  StatusOr<ObjectMetadata> CopyObject(
      rest_internal::RestContext& context, Options const& options,
      CopyObjectRequest const& request) override;
  StatusOr<BucketAccessControl> CreateBucketAcl(
      rest_internal::RestContext& context, Options const& options,
      CreateBucketAclRequest const& request) override;
  StatusOr<BucketAccessControl> GetBucketAcl(
      rest_internal::RestContext& context, Options const& options,
      GetBucketAclRequest const& request) override;
  StatusOr<EmptyResponse> DeleteBucketAcl(
      rest_internal::RestContext& context, Options const& options,
      DeleteBucketAclRequest const& request) override;
  StatusOr<BucketAccessControl> UpdateBucketAcl(
      rest_internal::RestContext& context, Options const& options,
      UpdateBucketAclRequest const& request) override;
  StatusOr<BucketAccessControl> PatchBucketAcl(
      rest_internal::RestContext& context, Options const& options,
      PatchBucketAclRequest const& request) override;

  StatusOr<ListObjectAclResponse> ListObjectAcl(
      rest_internal::RestContext& context, Options const& options,
      ListObjectAclRequest const& request) override;
  StatusOr<ObjectAccessControl> CreateObjectAcl(
      rest_internal::RestContext& context, Options const& options,
      CreateObjectAclRequest const& request) override;
  StatusOr<EmptyResponse> DeleteObjectAcl(
      rest_internal::RestContext& context, Options const& options,
      DeleteObjectAclRequest const& request) override;
  StatusOr<ObjectAccessControl> GetObjectAcl(
      rest_internal::RestContext& context, Options const& options,
      GetObjectAclRequest const& request) override;
  StatusOr<ObjectAccessControl> UpdateObjectAcl(
      rest_internal::RestContext& context, Options const& options,
      UpdateObjectAclRequest const& request) override;
  StatusOr<ObjectAccessControl> PatchObjectAcl(
      rest_internal::RestContext& context, Options const& options,
      PatchObjectAclRequest const& request) override;
  StatusOr<RewriteObjectResponse> RewriteObject(
      rest_internal::RestContext& context, Options const& options,
      RewriteObjectRequest const& request) override;
  StatusOr<ObjectMetadata> RestoreObject(
      rest_internal::RestContext& context, Options const& options,
      RestoreObjectRequest const& request) override;

  StatusOr<ListDefaultObjectAclResponse> ListDefaultObjectAcl(
      rest_internal::RestContext& context, Options const& options,
      ListDefaultObjectAclRequest const& request) override;
  StatusOr<ObjectAccessControl> CreateDefaultObjectAcl(
      rest_internal::RestContext& context, Options const& options,
      CreateDefaultObjectAclRequest const& request) override;
  StatusOr<EmptyResponse> DeleteDefaultObjectAcl(
      rest_internal::RestContext& context, Options const& options,
      DeleteDefaultObjectAclRequest const& request) override;
  StatusOr<ObjectAccessControl> GetDefaultObjectAcl(
      rest_internal::RestContext& context, Options const& options,
      GetDefaultObjectAclRequest const& request) override;
  StatusOr<ObjectAccessControl> UpdateDefaultObjectAcl(
      rest_internal::RestContext& context, Options const& options,
      UpdateDefaultObjectAclRequest const& request) override;
  StatusOr<ObjectAccessControl> PatchDefaultObjectAcl(
      rest_internal::RestContext& context, Options const& options,
      PatchDefaultObjectAclRequest const& request) override;

  StatusOr<ServiceAccount> GetServiceAccount(
      rest_internal::RestContext& context, Options const& options,
      GetProjectServiceAccountRequest const&) override;
  StatusOr<ListHmacKeysResponse> ListHmacKeys(
      rest_internal::RestContext& context, Options const& options,
      ListHmacKeysRequest const& request) override;
  StatusOr<CreateHmacKeyResponse> CreateHmacKey(
      rest_internal::RestContext& context, Options const& options,
      CreateHmacKeyRequest const& request) override;
  StatusOr<EmptyResponse> DeleteHmacKey(rest_internal::RestContext& context,
                                        Options const& options,
                                        DeleteHmacKeyRequest const&) override;
  StatusOr<HmacKeyMetadata> GetHmacKey(
      rest_internal::RestContext& context, Options const& options,
      GetHmacKeyRequest const& request) override;
  StatusOr<HmacKeyMetadata> UpdateHmacKey(
      rest_internal::RestContext& context, Options const& options,
      UpdateHmacKeyRequest const& request) override;
  StatusOr<SignBlobResponse> SignBlob(rest_internal::RestContext& context,
                                      Options const& options,
                                      SignBlobRequest const& request) override;

  StatusOr<ListNotificationsResponse> ListNotifications(
      rest_internal::RestContext& context, Options const& options,
      ListNotificationsRequest const& request) override;
  StatusOr<NotificationMetadata> CreateNotification(
      rest_internal::RestContext& context, Options const& options,
      CreateNotificationRequest const& request) override;
  StatusOr<NotificationMetadata> GetNotification(
      rest_internal::RestContext& context, Options const& options,
      GetNotificationRequest const& request) override;
  StatusOr<EmptyResponse> DeleteNotification(
      rest_internal::RestContext& context, Options const& options,
      DeleteNotificationRequest const& request) override;

  std::vector<std::string> InspectStackStructure() const override;

 private:
  StatusOr<ObjectMetadata> InsertObjectMediaMultipart(
      rest_internal::RestContext& context, Options const& options,
      InsertObjectMediaRequest const& request);

  StatusOr<ObjectMetadata> InsertObjectMediaSimple(
      rest_internal::RestContext& context, Options const& options,
      InsertObjectMediaRequest const& request);

  std::string MakeBoundary();

  google::cloud::Options options_;
  std::shared_ptr<google::cloud::rest_internal::RestClient>
      storage_rest_client_;
  std::shared_ptr<google::cloud::rest_internal::RestClient> iam_rest_client_;
  std::mutex mu_;
  google::cloud::internal::DefaultPRNG generator_;  // GUARDED_BY(mu_);
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_REST_STUB_H
