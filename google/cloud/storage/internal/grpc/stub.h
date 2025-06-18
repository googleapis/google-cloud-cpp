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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_STUB_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_STUB_H

#include "google/cloud/storage/internal/generic_stub.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/background_threads.h"
#include "google/cloud/internal/minimal_iam_credentials_stub.h"
#include "google/cloud/internal/streaming_write_rpc.h"
#include "google/storage/v2/storage.pb.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
class GrpcChannelRefresh;
class StorageStub;

class GrpcStub : public GenericStub {
 public:
  explicit GrpcStub(Options opts);
  explicit GrpcStub(
      std::shared_ptr<storage_internal::StorageStub> stub,
      std::shared_ptr<google::cloud::internal::MinimalIamCredentialsStub> iam,
      Options opts);

  ~GrpcStub() override = default;

  using WriteObjectStream = ::google::cloud::internal::StreamingWriteRpc<
      google::storage::v2::WriteObjectRequest,
      google::storage::v2::WriteObjectResponse>;

  Options options() const override;

  StatusOr<storage::internal::ListBucketsResponse> ListBuckets(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::ListBucketsRequest const& request) override;
  StatusOr<storage::BucketMetadata> CreateBucket(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::CreateBucketRequest const& request) override;
  StatusOr<storage::BucketMetadata> GetBucketMetadata(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::GetBucketMetadataRequest const& request) override;
  StatusOr<storage::internal::EmptyResponse> DeleteBucket(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::DeleteBucketRequest const& request) override;
  StatusOr<storage::BucketMetadata> UpdateBucket(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::UpdateBucketRequest const& request) override;
  StatusOr<storage::BucketMetadata> PatchBucket(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::PatchBucketRequest const& request) override;
  StatusOr<storage::NativeIamPolicy> GetNativeBucketIamPolicy(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::GetBucketIamPolicyRequest const& request) override;
  StatusOr<storage::NativeIamPolicy> SetNativeBucketIamPolicy(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::SetNativeBucketIamPolicyRequest const& request)
      override;
  StatusOr<storage::internal::TestBucketIamPermissionsResponse>
  TestBucketIamPermissions(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::TestBucketIamPermissionsRequest const& request)
      override;
  StatusOr<storage::BucketMetadata> LockBucketRetentionPolicy(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::LockBucketRetentionPolicyRequest const& request)
      override;

  StatusOr<storage::ObjectMetadata> InsertObjectMedia(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::InsertObjectMediaRequest const& request) override;
  StatusOr<storage::ObjectMetadata> CopyObject(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::CopyObjectRequest const& request) override;
  StatusOr<storage::ObjectMetadata> GetObjectMetadata(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::GetObjectMetadataRequest const& request) override;

  StatusOr<std::unique_ptr<storage::internal::ObjectReadSource>> ReadObject(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::ReadObjectRangeRequest const& request) override;

  StatusOr<storage::internal::ListObjectsResponse> ListObjects(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::ListObjectsRequest const& request) override;
  StatusOr<storage::internal::EmptyResponse> DeleteObject(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::DeleteObjectRequest const& request) override;
  StatusOr<storage::ObjectMetadata> UpdateObject(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::UpdateObjectRequest const& request) override;
  StatusOr<storage::ObjectMetadata> MoveObject(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::MoveObjectRequest const& request) override;
  StatusOr<storage::ObjectMetadata> PatchObject(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::PatchObjectRequest const& request) override;
  StatusOr<storage::ObjectMetadata> ComposeObject(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::ComposeObjectRequest const& request) override;
  StatusOr<storage::internal::RewriteObjectResponse> RewriteObject(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::RewriteObjectRequest const& request) override;
  StatusOr<storage::ObjectMetadata> RestoreObject(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::RestoreObjectRequest const& request) override;

  StatusOr<storage::internal::CreateResumableUploadResponse>
  CreateResumableUpload(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::ResumableUploadRequest const& request) override;
  StatusOr<storage::internal::QueryResumableUploadResponse>
  QueryResumableUpload(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::QueryResumableUploadRequest const& request) override;
  StatusOr<storage::internal::EmptyResponse> DeleteResumableUpload(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::DeleteResumableUploadRequest const& request) override;
  StatusOr<storage::internal::QueryResumableUploadResponse> UploadChunk(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::UploadChunkRequest const& request) override;

  StatusOr<storage::internal::ListBucketAclResponse> ListBucketAcl(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::ListBucketAclRequest const& request) override;
  StatusOr<storage::BucketAccessControl> CreateBucketAcl(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::CreateBucketAclRequest const& request) override;
  StatusOr<storage::internal::EmptyResponse> DeleteBucketAcl(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::DeleteBucketAclRequest const& request) override;
  StatusOr<storage::BucketAccessControl> GetBucketAcl(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::GetBucketAclRequest const& request) override;
  StatusOr<storage::BucketAccessControl> UpdateBucketAcl(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::UpdateBucketAclRequest const& request) override;
  StatusOr<storage::BucketAccessControl> PatchBucketAcl(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::PatchBucketAclRequest const& request) override;

  StatusOr<storage::internal::ListObjectAclResponse> ListObjectAcl(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::ListObjectAclRequest const& request) override;
  StatusOr<storage::ObjectAccessControl> CreateObjectAcl(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::CreateObjectAclRequest const& request) override;
  StatusOr<storage::internal::EmptyResponse> DeleteObjectAcl(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::DeleteObjectAclRequest const& request) override;
  StatusOr<storage::ObjectAccessControl> GetObjectAcl(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::GetObjectAclRequest const& request) override;
  StatusOr<storage::ObjectAccessControl> UpdateObjectAcl(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::UpdateObjectAclRequest const& request) override;
  StatusOr<storage::ObjectAccessControl> PatchObjectAcl(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::PatchObjectAclRequest const& request) override;

