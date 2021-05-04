// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_STORAGE_AUTH_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_STORAGE_AUTH_H

#include "google/cloud/storage/internal/storage_stub.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/internal/unified_grpc_credentials.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

class StorageAuth : public StorageStub {
 public:
  explicit StorageAuth(
      std::shared_ptr<google::cloud::internal::GrpcAuthenticationStrategy> auth,
      std::shared_ptr<StorageStub> child)
      : auth_(std::move(auth)), child_(std::move(child)) {}
  ~StorageAuth() override = default;

  std::unique_ptr<ObjectMediaStream> GetObjectMedia(
      std::unique_ptr<grpc::ClientContext> context,
      google::storage::v1::GetObjectMediaRequest const& request) override;

  std::unique_ptr<InsertStream> InsertObjectMedia(
      std::unique_ptr<grpc::ClientContext> context) override;

  Status DeleteBucketAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::DeleteBucketAccessControlRequest const& request)
      override;
  StatusOr<google::storage::v1::BucketAccessControl> GetBucketAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::GetBucketAccessControlRequest const& request)
      override;
  StatusOr<google::storage::v1::BucketAccessControl> InsertBucketAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::InsertBucketAccessControlRequest const& request)
      override;
  StatusOr<google::storage::v1::ListBucketAccessControlsResponse>
  ListBucketAccessControls(
      grpc::ClientContext& context,
      google::storage::v1::ListBucketAccessControlsRequest const& request)
      override;
  StatusOr<google::storage::v1::BucketAccessControl> UpdateBucketAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::UpdateBucketAccessControlRequest const& request)
      override;
  StatusOr<google::storage::v1::BucketAccessControl> PatchBucketAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::PatchBucketAccessControlRequest const& request)
      override;
  Status DeleteBucket(
      grpc::ClientContext& context,
      google::storage::v1::DeleteBucketRequest const& request) override;
  StatusOr<google::storage::v1::Bucket> GetBucket(
      grpc::ClientContext& context,
      google::storage::v1::GetBucketRequest const& request) override;
  StatusOr<google::storage::v1::Bucket> InsertBucket(
      grpc::ClientContext& context,
      google::storage::v1::InsertBucketRequest const& request) override;
  StatusOr<google::storage::v1::ListBucketsResponse> ListBuckets(
      grpc::ClientContext& context,
      google::storage::v1::ListBucketsRequest const& request) override;
  StatusOr<google::storage::v1::Bucket> LockBucketRetentionPolicy(
      grpc::ClientContext& context,
      google::storage::v1::LockRetentionPolicyRequest const& request) override;
  StatusOr<google::iam::v1::Policy> GetBucketIamPolicy(
      grpc::ClientContext& context,
      google::storage::v1::GetIamPolicyRequest const& request) override;
  StatusOr<google::iam::v1::Policy> SetBucketIamPolicy(
      grpc::ClientContext& context,
      google::storage::v1::SetIamPolicyRequest const& request) override;
  StatusOr<google::iam::v1::TestIamPermissionsResponse>
  TestBucketIamPermissions(
      grpc::ClientContext& context,
      google::storage::v1::TestIamPermissionsRequest const& request) override;
  StatusOr<google::storage::v1::Bucket> PatchBucket(
      grpc::ClientContext& context,
      google::storage::v1::PatchBucketRequest const& request) override;
  StatusOr<google::storage::v1::Bucket> UpdateBucket(
      grpc::ClientContext& context,
      google::storage::v1::UpdateBucketRequest const& request) override;
  Status DeleteDefaultObjectAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::DeleteDefaultObjectAccessControlRequest const&
          request) override;
  StatusOr<google::storage::v1::ObjectAccessControl>
  GetDefaultObjectAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::GetDefaultObjectAccessControlRequest const& request)
      override;
  StatusOr<google::storage::v1::ObjectAccessControl>
  InsertDefaultObjectAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::InsertDefaultObjectAccessControlRequest const&
          request) override;
  StatusOr<google::storage::v1::ListObjectAccessControlsResponse>
  ListDefaultObjectAccessControls(
      grpc::ClientContext& context,
      google::storage::v1::ListDefaultObjectAccessControlsRequest const&
          request) override;
  StatusOr<google::storage::v1::ObjectAccessControl>
  PatchDefaultObjectAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::PatchDefaultObjectAccessControlRequest const&
          request) override;
  StatusOr<google::storage::v1::ObjectAccessControl>
  UpdateDefaultObjectAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::UpdateDefaultObjectAccessControlRequest const&
          request) override;
  Status DeleteNotification(
      grpc::ClientContext& context,
      google::storage::v1::DeleteNotificationRequest const& request) override;
  StatusOr<google::storage::v1::Notification> GetNotification(
      grpc::ClientContext& context,
      google::storage::v1::GetNotificationRequest const& request) override;
  StatusOr<google::storage::v1::Notification> InsertNotification(
      grpc::ClientContext& context,
      google::storage::v1::InsertNotificationRequest const& request) override;
  StatusOr<google::storage::v1::ListNotificationsResponse> ListNotifications(
      grpc::ClientContext& context,
      google::storage::v1::ListNotificationsRequest const& request) override;
  Status DeleteObjectAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::DeleteObjectAccessControlRequest const& request)
      override;
  StatusOr<google::storage::v1::ObjectAccessControl> GetObjectAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::GetObjectAccessControlRequest const& request)
      override;
  StatusOr<google::storage::v1::ObjectAccessControl> InsertObjectAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::InsertObjectAccessControlRequest const& request)
      override;
  StatusOr<google::storage::v1::ListObjectAccessControlsResponse>
  ListObjectAccessControls(
      grpc::ClientContext& context,
      google::storage::v1::ListObjectAccessControlsRequest const& request)
      override;
  StatusOr<google::storage::v1::ObjectAccessControl> PatchObjectAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::PatchObjectAccessControlRequest const& request)
      override;
  StatusOr<google::storage::v1::ObjectAccessControl> UpdateObjectAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::UpdateObjectAccessControlRequest const& request)
      override;
  StatusOr<google::storage::v1::Object> ComposeObject(
      grpc::ClientContext& context,
      google::storage::v1::ComposeObjectRequest const& request) override;
  StatusOr<google::storage::v1::Object> CopyObject(
      grpc::ClientContext& context,
      google::storage::v1::CopyObjectRequest const& request) override;
  Status DeleteObject(
      grpc::ClientContext& context,
      google::storage::v1::DeleteObjectRequest const& request) override;
  StatusOr<google::storage::v1::Object> GetObject(
      grpc::ClientContext& context,
      google::storage::v1::GetObjectRequest const& request) override;
  StatusOr<google::storage::v1::ListObjectsResponse> ListObjects(
      grpc::ClientContext& context,
      google::storage::v1::ListObjectsRequest const& request) override;
  StatusOr<google::storage::v1::RewriteResponse> RewriteObject(
      grpc::ClientContext& context,
      google::storage::v1::RewriteObjectRequest const& request) override;
  StatusOr<google::storage::v1::StartResumableWriteResponse>
  StartResumableWrite(
      grpc::ClientContext& context,
      google::storage::v1::StartResumableWriteRequest const& request) override;
  StatusOr<google::storage::v1::QueryWriteStatusResponse> QueryWriteStatus(
      grpc::ClientContext& context,
      google::storage::v1::QueryWriteStatusRequest const& request) override;
  StatusOr<google::storage::v1::Object> PatchObject(
      grpc::ClientContext& context,
      google::storage::v1::PatchObjectRequest const& request) override;
  StatusOr<google::storage::v1::Object> UpdateObject(
      grpc::ClientContext& context,
      google::storage::v1::UpdateObjectRequest const& request) override;
  StatusOr<google::storage::v1::ServiceAccount> GetServiceAccount(
      grpc::ClientContext& context,
      google::storage::v1::GetProjectServiceAccountRequest const& request)
      override;
  StatusOr<google::storage::v1::CreateHmacKeyResponse> CreateHmacKey(
      grpc::ClientContext& context,
      google::storage::v1::CreateHmacKeyRequest const& request) override;
  Status DeleteHmacKey(
      grpc::ClientContext& context,
      google::storage::v1::DeleteHmacKeyRequest const& request) override;
  StatusOr<google::storage::v1::HmacKeyMetadata> GetHmacKey(
      grpc::ClientContext& context,
      google::storage::v1::GetHmacKeyRequest const& request) override;
  StatusOr<google::storage::v1::ListHmacKeysResponse> ListHmacKeys(
      grpc::ClientContext& context,
      google::storage::v1::ListHmacKeysRequest const& request) override;
  StatusOr<google::storage::v1::HmacKeyMetadata> UpdateHmacKey(
      grpc::ClientContext& context,
      google::storage::v1::UpdateHmacKeyRequest const& request) override;

 private:
  std::shared_ptr<google::cloud::internal::GrpcAuthenticationStrategy> auth_;
  std::shared_ptr<StorageStub> child_;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_STORAGE_AUTH_H
