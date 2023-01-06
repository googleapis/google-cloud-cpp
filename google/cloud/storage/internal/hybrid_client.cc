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

#include "google/cloud/storage/internal/hybrid_client.h"
#include "google/cloud/storage/internal/grpc_client.h"
#include "google/cloud/storage/internal/rest_client.h"
#include "absl/strings/match.h"

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::shared_ptr<storage::internal::RawClient> HybridClient::Create(
    Options const& options) {
  return std::shared_ptr<RawClient>(new HybridClient(options));
}

HybridClient::HybridClient(Options const& options)
    : grpc_(GrpcClient::Create(DefaultOptionsGrpc(options))),
      rest_(storage::internal::RestClient::Create(
          storage::internal::DefaultOptionsWithCredentials(options))) {}

storage::ClientOptions const& HybridClient::client_options() const {
  return rest_->client_options();
}

Options HybridClient::options() const { return rest_->options(); }

StatusOr<storage::internal::ListBucketsResponse> HybridClient::ListBuckets(
    storage::internal::ListBucketsRequest const& request) {
  return rest_->ListBuckets(request);
}

StatusOr<storage::BucketMetadata> HybridClient::CreateBucket(
    storage::internal::CreateBucketRequest const& request) {
  return rest_->CreateBucket(request);
}

StatusOr<storage::BucketMetadata> HybridClient::GetBucketMetadata(
    storage::internal::GetBucketMetadataRequest const& request) {
  return rest_->GetBucketMetadata(request);
}

StatusOr<storage::internal::EmptyResponse> HybridClient::DeleteBucket(
    storage::internal::DeleteBucketRequest const& request) {
  return rest_->DeleteBucket(request);
}

StatusOr<storage::BucketMetadata> HybridClient::UpdateBucket(
    storage::internal::UpdateBucketRequest const& request) {
  return rest_->UpdateBucket(request);
}

StatusOr<storage::BucketMetadata> HybridClient::PatchBucket(
    storage::internal::PatchBucketRequest const& request) {
  return rest_->PatchBucket(request);
}

StatusOr<storage::NativeIamPolicy> HybridClient::GetNativeBucketIamPolicy(
    storage::internal::GetBucketIamPolicyRequest const& request) {
  return rest_->GetNativeBucketIamPolicy(request);
}

StatusOr<storage::NativeIamPolicy> HybridClient::SetNativeBucketIamPolicy(
    storage::internal::SetNativeBucketIamPolicyRequest const& request) {
  return rest_->SetNativeBucketIamPolicy(request);
}

StatusOr<storage::internal::TestBucketIamPermissionsResponse>
HybridClient::TestBucketIamPermissions(
    storage::internal::TestBucketIamPermissionsRequest const& request) {
  return rest_->TestBucketIamPermissions(request);
}

StatusOr<storage::BucketMetadata> HybridClient::LockBucketRetentionPolicy(
    storage::internal::LockBucketRetentionPolicyRequest const& request) {
  return rest_->LockBucketRetentionPolicy(request);
}

StatusOr<storage::ObjectMetadata> HybridClient::InsertObjectMedia(
    storage::internal::InsertObjectMediaRequest const& request) {
  return grpc_->InsertObjectMedia(request);
}

StatusOr<storage::ObjectMetadata> HybridClient::CopyObject(
    storage::internal::CopyObjectRequest const& request) {
  return rest_->CopyObject(request);
}

StatusOr<storage::ObjectMetadata> HybridClient::GetObjectMetadata(
    storage::internal::GetObjectMetadataRequest const& request) {
  return rest_->GetObjectMetadata(request);
}

StatusOr<std::unique_ptr<storage::internal::ObjectReadSource>>
HybridClient::ReadObject(
    storage::internal::ReadObjectRangeRequest const& request) {
  return grpc_->ReadObject(request);
}

StatusOr<storage::internal::ListObjectsResponse> HybridClient::ListObjects(
    storage::internal::ListObjectsRequest const& request) {
  return rest_->ListObjects(request);
}

