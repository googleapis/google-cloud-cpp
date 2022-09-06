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
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

std::shared_ptr<RawClient> HybridClient::Create(Options const& options) {
  return std::shared_ptr<RawClient>(new HybridClient(options));
}

HybridClient::HybridClient(Options const& options)
    : grpc_(GrpcClient::Create(DefaultOptionsGrpc(options))),
      rest_(RestClient::Create(DefaultOptionsWithCredentials(options))) {}

ClientOptions const& HybridClient::client_options() const {
  return rest_->client_options();
}

Options HybridClient::options() const { return rest_->options(); }

StatusOr<ListBucketsResponse> HybridClient::ListBuckets(
    ListBucketsRequest const& request) {
  return rest_->ListBuckets(request);
}

StatusOr<BucketMetadata> HybridClient::CreateBucket(
    CreateBucketRequest const& request) {
  return rest_->CreateBucket(request);
}

StatusOr<BucketMetadata> HybridClient::GetBucketMetadata(
    GetBucketMetadataRequest const& request) {
  return rest_->GetBucketMetadata(request);
}

StatusOr<EmptyResponse> HybridClient::DeleteBucket(
    DeleteBucketRequest const& request) {
  return rest_->DeleteBucket(request);
}

StatusOr<BucketMetadata> HybridClient::UpdateBucket(
    UpdateBucketRequest const& request) {
  return rest_->UpdateBucket(request);
}

StatusOr<BucketMetadata> HybridClient::PatchBucket(
    PatchBucketRequest const& request) {
  return rest_->PatchBucket(request);
}

StatusOr<NativeIamPolicy> HybridClient::GetNativeBucketIamPolicy(
    GetBucketIamPolicyRequest const& request) {
  return rest_->GetNativeBucketIamPolicy(request);
}

StatusOr<NativeIamPolicy> HybridClient::SetNativeBucketIamPolicy(
    SetNativeBucketIamPolicyRequest const& request) {
  return rest_->SetNativeBucketIamPolicy(request);
}

StatusOr<TestBucketIamPermissionsResponse>
HybridClient::TestBucketIamPermissions(
    TestBucketIamPermissionsRequest const& request) {
  return rest_->TestBucketIamPermissions(request);
}

StatusOr<BucketMetadata> HybridClient::LockBucketRetentionPolicy(
    LockBucketRetentionPolicyRequest const& request) {
  return rest_->LockBucketRetentionPolicy(request);
}

StatusOr<ObjectMetadata> HybridClient::InsertObjectMedia(
    InsertObjectMediaRequest const& request) {
  return grpc_->InsertObjectMedia(request);
}

StatusOr<ObjectMetadata> HybridClient::CopyObject(
    CopyObjectRequest const& request) {
  return rest_->CopyObject(request);
}

StatusOr<ObjectMetadata> HybridClient::GetObjectMetadata(
    GetObjectMetadataRequest const& request) {
  return rest_->GetObjectMetadata(request);
}

StatusOr<std::unique_ptr<ObjectReadSource>> HybridClient::ReadObject(
    ReadObjectRangeRequest const& request) {
  return grpc_->ReadObject(request);
}

StatusOr<ListObjectsResponse> HybridClient::ListObjects(
    ListObjectsRequest const& request) {
  return rest_->ListObjects(request);
}

StatusOr<EmptyResponse> HybridClient::DeleteObject(
    DeleteObjectRequest const& request) {
  return rest_->DeleteObject(request);
}

StatusOr<ObjectMetadata> HybridClient::UpdateObject(
    UpdateObjectRequest const& request) {
  return rest_->UpdateObject(request);
}

StatusOr<ObjectMetadata> HybridClient::PatchObject(
    PatchObjectRequest const& request) {
  return rest_->PatchObject(request);
}

StatusOr<ObjectMetadata> HybridClient::ComposeObject(
    ComposeObjectRequest const& request) {
  return rest_->ComposeObject(request);
}

StatusOr<RewriteObjectResponse> HybridClient::RewriteObject(
    RewriteObjectRequest const& request) {
  return rest_->RewriteObject(request);
}

StatusOr<CreateResumableUploadResponse> HybridClient::CreateResumableUpload(
    ResumableUploadRequest const& request) {
  return grpc_->CreateResumableUpload(request);
}

StatusOr<QueryResumableUploadResponse> HybridClient::QueryResumableUpload(
    QueryResumableUploadRequest const& request) {
  return grpc_->QueryResumableUpload(request);
}

StatusOr<EmptyResponse> HybridClient::DeleteResumableUpload(
    DeleteResumableUploadRequest const& request) {
  if (absl::StartsWith(request.upload_session_url(), "https://")) {
    return rest_->DeleteResumableUpload(request);
  }
  return grpc_->DeleteResumableUpload(request);
}

StatusOr<QueryResumableUploadResponse> HybridClient::UploadChunk(
    UploadChunkRequest const& request) {
  return grpc_->UploadChunk(request);
}

StatusOr<ListBucketAclResponse> HybridClient::ListBucketAcl(
    ListBucketAclRequest const& request) {
  return rest_->ListBucketAcl(request);
}

