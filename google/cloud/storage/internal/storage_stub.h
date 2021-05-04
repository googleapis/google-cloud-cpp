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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_STORAGE_STUB_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_STORAGE_STUB_H

#include "google/cloud/storage/version.h"
#include "google/cloud/internal/streaming_read_rpc.h"
#include "google/cloud/internal/streaming_write_rpc.h"
#include "google/cloud/status_or.h"
#include <google/storage/v1/storage.pb.h>
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

class StorageStub {
 public:
  virtual ~StorageStub() = default;

  using ObjectMediaStream = google::cloud::internal::StreamingReadRpc<
      google::storage::v1::GetObjectMediaResponse>;
  virtual std::unique_ptr<ObjectMediaStream> GetObjectMedia(
      std::unique_ptr<grpc::ClientContext> context,
      google::storage::v1::GetObjectMediaRequest const& request) = 0;

  using InsertStream = google::cloud::internal::StreamingWriteRpc<
      google::storage::v1::InsertObjectRequest, google::storage::v1::Object>;
  virtual std::unique_ptr<InsertStream> InsertObjectMedia(
      std::unique_ptr<grpc::ClientContext> context) = 0;

  virtual Status DeleteBucketAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::DeleteBucketAccessControlRequest const& request) = 0;
  virtual StatusOr<google::storage::v1::BucketAccessControl>
  GetBucketAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::GetBucketAccessControlRequest const& request) = 0;
  virtual StatusOr<google::storage::v1::BucketAccessControl>
  InsertBucketAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::InsertBucketAccessControlRequest const& request) = 0;
  virtual StatusOr<google::storage::v1::ListBucketAccessControlsResponse>
  ListBucketAccessControls(
      grpc::ClientContext& context,
      google::storage::v1::ListBucketAccessControlsRequest const& request) = 0;
  virtual StatusOr<google::storage::v1::BucketAccessControl>
  UpdateBucketAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::UpdateBucketAccessControlRequest const& request) = 0;
  virtual StatusOr<google::storage::v1::BucketAccessControl>
  PatchBucketAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::PatchBucketAccessControlRequest const& request) = 0;
  virtual Status DeleteBucket(
      grpc::ClientContext& context,
      google::storage::v1::DeleteBucketRequest const& request) = 0;
  virtual StatusOr<google::storage::v1::Bucket> GetBucket(
      grpc::ClientContext& context,
      google::storage::v1::GetBucketRequest const& request) = 0;
  virtual StatusOr<google::storage::v1::Bucket> InsertBucket(
      grpc::ClientContext& context,
      google::storage::v1::InsertBucketRequest const& request) = 0;
  virtual StatusOr<google::storage::v1::ListBucketsResponse> ListBuckets(
      grpc::ClientContext& context,
      google::storage::v1::ListBucketsRequest const& request) = 0;
  virtual StatusOr<google::storage::v1::Bucket> LockBucketRetentionPolicy(
      grpc::ClientContext& context,
      google::storage::v1::LockRetentionPolicyRequest const& request) = 0;
  virtual StatusOr<google::iam::v1::Policy> GetBucketIamPolicy(
      grpc::ClientContext& context,
      google::storage::v1::GetIamPolicyRequest const& request) = 0;
  virtual StatusOr<google::iam::v1::Policy> SetBucketIamPolicy(
      grpc::ClientContext& context,
      google::storage::v1::SetIamPolicyRequest const& request) = 0;
  virtual StatusOr<google::iam::v1::TestIamPermissionsResponse>
  TestBucketIamPermissions(
      grpc::ClientContext& context,
      google::storage::v1::TestIamPermissionsRequest const& request) = 0;
  virtual StatusOr<google::storage::v1::Bucket> PatchBucket(
      grpc::ClientContext& context,
      google::storage::v1::PatchBucketRequest const& request) = 0;
  virtual StatusOr<google::storage::v1::Bucket> UpdateBucket(
      grpc::ClientContext& context,
      google::storage::v1::UpdateBucketRequest const& request) = 0;
  virtual Status DeleteDefaultObjectAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::DeleteDefaultObjectAccessControlRequest const&
          request) = 0;
  virtual StatusOr<google::storage::v1::ObjectAccessControl>
  GetDefaultObjectAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::GetDefaultObjectAccessControlRequest const&
          request) = 0;
  virtual StatusOr<google::storage::v1::ObjectAccessControl>
  InsertDefaultObjectAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::InsertDefaultObjectAccessControlRequest const&
          request) = 0;
  virtual StatusOr<google::storage::v1::ListObjectAccessControlsResponse>
  ListDefaultObjectAccessControls(
      grpc::ClientContext& context,
      google::storage::v1::ListDefaultObjectAccessControlsRequest const&
          request) = 0;
  virtual StatusOr<google::storage::v1::ObjectAccessControl>
  PatchDefaultObjectAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::PatchDefaultObjectAccessControlRequest const&
          request) = 0;
  virtual StatusOr<google::storage::v1::ObjectAccessControl>
  UpdateDefaultObjectAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::UpdateDefaultObjectAccessControlRequest const&
          request) = 0;
  virtual Status DeleteNotification(
      grpc::ClientContext& context,
      google::storage::v1::DeleteNotificationRequest const& request) = 0;
  virtual StatusOr<google::storage::v1::Notification> GetNotification(
      grpc::ClientContext& context,
      google::storage::v1::GetNotificationRequest const& request) = 0;
  virtual StatusOr<google::storage::v1::Notification> InsertNotification(
      grpc::ClientContext& context,
      google::storage::v1::InsertNotificationRequest const& request) = 0;
  virtual StatusOr<google::storage::v1::ListNotificationsResponse>
  ListNotifications(
      grpc::ClientContext& context,
      google::storage::v1::ListNotificationsRequest const& request) = 0;
  virtual Status DeleteObjectAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::DeleteObjectAccessControlRequest const& request) = 0;
  virtual StatusOr<google::storage::v1::ObjectAccessControl>
  GetObjectAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::GetObjectAccessControlRequest const& request) = 0;
  virtual StatusOr<google::storage::v1::ObjectAccessControl>
  InsertObjectAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::InsertObjectAccessControlRequest const& request) = 0;
  virtual StatusOr<google::storage::v1::ListObjectAccessControlsResponse>
  ListObjectAccessControls(
      grpc::ClientContext& context,
      google::storage::v1::ListObjectAccessControlsRequest const& request) = 0;
  virtual StatusOr<google::storage::v1::ObjectAccessControl>
  PatchObjectAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::PatchObjectAccessControlRequest const& request) = 0;
  virtual StatusOr<google::storage::v1::ObjectAccessControl>
  UpdateObjectAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::UpdateObjectAccessControlRequest const& request) = 0;
  virtual StatusOr<google::storage::v1::Object> ComposeObject(
      grpc::ClientContext& context,
      google::storage::v1::ComposeObjectRequest const& request) = 0;
  virtual StatusOr<google::storage::v1::Object> CopyObject(
      grpc::ClientContext& context,
      google::storage::v1::CopyObjectRequest const& request) = 0;
  virtual Status DeleteObject(
      grpc::ClientContext& context,
      google::storage::v1::DeleteObjectRequest const& request) = 0;
  virtual StatusOr<google::storage::v1::Object> GetObject(
      grpc::ClientContext& context,
      google::storage::v1::GetObjectRequest const& request) = 0;
  virtual StatusOr<google::storage::v1::ListObjectsResponse> ListObjects(
      grpc::ClientContext& context,
      google::storage::v1::ListObjectsRequest const& request) = 0;
  virtual StatusOr<google::storage::v1::RewriteResponse> RewriteObject(
      grpc::ClientContext& context,
      google::storage::v1::RewriteObjectRequest const& request) = 0;
  virtual StatusOr<google::storage::v1::StartResumableWriteResponse>
  StartResumableWrite(
      grpc::ClientContext& context,
      google::storage::v1::StartResumableWriteRequest const& request) = 0;
  virtual StatusOr<google::storage::v1::QueryWriteStatusResponse>
  QueryWriteStatus(
      grpc::ClientContext& context,
      google::storage::v1::QueryWriteStatusRequest const& request) = 0;
  virtual StatusOr<google::storage::v1::Object> PatchObject(
      grpc::ClientContext& context,
      google::storage::v1::PatchObjectRequest const& request) = 0;
  virtual StatusOr<google::storage::v1::Object> UpdateObject(
      grpc::ClientContext& context,
      google::storage::v1::UpdateObjectRequest const& request) = 0;
  virtual StatusOr<google::storage::v1::ServiceAccount> GetServiceAccount(
      grpc::ClientContext& context,
      google::storage::v1::GetProjectServiceAccountRequest const& request) = 0;
  virtual StatusOr<google::storage::v1::CreateHmacKeyResponse> CreateHmacKey(
      grpc::ClientContext& context,
      google::storage::v1::CreateHmacKeyRequest const& request) = 0;
  virtual Status DeleteHmacKey(
      grpc::ClientContext& context,
      google::storage::v1::DeleteHmacKeyRequest const& request) = 0;
  virtual StatusOr<google::storage::v1::HmacKeyMetadata> GetHmacKey(
      grpc::ClientContext& context,
      google::storage::v1::GetHmacKeyRequest const& request) = 0;
  virtual StatusOr<google::storage::v1::ListHmacKeysResponse> ListHmacKeys(
      grpc::ClientContext& context,
      google::storage::v1::ListHmacKeysRequest const& request) = 0;
  virtual StatusOr<google::storage::v1::HmacKeyMetadata> UpdateHmacKey(
      grpc::ClientContext& context,
      google::storage::v1::UpdateHmacKeyRequest const& request) = 0;
};

std::shared_ptr<StorageStub> MakeDefaultStorageStub(
    std::shared_ptr<grpc::Channel> channel);

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_STORAGE_STUB_H
