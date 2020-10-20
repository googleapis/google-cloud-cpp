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

#include "google/cloud/storage/internal/hybrid_client.h"
#include "google/cloud/storage/internal/grpc_resumable_upload_session_url.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

HybridClient::HybridClient(ClientOptions options)
    : grpc_(std::make_shared<GrpcClient>(options)),
      curl_(CurlClient::Create(std::move(options))) {}

ClientOptions const& HybridClient::client_options() const {
  return curl_->client_options();
}

StatusOr<ListBucketsResponse> HybridClient::ListBuckets(
    ListBucketsRequest const& request) {
  return curl_->ListBuckets(request);
}

StatusOr<BucketMetadata> HybridClient::CreateBucket(
    CreateBucketRequest const& request) {
  return curl_->CreateBucket(request);
}

StatusOr<BucketMetadata> HybridClient::GetBucketMetadata(
    GetBucketMetadataRequest const& request) {
  return curl_->GetBucketMetadata(request);
}

StatusOr<EmptyResponse> HybridClient::DeleteBucket(
    DeleteBucketRequest const& request) {
  return curl_->DeleteBucket(request);
}

StatusOr<BucketMetadata> HybridClient::UpdateBucket(
    UpdateBucketRequest const& request) {
  return curl_->UpdateBucket(request);
}

StatusOr<BucketMetadata> HybridClient::PatchBucket(
    PatchBucketRequest const& request) {
  return curl_->PatchBucket(request);
}

StatusOr<IamPolicy> HybridClient::GetBucketIamPolicy(
    GetBucketIamPolicyRequest const& request) {
  return curl_->GetBucketIamPolicy(request);
}

StatusOr<NativeIamPolicy> HybridClient::GetNativeBucketIamPolicy(
    GetBucketIamPolicyRequest const& request) {
  return curl_->GetNativeBucketIamPolicy(request);
}

StatusOr<IamPolicy> HybridClient::SetBucketIamPolicy(
    SetBucketIamPolicyRequest const& request) {
  return curl_->SetBucketIamPolicy(request);
}

StatusOr<NativeIamPolicy> HybridClient::SetNativeBucketIamPolicy(
    SetNativeBucketIamPolicyRequest const& request) {
  return curl_->SetNativeBucketIamPolicy(request);
}

StatusOr<TestBucketIamPermissionsResponse>
HybridClient::TestBucketIamPermissions(
    TestBucketIamPermissionsRequest const& request) {
  return curl_->TestBucketIamPermissions(request);
}

StatusOr<BucketMetadata> HybridClient::LockBucketRetentionPolicy(
    LockBucketRetentionPolicyRequest const& request) {
  return curl_->LockBucketRetentionPolicy(request);
}

StatusOr<ObjectMetadata> HybridClient::InsertObjectMedia(
    InsertObjectMediaRequest const& request) {
  return grpc_->InsertObjectMedia(request);
}

StatusOr<ObjectMetadata> HybridClient::CopyObject(
    CopyObjectRequest const& request) {
  return curl_->CopyObject(request);
}

StatusOr<ObjectMetadata> HybridClient::GetObjectMetadata(
    GetObjectMetadataRequest const& request) {
  return curl_->GetObjectMetadata(request);
}

StatusOr<std::unique_ptr<ObjectReadSource>> HybridClient::ReadObject(
    ReadObjectRangeRequest const& request) {
  return grpc_->ReadObject(request);
}

StatusOr<ListObjectsResponse> HybridClient::ListObjects(
    ListObjectsRequest const& request) {
  return curl_->ListObjects(request);
}

StatusOr<EmptyResponse> HybridClient::DeleteObject(
    DeleteObjectRequest const& request) {
  return curl_->DeleteObject(request);
}

StatusOr<ObjectMetadata> HybridClient::UpdateObject(
    UpdateObjectRequest const& request) {
  return curl_->UpdateObject(request);
}

StatusOr<ObjectMetadata> HybridClient::PatchObject(
    PatchObjectRequest const& request) {
  return curl_->PatchObject(request);
}

StatusOr<ObjectMetadata> HybridClient::ComposeObject(
    ComposeObjectRequest const& request) {
  return curl_->ComposeObject(request);
}

StatusOr<RewriteObjectResponse> HybridClient::RewriteObject(
    RewriteObjectRequest const& request) {
  return curl_->RewriteObject(request);
}

StatusOr<std::unique_ptr<ResumableUploadSession>>
HybridClient::CreateResumableSession(ResumableUploadRequest const& request) {
  return grpc_->CreateResumableSession(request);
}

StatusOr<std::unique_ptr<ResumableUploadSession>>
HybridClient::RestoreResumableSession(std::string const& upload_id) {
  if (internal::IsGrpcResumableSessionUrl(upload_id)) {
    return grpc_->RestoreResumableSession(upload_id);
  }
  return curl_->RestoreResumableSession(upload_id);
}

StatusOr<EmptyResponse> HybridClient::DeleteResumableUpload(
    DeleteResumableUploadRequest const& request) {
  if (internal::IsGrpcResumableSessionUrl(request.upload_session_url())) {
    return grpc_->DeleteResumableUpload(request);
  }
  return curl_->DeleteResumableUpload(request);
}

