// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_CLIENT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_CLIENT_H

#include "google/cloud/storage/internal/raw_client.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/background_threads.h"
#include "google/cloud/internal/streaming_write_rpc.h"
#include <google/storage/v1/storage.pb.h>
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

/**
 * The default options for gRPC.
 *
 * This adds some additional defaults to the options for REST.
 */
Options DefaultOptionsGrpc(Options = {});

class StorageStub;

class GrpcClient : public RawClient,
                   public std::enable_shared_from_this<GrpcClient> {
 public:
  static std::shared_ptr<GrpcClient> Create(Options const& opts);

  // This is used to create a client from a mocked StorageStub.
  static std::shared_ptr<GrpcClient> CreateMock(
      std::shared_ptr<StorageStub> stub, Options opts = {});

  ~GrpcClient() override = default;

  //@{
  /// @name Implement the ResumableSession operations.
  // Note that these member functions are not inherited from RawClient, they are
  // called only by `GrpcResumableUploadSession`, because the retry loop for
  // them is very different from the standard retry loop. Also note that these
  // are virtual functions only because we need to override them in the unit
  // tests.
  using InsertStream = ::google::cloud::internal::StreamingWriteRpc<
      google::storage::v1::InsertObjectRequest, google::storage::v1::Object>;
  virtual std::unique_ptr<InsertStream> CreateUploadWriter(
      std::unique_ptr<grpc::ClientContext> context);
  virtual StatusOr<ResumableUploadResponse> QueryResumableUpload(
      QueryResumableUploadRequest const&);
  //@}

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
  StatusOr<std::unique_ptr<ResumableUploadSession>> RestoreResumableSession(
      std::string const& upload_url) override;
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

  static google::storage::v1::Object::CustomerEncryption ToProto(
      CustomerEncryption rhs);
  static CustomerEncryption FromProto(
      google::storage::v1::Object::CustomerEncryption rhs);

  static ObjectMetadata FromProto(google::storage::v1::Object object);

  static google::storage::v1::ObjectAccessControl ToProto(
      ObjectAccessControl const& acl);
  static ObjectAccessControl FromProto(
      google::storage::v1::ObjectAccessControl acl);

  static google::storage::v1::Owner ToProto(Owner);
  static Owner FromProto(google::storage::v1::Owner);

  static google::storage::v1::CommonEnums::Projection ToProto(
      Projection const& p);
  static google::storage::v1::CommonEnums::PredefinedObjectAcl ToProtoObject(
      PredefinedAcl const& acl);

  static StatusOr<google::storage::v1::InsertObjectRequest> ToProto(
      InsertObjectMediaRequest const& request);
  static google::storage::v1::StartResumableWriteRequest ToProto(
      ResumableUploadRequest const& request);
  static google::storage::v1::QueryWriteStatusRequest ToProto(
      QueryResumableUploadRequest const& request);

  static google::storage::v1::GetObjectMediaRequest ToProto(
      ReadObjectRangeRequest const& request);

  static std::string Crc32cFromProto(google::protobuf::UInt32Value const&);
  static StatusOr<std::uint32_t> Crc32cToProto(std::string const&);
  static std::string MD5FromProto(std::string const&);
  static StatusOr<std::string> MD5ToProto(std::string const&);
  static std::string ComputeMD5Hash(std::string const& payload);

 protected:
  explicit GrpcClient(Options const& opts);
  explicit GrpcClient(std::shared_ptr<StorageStub> stub, Options const& opts);

 private:
  ClientOptions backwards_compatibility_options_;
  std::unique_ptr<google::cloud::BackgroundThreads> background_;
  std::shared_ptr<StorageStub> stub_;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_CLIENT_H