StatusOr<BucketAccessControl> HybridClient::CreateBucketAcl(
    CreateBucketAclRequest const& request) {
  return rest_->CreateBucketAcl(request);
}

StatusOr<EmptyResponse> HybridClient::DeleteBucketAcl(
    DeleteBucketAclRequest const& request) {
  return rest_->DeleteBucketAcl(request);
}

StatusOr<BucketAccessControl> HybridClient::GetBucketAcl(
    GetBucketAclRequest const& request) {
  return rest_->GetBucketAcl(request);
}

StatusOr<BucketAccessControl> HybridClient::UpdateBucketAcl(
    UpdateBucketAclRequest const& request) {
  return rest_->UpdateBucketAcl(request);
}

StatusOr<BucketAccessControl> HybridClient::PatchBucketAcl(
    PatchBucketAclRequest const& request) {
  return rest_->PatchBucketAcl(request);
}

StatusOr<ListObjectAclResponse> HybridClient::ListObjectAcl(
    ListObjectAclRequest const& request) {
  return rest_->ListObjectAcl(request);
}

StatusOr<ObjectAccessControl> HybridClient::CreateObjectAcl(
    CreateObjectAclRequest const& request) {
  return rest_->CreateObjectAcl(request);
}

StatusOr<EmptyResponse> HybridClient::DeleteObjectAcl(
    DeleteObjectAclRequest const& request) {
  return rest_->DeleteObjectAcl(request);
}

StatusOr<ObjectAccessControl> HybridClient::GetObjectAcl(
    GetObjectAclRequest const& request) {
  return rest_->GetObjectAcl(request);
}

StatusOr<ObjectAccessControl> HybridClient::UpdateObjectAcl(
    UpdateObjectAclRequest const& request) {
  return rest_->UpdateObjectAcl(request);
}

StatusOr<ObjectAccessControl> HybridClient::PatchObjectAcl(
    PatchObjectAclRequest const& request) {
  return rest_->PatchObjectAcl(request);
}

StatusOr<ListDefaultObjectAclResponse> HybridClient::ListDefaultObjectAcl(
    ListDefaultObjectAclRequest const& request) {
  return rest_->ListDefaultObjectAcl(request);
}

StatusOr<ObjectAccessControl> HybridClient::CreateDefaultObjectAcl(
    CreateDefaultObjectAclRequest const& request) {
  return rest_->CreateDefaultObjectAcl(request);
}

StatusOr<EmptyResponse> HybridClient::DeleteDefaultObjectAcl(
    DeleteDefaultObjectAclRequest const& request) {
  return rest_->DeleteDefaultObjectAcl(request);
}

StatusOr<ObjectAccessControl> HybridClient::GetDefaultObjectAcl(
    GetDefaultObjectAclRequest const& request) {
  return rest_->GetDefaultObjectAcl(request);
}

StatusOr<ObjectAccessControl> HybridClient::UpdateDefaultObjectAcl(
    UpdateDefaultObjectAclRequest const& request) {
  return rest_->UpdateDefaultObjectAcl(request);
}

StatusOr<ObjectAccessControl> HybridClient::PatchDefaultObjectAcl(
    PatchDefaultObjectAclRequest const& request) {
  return rest_->PatchDefaultObjectAcl(request);
}

StatusOr<ServiceAccount> HybridClient::GetServiceAccount(
    GetProjectServiceAccountRequest const& request) {
  return rest_->GetServiceAccount(request);
}

StatusOr<ListHmacKeysResponse> HybridClient::ListHmacKeys(
    ListHmacKeysRequest const& request) {
  return rest_->ListHmacKeys(request);
}

StatusOr<CreateHmacKeyResponse> HybridClient::CreateHmacKey(
    CreateHmacKeyRequest const& request) {
  return rest_->CreateHmacKey(request);
}

StatusOr<EmptyResponse> HybridClient::DeleteHmacKey(
    DeleteHmacKeyRequest const& request) {
  return rest_->DeleteHmacKey(request);
}

StatusOr<HmacKeyMetadata> HybridClient::GetHmacKey(
    GetHmacKeyRequest const& request) {
  return rest_->GetHmacKey(request);
}

StatusOr<HmacKeyMetadata> HybridClient::UpdateHmacKey(
    UpdateHmacKeyRequest const& request) {
  return rest_->UpdateHmacKey(request);
}

StatusOr<SignBlobResponse> HybridClient::SignBlob(
    SignBlobRequest const& request) {
  return rest_->SignBlob(request);
}

StatusOr<ListNotificationsResponse> HybridClient::ListNotifications(
    ListNotificationsRequest const& request) {
  return rest_->ListNotifications(request);
}

StatusOr<NotificationMetadata> HybridClient::CreateNotification(
    CreateNotificationRequest const& request) {
  return rest_->CreateNotification(request);
}

StatusOr<NotificationMetadata> HybridClient::GetNotification(
    GetNotificationRequest const& request) {
  return rest_->GetNotification(request);
}

StatusOr<EmptyResponse> HybridClient::DeleteNotification(
    DeleteNotificationRequest const& request) {
  return rest_->DeleteNotification(request);
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