  StatusOr<storage::internal::ListDefaultObjectAclResponse>
  ListDefaultObjectAcl(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::ListDefaultObjectAclRequest const& request) override;
  StatusOr<storage::ObjectAccessControl> CreateDefaultObjectAcl(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::CreateDefaultObjectAclRequest const& request) override;
  StatusOr<storage::internal::EmptyResponse> DeleteDefaultObjectAcl(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::DeleteDefaultObjectAclRequest const& request) override;
  StatusOr<storage::ObjectAccessControl> GetDefaultObjectAcl(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::GetDefaultObjectAclRequest const& request) override;
  StatusOr<storage::ObjectAccessControl> UpdateDefaultObjectAcl(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::UpdateDefaultObjectAclRequest const& request) override;
  StatusOr<storage::ObjectAccessControl> PatchDefaultObjectAcl(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::PatchDefaultObjectAclRequest const& request) override;

  StatusOr<storage::ServiceAccount> GetServiceAccount(
      rest_internal::RestContext&, Options const&,
      storage::internal::GetProjectServiceAccountRequest const&) override;
  StatusOr<storage::internal::ListHmacKeysResponse> ListHmacKeys(
      rest_internal::RestContext&, Options const&,
      storage::internal::ListHmacKeysRequest const&) override;
  StatusOr<storage::internal::CreateHmacKeyResponse> CreateHmacKey(
      rest_internal::RestContext&, Options const&,
      storage::internal::CreateHmacKeyRequest const&) override;
  StatusOr<storage::internal::EmptyResponse> DeleteHmacKey(
      rest_internal::RestContext&, Options const&,
      storage::internal::DeleteHmacKeyRequest const&) override;
  StatusOr<storage::HmacKeyMetadata> GetHmacKey(
      rest_internal::RestContext&, Options const&,
      storage::internal::GetHmacKeyRequest const&) override;
  StatusOr<storage::HmacKeyMetadata> UpdateHmacKey(
      rest_internal::RestContext&, Options const&,
      storage::internal::UpdateHmacKeyRequest const&) override;
  StatusOr<storage::internal::SignBlobResponse> SignBlob(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::SignBlobRequest const& request) override;

  StatusOr<storage::internal::ListNotificationsResponse> ListNotifications(
      rest_internal::RestContext&, Options const&,
      storage::internal::ListNotificationsRequest const&) override;
  StatusOr<storage::NotificationMetadata> CreateNotification(
      rest_internal::RestContext&, Options const&,
      storage::internal::CreateNotificationRequest const&) override;
  StatusOr<storage::NotificationMetadata> GetNotification(
      rest_internal::RestContext&, Options const&,
      storage::internal::GetNotificationRequest const&) override;
  StatusOr<storage::internal::EmptyResponse> DeleteNotification(
      rest_internal::RestContext&, Options const&,
      storage::internal::DeleteNotificationRequest const&) override;

  std::vector<std::string> InspectStackStructure() const override;

 private:
  StatusOr<google::storage::v2::Bucket> GetBucketMetadataImpl(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::GetBucketMetadataRequest const& request);
  StatusOr<google::storage::v2::Bucket> PatchBucketImpl(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::PatchBucketRequest const& request);
  StatusOr<google::storage::v2::Object> GetObjectMetadataImpl(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::GetObjectMetadataRequest const& request);
  StatusOr<google::storage::v2::Object> PatchObjectImpl(
      rest_internal::RestContext& context, Options const& options,
      storage::internal::PatchObjectRequest const& request);

  using BucketAccessControlList = google::protobuf::RepeatedPtrField<
      google::storage::v2::BucketAccessControl>;
  using BucketAclUpdater = std::function<StatusOr<BucketAccessControlList>(
      BucketAccessControlList acl)>;

  // REST has RPCs that change `BucketAccessControl` resources atomically. gRPC
  // lacks such RPCs. This function hijacks the retry loop to implement an OCC
  // loop to make such changes.
  StatusOr<google::storage::v2::Bucket> ModifyBucketAccessControl(
      Options const& options,
      storage::internal::GetBucketMetadataRequest const& request,
      BucketAclUpdater const& updater);

  using ObjectAccessControlList = google::protobuf::RepeatedPtrField<
      google::storage::v2::ObjectAccessControl>;
  using ObjectAclUpdater = std::function<StatusOr<ObjectAccessControlList>(
      ObjectAccessControlList acl)>;

  // REST has RPCs that change `ObjectAccessControl` resources atomically. gRPC
  // lacks such RPCs. This function hijacks the retry loop to implement an OCC
  // loop to make such changes.
  StatusOr<google::storage::v2::Object> ModifyObjectAccessControl(
      Options const& options,
      storage::internal::GetObjectMetadataRequest const& request,
      ObjectAclUpdater const& updater);

  using DefaultObjectAclUpdater =
      std::function<StatusOr<ObjectAccessControlList>(
          ObjectAccessControlList acl)>;

  // REST has RPCs that change `DefaultObjectAccessControl` resources
  // atomically. gRPC lacks such RPCs. This function hijacks the retry loop to
  // implement an OCC loop to make such changes.
  StatusOr<google::storage::v2::Bucket> ModifyDefaultAccessControl(
      Options const& options,
      storage::internal::GetBucketMetadataRequest const& request,
      DefaultObjectAclUpdater const& updater);

  Options options_;
  std::unique_ptr<google::cloud::BackgroundThreads> background_;
  std::shared_ptr<storage_internal::GrpcChannelRefresh> refresh_;
  std::shared_ptr<storage_internal::StorageStub> stub_;
  std::shared_ptr<google::cloud::internal::MinimalIamCredentialsStub> iam_stub_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_STUB_H
