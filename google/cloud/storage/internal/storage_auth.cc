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

#include "google/cloud/storage/internal/storage_auth.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

std::unique_ptr<StorageStub::ObjectMediaStream> StorageAuth::GetObjectMedia(
    std::unique_ptr<grpc::ClientContext> context,
    google::storage::v1::GetObjectMediaRequest const& request) {
  // TODO(coryan) - refactor to return a "always fails" stream
  auto status = auth_->ConfigureContext(*context);
  if (!status.ok()) return {};
  return child_->GetObjectMedia(std::move(context), request);
}

std::unique_ptr<StorageStub::InsertStream> StorageAuth::InsertObjectMedia(
    std::unique_ptr<grpc::ClientContext> context) {
  // TODO(coryan) - refactor to return a "always fails" stream
  auto status = auth_->ConfigureContext(*context);
  if (!status.ok()) return {};
  return child_->InsertObjectMedia(std::move(context));
}

Status StorageAuth::DeleteBucketAccessControl(
    grpc::ClientContext& context,
    google::storage::v1::DeleteBucketAccessControlRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->DeleteBucketAccessControl(context, request);
}

StatusOr<google::storage::v1::BucketAccessControl>
StorageAuth::GetBucketAccessControl(
    grpc::ClientContext& context,
    google::storage::v1::GetBucketAccessControlRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->GetBucketAccessControl(context, request);
}

StatusOr<google::storage::v1::BucketAccessControl>
StorageAuth::InsertBucketAccessControl(
    grpc::ClientContext& context,
    google::storage::v1::InsertBucketAccessControlRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->InsertBucketAccessControl(context, request);
}

StatusOr<google::storage::v1::ListBucketAccessControlsResponse>
StorageAuth::ListBucketAccessControls(
    grpc::ClientContext& context,
    google::storage::v1::ListBucketAccessControlsRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->ListBucketAccessControls(context, request);
}

StatusOr<google::storage::v1::BucketAccessControl>
StorageAuth::UpdateBucketAccessControl(
    grpc::ClientContext& context,
    google::storage::v1::UpdateBucketAccessControlRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->UpdateBucketAccessControl(context, request);
}

StatusOr<google::storage::v1::BucketAccessControl>
StorageAuth::PatchBucketAccessControl(
    grpc::ClientContext& context,
    google::storage::v1::PatchBucketAccessControlRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->PatchBucketAccessControl(context, request);
}

Status StorageAuth::DeleteBucket(
    grpc::ClientContext& context,
    google::storage::v1::DeleteBucketRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->DeleteBucket(context, request);
}

StatusOr<google::storage::v1::Bucket> StorageAuth::GetBucket(
    grpc::ClientContext& context,
    google::storage::v1::GetBucketRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->GetBucket(context, request);
}

StatusOr<google::storage::v1::Bucket> StorageAuth::InsertBucket(
    grpc::ClientContext& context,
    google::storage::v1::InsertBucketRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->InsertBucket(context, request);
}

StatusOr<google::storage::v1::ListBucketsResponse> StorageAuth::ListBuckets(
    grpc::ClientContext& context,
    google::storage::v1::ListBucketsRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->ListBuckets(context, request);
}

StatusOr<google::storage::v1::Bucket> StorageAuth::LockBucketRetentionPolicy(
    grpc::ClientContext& context,
    google::storage::v1::LockRetentionPolicyRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->LockBucketRetentionPolicy(context, request);
}

StatusOr<google::iam::v1::Policy> StorageAuth::GetBucketIamPolicy(
    grpc::ClientContext& context,
    google::storage::v1::GetIamPolicyRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->GetBucketIamPolicy(context, request);
}

StatusOr<google::iam::v1::Policy> StorageAuth::SetBucketIamPolicy(
    grpc::ClientContext& context,
    google::storage::v1::SetIamPolicyRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->SetBucketIamPolicy(context, request);
}