StatusOr<storage::internal::EmptyResponse> HybridClient::DeleteObject(
    storage::internal::DeleteObjectRequest const& request) {
  return rest_->DeleteObject(request);
}

StatusOr<storage::ObjectMetadata> HybridClient::UpdateObject(
    storage::internal::UpdateObjectRequest const& request) {
  return rest_->UpdateObject(request);
}

StatusOr<storage::ObjectMetadata> HybridClient::PatchObject(
    storage::internal::PatchObjectRequest const& request) {
  return rest_->PatchObject(request);
}

StatusOr<storage::ObjectMetadata> HybridClient::ComposeObject(
    storage::internal::ComposeObjectRequest const& request) {
  return rest_->ComposeObject(request);
}

StatusOr<storage::internal::RewriteObjectResponse> HybridClient::RewriteObject(
    storage::internal::RewriteObjectRequest const& request) {
  return rest_->RewriteObject(request);
}

StatusOr<storage::internal::CreateResumableUploadResponse>
HybridClient::CreateResumableUpload(
    storage::internal::ResumableUploadRequest const& request) {
  return grpc_->CreateResumableUpload(request);
}

StatusOr<storage::internal::QueryResumableUploadResponse>
HybridClient::QueryResumableUpload(
    storage::internal::QueryResumableUploadRequest const& request) {
  return grpc_->QueryResumableUpload(request);
}

StatusOr<storage::internal::EmptyResponse> HybridClient::DeleteResumableUpload(
    storage::internal::DeleteResumableUploadRequest const& request) {
  if (absl::StartsWith(request.upload_session_url(), "https://")) {
    return rest_->DeleteResumableUpload(request);
  }
  return grpc_->DeleteResumableUpload(request);
}

StatusOr<storage::internal::QueryResumableUploadResponse>
HybridClient::UploadChunk(
    storage::internal::UploadChunkRequest const& request) {
  return grpc_->UploadChunk(request);
}

StatusOr<storage::internal::ListBucketAclResponse> HybridClient::ListBucketAcl(
    storage::internal::ListBucketAclRequest const& request) {
  return rest_->ListBucketAcl(request);
}

StatusOr<storage::BucketAccessControl> HybridClient::CreateBucketAcl(
    storage::internal::CreateBucketAclRequest const& request) {
  return rest_->CreateBucketAcl(request);
}

StatusOr<storage::internal::EmptyResponse> HybridClient::DeleteBucketAcl(
    storage::internal::DeleteBucketAclRequest const& request) {
  return rest_->DeleteBucketAcl(request);
}

StatusOr<storage::BucketAccessControl> HybridClient::GetBucketAcl(
    storage::internal::GetBucketAclRequest const& request) {
  return rest_->GetBucketAcl(request);
}

StatusOr<storage::BucketAccessControl> HybridClient::UpdateBucketAcl(
    storage::internal::UpdateBucketAclRequest const& request) {
  return rest_->UpdateBucketAcl(request);
}

StatusOr<storage::BucketAccessControl> HybridClient::PatchBucketAcl(
    storage::internal::PatchBucketAclRequest const& request) {
  return rest_->PatchBucketAcl(request);
}

StatusOr<storage::internal::ListObjectAclResponse> HybridClient::ListObjectAcl(
    storage::internal::ListObjectAclRequest const& request) {
  return rest_->ListObjectAcl(request);
}

StatusOr<storage::ObjectAccessControl> HybridClient::CreateObjectAcl(
    storage::internal::CreateObjectAclRequest const& request) {
  return rest_->CreateObjectAcl(request);
}

StatusOr<storage::internal::EmptyResponse> HybridClient::DeleteObjectAcl(
    storage::internal::DeleteObjectAclRequest const& request) {
  return rest_->DeleteObjectAcl(request);
}

StatusOr<storage::ObjectAccessControl> HybridClient::GetObjectAcl(
    storage::internal::GetObjectAclRequest const& request) {
  return rest_->GetObjectAcl(request);
}

