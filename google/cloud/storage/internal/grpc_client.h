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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_CLIENT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_CLIENT_H

#include "google/cloud/storage/internal/raw_client.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/background_threads.h"
#include "google/cloud/internal/minimal_iam_credentials_stub.h"
#include "google/cloud/internal/streaming_write_rpc.h"
#include <google/storage/v2/storage.pb.h>
#include <functional>
#include <memory>
#include <string>

namespace google {
namespace cloud {

namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
class StorageStub;
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal

namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * The default options for gRPC.
 *
 * This adds some additional defaults to the options for REST.
 */
Options DefaultOptionsGrpc(Options = {});

class GrpcClient : public RawClient,
                   public std::enable_shared_from_this<GrpcClient> {
 public:
  // Creates a new instance, assumes the options have all default values set.
  static std::shared_ptr<GrpcClient> Create(Options opts);

  // This is used to create a client from a mocked StorageStub.
  static std::shared_ptr<GrpcClient> CreateMock(
      std::shared_ptr<storage_internal::StorageStub> stub, Options opts = {});

  // This is used to create a client from a mocked StorageStub.
  static std::shared_ptr<GrpcClient> CreateMock(
      std::shared_ptr<storage_internal::StorageStub> stub,
      std::shared_ptr<google::cloud::internal::MinimalIamCredentialsStub> iam,
      Options opts = {});

  ~GrpcClient() override = default;

  using WriteObjectStream = ::google::cloud::internal::StreamingWriteRpc<
      google::storage::v2::WriteObjectRequest,
      google::storage::v2::WriteObjectResponse>;

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
  StatusOr<ObjectMetadata> CopyObject(
      CopyObjectRequest const& request) override;
  StatusOr<ObjectMetadata> GetObjectMetadata(
      GetObjectMetadataRequest const& request) override;

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

 protected:
  explicit GrpcClient(Options opts);
  explicit GrpcClient(
      std::shared_ptr<storage_internal::StorageStub> stub,
      std::shared_ptr<google::cloud::internal::MinimalIamCredentialsStub> iam,
      Options opts);

 private:
  using BucketAccessControlList = google::protobuf::RepeatedPtrField<
      google::storage::v2::BucketAccessControl>;
  using BucketAclUpdater = std::function<StatusOr<BucketAccessControlList>(
      BucketAccessControlList acl)>;

  // REST has RPCs that change `BucketAccessControl` resources atomically. gRPC
  // lacks such RPCs. This function hijacks the retry loop to implement an OCC
  // loop to make such changes.
  StatusOr<BucketMetadata> ModifyBucketAccessControl(
      GetBucketMetadataRequest const& request, BucketAclUpdater const& updater);

  using ObjectAccessControlList = google::protobuf::RepeatedPtrField<
      google::storage::v2::ObjectAccessControl>;
  using ObjectAclUpdater = std::function<StatusOr<ObjectAccessControlList>(
      ObjectAccessControlList acl)>;

  // REST has RPCs that change `ObjectAccessControl` resources atomically. gRPC
  // lacks such RPCs. This function hijacks the retry loop to implement an OCC
  // loop to make such changes.
  StatusOr<ObjectMetadata> ModifyObjectAccessControl(
      GetObjectMetadataRequest const& request, ObjectAclUpdater const& updater);

  using DefaultObjectAclUpdater =
      std::function<StatusOr<ObjectAccessControlList>(
          ObjectAccessControlList acl)>;

  // REST has RPCs that change `DefaultObjectAccessControl` resources
  // atomically. gRPC lacks such RPCs. This function hijacks the retry loop to
  // implement an OCC loop to make such changes.
  StatusOr<BucketMetadata> ModifyDefaultAccessControl(
      GetBucketMetadataRequest const& request,
      DefaultObjectAclUpdater const& updater);

  Options options_;
  ClientOptions backwards_compatibility_options_;
  std::unique_ptr<google::cloud::BackgroundThreads> background_;
  std::shared_ptr<storage_internal::StorageStub> stub_;
  std::shared_ptr<google::cloud::internal::MinimalIamCredentialsStub> iam_stub_;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_CLIENT_H