StatusOr<google::iam::v1::TestIamPermissionsResponse>
StorageAuth::TestBucketIamPermissions(
    grpc::ClientContext& context,
    google::storage::v1::TestIamPermissionsRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->TestBucketIamPermissions(context, request);
}

StatusOr<google::storage::v1::Bucket> StorageAuth::PatchBucket(
    grpc::ClientContext& context,
    google::storage::v1::PatchBucketRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->PatchBucket(context, request);
}

StatusOr<google::storage::v1::Bucket> StorageAuth::UpdateBucket(
    grpc::ClientContext& context,
    google::storage::v1::UpdateBucketRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->UpdateBucket(context, request);
}

Status StorageAuth::DeleteDefaultObjectAccessControl(
    grpc::ClientContext& context,
    google::storage::v1::DeleteDefaultObjectAccessControlRequest const&
        request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->DeleteDefaultObjectAccessControl(context, request);
}

StatusOr<google::storage::v1::ObjectAccessControl>
StorageAuth::GetDefaultObjectAccessControl(
    grpc::ClientContext& context,
    google::storage::v1::GetDefaultObjectAccessControlRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->GetDefaultObjectAccessControl(context, request);
}

StatusOr<google::storage::v1::ObjectAccessControl>
StorageAuth::InsertDefaultObjectAccessControl(
    grpc::ClientContext& context,
    google::storage::v1::InsertDefaultObjectAccessControlRequest const&
        request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->InsertDefaultObjectAccessControl(context, request);
}

StatusOr<google::storage::v1::ListObjectAccessControlsResponse>
StorageAuth::ListDefaultObjectAccessControls(
    grpc::ClientContext& context,
    google::storage::v1::ListDefaultObjectAccessControlsRequest const&
        request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->ListDefaultObjectAccessControls(context, request);
}

StatusOr<google::storage::v1::ObjectAccessControl>
StorageAuth::PatchDefaultObjectAccessControl(
    grpc::ClientContext& context,
    google::storage::v1::PatchDefaultObjectAccessControlRequest const&
        request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->PatchDefaultObjectAccessControl(context, request);
}

StatusOr<google::storage::v1::ObjectAccessControl>
StorageAuth::UpdateDefaultObjectAccessControl(
    grpc::ClientContext& context,
    google::storage::v1::UpdateDefaultObjectAccessControlRequest const&
        request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->UpdateDefaultObjectAccessControl(context, request);
}

Status StorageAuth::DeleteNotification(
    grpc::ClientContext& context,
    google::storage::v1::DeleteNotificationRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->DeleteNotification(context, request);
}

StatusOr<google::storage::v1::Notification> StorageAuth::GetNotification(
    grpc::ClientContext& context,
    google::storage::v1::GetNotificationRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->GetNotification(context, request);
}

StatusOr<google::storage::v1::Notification> StorageAuth::InsertNotification(
    grpc::ClientContext& context,
    google::storage::v1::InsertNotificationRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->InsertNotification(context, request);
}

StatusOr<google::storage::v1::ListNotificationsResponse>
StorageAuth::ListNotifications(
    grpc::ClientContext& context,
    google::storage::v1::ListNotificationsRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->ListNotifications(context, request);
}

Status StorageAuth::DeleteObjectAccessControl(
    grpc::ClientContext& context,
    google::storage::v1::DeleteObjectAccessControlRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->DeleteObjectAccessControl(context, request);
}

StatusOr<google::storage::v1::ObjectAccessControl>
StorageAuth::GetObjectAccessControl(
    grpc::ClientContext& context,
    google::storage::v1::GetObjectAccessControlRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->GetObjectAccessControl(context, request);
}

StatusOr<google::storage::v1::ObjectAccessControl>
StorageAuth::InsertObjectAccessControl(
    grpc::ClientContext& context,
    google::storage::v1::InsertObjectAccessControlRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->InsertObjectAccessControl(context, request);
}

StatusOr<google::storage::v1::ListObjectAccessControlsResponse>
StorageAuth::ListObjectAccessControls(
    grpc::ClientContext& context,
    google::storage::v1::ListObjectAccessControlsRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->ListObjectAccessControls(context, request);
}