StatusOr<ListBucketAclResponse> HybridClient::ListBucketAcl(
    ListBucketAclRequest const& request) {
  return curl_->ListBucketAcl(request);
}

StatusOr<BucketAccessControl> HybridClient::CreateBucketAcl(
    CreateBucketAclRequest const& request) {
  return curl_->CreateBucketAcl(request);
}

StatusOr<EmptyResponse> HybridClient::DeleteBucketAcl(
    DeleteBucketAclRequest const& request) {
  return curl_->DeleteBucketAcl(request);
}

StatusOr<BucketAccessControl> HybridClient::GetBucketAcl(
    GetBucketAclRequest const& request) {
  return curl_->GetBucketAcl(request);
}

StatusOr<BucketAccessControl> HybridClient::UpdateBucketAcl(
    UpdateBucketAclRequest const& request) {
  return curl_->UpdateBucketAcl(request);
}

StatusOr<BucketAccessControl> HybridClient::PatchBucketAcl(
    PatchBucketAclRequest const& request) {
  return curl_->PatchBucketAcl(request);
}

StatusOr<ListObjectAclResponse> HybridClient::ListObjectAcl(
    ListObjectAclRequest const& request) {
  return curl_->ListObjectAcl(request);
}

StatusOr<ObjectAccessControl> HybridClient::CreateObjectAcl(
    CreateObjectAclRequest const& request) {
  return curl_->CreateObjectAcl(request);
}

StatusOr<EmptyResponse> HybridClient::DeleteObjectAcl(
    DeleteObjectAclRequest const& request) {
  return curl_->DeleteObjectAcl(request);
}

StatusOr<ObjectAccessControl> HybridClient::GetObjectAcl(
    GetObjectAclRequest const& request) {
  return curl_->GetObjectAcl(request);
}

StatusOr<ObjectAccessControl> HybridClient::UpdateObjectAcl(
    UpdateObjectAclRequest const& request) {
  return curl_->UpdateObjectAcl(request);
}

StatusOr<ObjectAccessControl> HybridClient::PatchObjectAcl(
    PatchObjectAclRequest const& request) {
  return curl_->PatchObjectAcl(request);
}

StatusOr<ListDefaultObjectAclResponse> HybridClient::ListDefaultObjectAcl(
    ListDefaultObjectAclRequest const& request) {
  return curl_->ListDefaultObjectAcl(request);
}

StatusOr<ObjectAccessControl> HybridClient::CreateDefaultObjectAcl(
    CreateDefaultObjectAclRequest const& request) {
  return curl_->CreateDefaultObjectAcl(request);
}

StatusOr<EmptyResponse> HybridClient::DeleteDefaultObjectAcl(
    DeleteDefaultObjectAclRequest const& request) {
  return curl_->DeleteDefaultObjectAcl(request);
}

StatusOr<ObjectAccessControl> HybridClient::GetDefaultObjectAcl(
    GetDefaultObjectAclRequest const& request) {
  return curl_->GetDefaultObjectAcl(request);
}

StatusOr<ObjectAccessControl> HybridClient::UpdateDefaultObjectAcl(
    UpdateDefaultObjectAclRequest const& request) {
  return curl_->UpdateDefaultObjectAcl(request);
}

StatusOr<ObjectAccessControl> HybridClient::PatchDefaultObjectAcl(
    PatchDefaultObjectAclRequest const& request) {
  return curl_->PatchDefaultObjectAcl(request);
}

StatusOr<ServiceAccount> HybridClient::GetServiceAccount(
    GetProjectServiceAccountRequest const& request) {
  return curl_->GetServiceAccount(request);
}

StatusOr<ListHmacKeysResponse> HybridClient::ListHmacKeys(
    ListHmacKeysRequest const& request) {
  return curl_->ListHmacKeys(request);
}

StatusOr<CreateHmacKeyResponse> HybridClient::CreateHmacKey(
    CreateHmacKeyRequest const& request) {
  return curl_->CreateHmacKey(request);
}

StatusOr<EmptyResponse> HybridClient::DeleteHmacKey(
    DeleteHmacKeyRequest const& request) {
  return curl_->DeleteHmacKey(request);
}

StatusOr<HmacKeyMetadata> HybridClient::GetHmacKey(
    GetHmacKeyRequest const& request) {
  return curl_->GetHmacKey(request);
}

StatusOr<HmacKeyMetadata> HybridClient::UpdateHmacKey(
    UpdateHmacKeyRequest const& request) {
  return curl_->UpdateHmacKey(request);
}

StatusOr<SignBlobResponse> HybridClient::SignBlob(
    SignBlobRequest const& request) {
  return curl_->SignBlob(request);
}

StatusOr<ListNotificationsResponse> HybridClient::ListNotifications(
    ListNotificationsRequest const& request) {
  return curl_->ListNotifications(request);
}

StatusOr<NotificationMetadata> HybridClient::CreateNotification(
    CreateNotificationRequest const& request) {
  return curl_->CreateNotification(request);
}

StatusOr<NotificationMetadata> HybridClient::GetNotification(
    GetNotificationRequest const& request) {
  return curl_->GetNotification(request);
}

StatusOr<EmptyResponse> HybridClient::DeleteNotification(
    DeleteNotificationRequest const& request) {
  return curl_->DeleteNotification(request);
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