StatusOr<storage::ObjectAccessControl> HybridClient::UpdateObjectAcl(
    storage::internal::UpdateObjectAclRequest const& request) {
  return rest_->UpdateObjectAcl(request);
}

StatusOr<storage::ObjectAccessControl> HybridClient::PatchObjectAcl(
    storage::internal::PatchObjectAclRequest const& request) {
  return rest_->PatchObjectAcl(request);
}

StatusOr<storage::internal::ListDefaultObjectAclResponse>
HybridClient::ListDefaultObjectAcl(
    storage::internal::ListDefaultObjectAclRequest const& request) {
  return rest_->ListDefaultObjectAcl(request);
}

StatusOr<storage::ObjectAccessControl> HybridClient::CreateDefaultObjectAcl(
    storage::internal::CreateDefaultObjectAclRequest const& request) {
  return rest_->CreateDefaultObjectAcl(request);
}

StatusOr<storage::internal::EmptyResponse> HybridClient::DeleteDefaultObjectAcl(
    storage::internal::DeleteDefaultObjectAclRequest const& request) {
  return rest_->DeleteDefaultObjectAcl(request);
}

StatusOr<storage::ObjectAccessControl> HybridClient::GetDefaultObjectAcl(
    storage::internal::GetDefaultObjectAclRequest const& request) {
  return rest_->GetDefaultObjectAcl(request);
}

StatusOr<storage::ObjectAccessControl> HybridClient::UpdateDefaultObjectAcl(
    storage::internal::UpdateDefaultObjectAclRequest const& request) {
  return rest_->UpdateDefaultObjectAcl(request);
}

StatusOr<storage::ObjectAccessControl> HybridClient::PatchDefaultObjectAcl(
    storage::internal::PatchDefaultObjectAclRequest const& request) {
  return rest_->PatchDefaultObjectAcl(request);
}

StatusOr<storage::ServiceAccount> HybridClient::GetServiceAccount(
    storage::internal::GetProjectServiceAccountRequest const& request) {
  return rest_->GetServiceAccount(request);
}

StatusOr<storage::internal::ListHmacKeysResponse> HybridClient::ListHmacKeys(
    storage::internal::ListHmacKeysRequest const& request) {
  return rest_->ListHmacKeys(request);
}

StatusOr<storage::internal::CreateHmacKeyResponse> HybridClient::CreateHmacKey(
    storage::internal::CreateHmacKeyRequest const& request) {
  return rest_->CreateHmacKey(request);
}

StatusOr<storage::internal::EmptyResponse> HybridClient::DeleteHmacKey(
    storage::internal::DeleteHmacKeyRequest const& request) {
  return rest_->DeleteHmacKey(request);
}

StatusOr<storage::HmacKeyMetadata> HybridClient::GetHmacKey(
    storage::internal::GetHmacKeyRequest const& request) {
  return rest_->GetHmacKey(request);
}

StatusOr<storage::HmacKeyMetadata> HybridClient::UpdateHmacKey(
    storage::internal::UpdateHmacKeyRequest const& request) {
  return rest_->UpdateHmacKey(request);
}

StatusOr<storage::internal::SignBlobResponse> HybridClient::SignBlob(
    storage::internal::SignBlobRequest const& request) {
  return rest_->SignBlob(request);
}

StatusOr<storage::internal::ListNotificationsResponse>
HybridClient::ListNotifications(
    storage::internal::ListNotificationsRequest const& request) {
  return rest_->ListNotifications(request);
}

StatusOr<storage::NotificationMetadata> HybridClient::CreateNotification(
    storage::internal::CreateNotificationRequest const& request) {
  return rest_->CreateNotification(request);
}

StatusOr<storage::NotificationMetadata> HybridClient::GetNotification(
    storage::internal::GetNotificationRequest const& request) {
  return rest_->GetNotification(request);
}

StatusOr<storage::internal::EmptyResponse> HybridClient::DeleteNotification(
    storage::internal::DeleteNotificationRequest const& request) {
  return rest_->DeleteNotification(request);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