StatusOr<google::storage::v1::ObjectAccessControl>
StorageAuth::PatchObjectAccessControl(
    grpc::ClientContext& context,
    google::storage::v1::PatchObjectAccessControlRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->PatchObjectAccessControl(context, request);
}

StatusOr<google::storage::v1::ObjectAccessControl>
StorageAuth::UpdateObjectAccessControl(
    grpc::ClientContext& context,
    google::storage::v1::UpdateObjectAccessControlRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->UpdateObjectAccessControl(context, request);
}

StatusOr<google::storage::v1::Object> StorageAuth::ComposeObject(
    grpc::ClientContext& context,
    google::storage::v1::ComposeObjectRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->ComposeObject(context, request);
}

StatusOr<google::storage::v1::Object> StorageAuth::CopyObject(
    grpc::ClientContext& context,
    google::storage::v1::CopyObjectRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->CopyObject(context, request);
}

Status StorageAuth::DeleteObject(
    grpc::ClientContext& context,
    google::storage::v1::DeleteObjectRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->DeleteObject(context, request);
}

StatusOr<google::storage::v1::Object> StorageAuth::GetObject(
    grpc::ClientContext& context,
    google::storage::v1::GetObjectRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->GetObject(context, request);
}

StatusOr<google::storage::v1::ListObjectsResponse> StorageAuth::ListObjects(
    grpc::ClientContext& context,
    google::storage::v1::ListObjectsRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->ListObjects(context, request);
}

StatusOr<google::storage::v1::RewriteResponse> StorageAuth::RewriteObject(
    grpc::ClientContext& context,
    google::storage::v1::RewriteObjectRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->RewriteObject(context, request);
}

StatusOr<google::storage::v1::StartResumableWriteResponse>
StorageAuth::StartResumableWrite(
    grpc::ClientContext& context,
    google::storage::v1::StartResumableWriteRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->StartResumableWrite(context, request);
}

StatusOr<google::storage::v1::QueryWriteStatusResponse>
StorageAuth::QueryWriteStatus(
    grpc::ClientContext& context,
    google::storage::v1::QueryWriteStatusRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->QueryWriteStatus(context, request);
}

StatusOr<google::storage::v1::Object> StorageAuth::PatchObject(
    grpc::ClientContext& context,
    google::storage::v1::PatchObjectRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->PatchObject(context, request);
}

StatusOr<google::storage::v1::Object> StorageAuth::UpdateObject(
    grpc::ClientContext& context,
    google::storage::v1::UpdateObjectRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->UpdateObject(context, request);
}

StatusOr<google::storage::v1::ServiceAccount> StorageAuth::GetServiceAccount(
    grpc::ClientContext& context,
    google::storage::v1::GetProjectServiceAccountRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->GetServiceAccount(context, request);
}

StatusOr<google::storage::v1::CreateHmacKeyResponse> StorageAuth::CreateHmacKey(
    grpc::ClientContext& context,
    google::storage::v1::CreateHmacKeyRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->CreateHmacKey(context, request);
}

Status StorageAuth::DeleteHmacKey(
    grpc::ClientContext& context,
    google::storage::v1::DeleteHmacKeyRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->DeleteHmacKey(context, request);
}

StatusOr<google::storage::v1::HmacKeyMetadata> StorageAuth::GetHmacKey(
    grpc::ClientContext& context,
    google::storage::v1::GetHmacKeyRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->GetHmacKey(context, request);
}

StatusOr<google::storage::v1::ListHmacKeysResponse> StorageAuth::ListHmacKeys(
    grpc::ClientContext& context,
    google::storage::v1::ListHmacKeysRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->ListHmacKeys(context, request);
}

StatusOr<google::storage::v1::HmacKeyMetadata> StorageAuth::UpdateHmacKey(
    grpc::ClientContext& context,
    google::storage::v1::UpdateHmacKeyRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->UpdateHmacKey(context, request);
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
