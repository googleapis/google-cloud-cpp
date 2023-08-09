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
#include "google/cloud/storage/internal/generic_stub_adapter.h"
#include "google/cloud/storage/internal/grpc/stub.h"
#include "google/cloud/storage/internal/rest/stub.h"
#include "absl/strings/match.h"
#include <iterator>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

HybridClient::HybridClient(Options const& options)
    : grpc_(std::make_unique<GrpcStub>(DefaultOptionsGrpc(options))),
      rest_(std::make_unique<storage::internal::RestStub>(
          storage::internal::DefaultOptionsWithCredentials(options))) {}

Options HybridClient::options() const { return grpc_->options(); }

StatusOr<storage::internal::ListBucketsResponse> HybridClient::ListBuckets(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::ListBucketsRequest const& request) {
  return rest_->ListBuckets(context, options, request);
}

StatusOr<storage::BucketMetadata> HybridClient::CreateBucket(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::CreateBucketRequest const& request) {
  return rest_->CreateBucket(context, options, request);
}

StatusOr<storage::BucketMetadata> HybridClient::GetBucketMetadata(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::GetBucketMetadataRequest const& request) {
  return rest_->GetBucketMetadata(context, options, request);
}

StatusOr<storage::internal::EmptyResponse> HybridClient::DeleteBucket(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::DeleteBucketRequest const& request) {
  return rest_->DeleteBucket(context, options, request);
}

StatusOr<storage::BucketMetadata> HybridClient::UpdateBucket(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::UpdateBucketRequest const& request) {
  return rest_->UpdateBucket(context, options, request);
}

StatusOr<storage::BucketMetadata> HybridClient::PatchBucket(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::PatchBucketRequest const& request) {
  return rest_->PatchBucket(context, options, request);
}

StatusOr<storage::NativeIamPolicy> HybridClient::GetNativeBucketIamPolicy(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::GetBucketIamPolicyRequest const& request) {
  return rest_->GetNativeBucketIamPolicy(context, options, request);
}

StatusOr<storage::NativeIamPolicy> HybridClient::SetNativeBucketIamPolicy(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::SetNativeBucketIamPolicyRequest const& request) {
  return rest_->SetNativeBucketIamPolicy(context, options, request);
}

StatusOr<storage::internal::TestBucketIamPermissionsResponse>
HybridClient::TestBucketIamPermissions(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::TestBucketIamPermissionsRequest const& request) {
  return rest_->TestBucketIamPermissions(context, options, request);
}

StatusOr<storage::BucketMetadata> HybridClient::LockBucketRetentionPolicy(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::LockBucketRetentionPolicyRequest const& request) {
  return rest_->LockBucketRetentionPolicy(context, options, request);
}

StatusOr<storage::ObjectMetadata> HybridClient::InsertObjectMedia(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::InsertObjectMediaRequest const& request) {
  return grpc_->InsertObjectMedia(context, options, request);
}

StatusOr<storage::ObjectMetadata> HybridClient::CopyObject(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::CopyObjectRequest const& request) {
  return rest_->CopyObject(context, options, request);
}

StatusOr<storage::ObjectMetadata> HybridClient::GetObjectMetadata(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::GetObjectMetadataRequest const& request) {
  return rest_->GetObjectMetadata(context, options, request);
}

StatusOr<std::unique_ptr<storage::internal::ObjectReadSource>>
HybridClient::ReadObject(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::ReadObjectRangeRequest const& request) {
  return grpc_->ReadObject(context, options, request);
}

StatusOr<storage::internal::ListObjectsResponse> HybridClient::ListObjects(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::ListObjectsRequest const& request) {
  return rest_->ListObjects(context, options, request);
}

StatusOr<storage::internal::EmptyResponse> HybridClient::DeleteObject(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::DeleteObjectRequest const& request) {
  return rest_->DeleteObject(context, options, request);
}

StatusOr<storage::ObjectMetadata> HybridClient::UpdateObject(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::UpdateObjectRequest const& request) {
  return rest_->UpdateObject(context, options, request);
}

StatusOr<storage::ObjectMetadata> HybridClient::PatchObject(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::PatchObjectRequest const& request) {
  return rest_->PatchObject(context, options, request);
}

StatusOr<storage::ObjectMetadata> HybridClient::ComposeObject(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::ComposeObjectRequest const& request) {
  return rest_->ComposeObject(context, options, request);
}

StatusOr<storage::internal::RewriteObjectResponse> HybridClient::RewriteObject(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::RewriteObjectRequest const& request) {
  return rest_->RewriteObject(context, options, request);
}

StatusOr<storage::internal::CreateResumableUploadResponse>
HybridClient::CreateResumableUpload(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::ResumableUploadRequest const& request) {
  return grpc_->CreateResumableUpload(context, options, request);
}

StatusOr<storage::internal::QueryResumableUploadResponse>
HybridClient::QueryResumableUpload(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::QueryResumableUploadRequest const& request) {
  return grpc_->QueryResumableUpload(context, options, request);
}

StatusOr<storage::internal::EmptyResponse> HybridClient::DeleteResumableUpload(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::DeleteResumableUploadRequest const& request) {
  if (absl::StartsWith(request.upload_session_url(), "https://")) {
    return rest_->DeleteResumableUpload(context, options, request);
  }
  return grpc_->DeleteResumableUpload(context, options, request);
}

StatusOr<storage::internal::QueryResumableUploadResponse>
HybridClient::UploadChunk(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::UploadChunkRequest const& request) {
  return grpc_->UploadChunk(context, options, request);
}

StatusOr<storage::internal::ListBucketAclResponse> HybridClient::ListBucketAcl(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::ListBucketAclRequest const& request) {
  return rest_->ListBucketAcl(context, options, request);
}

StatusOr<storage::BucketAccessControl> HybridClient::CreateBucketAcl(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::CreateBucketAclRequest const& request) {
  return rest_->CreateBucketAcl(context, options, request);
}

StatusOr<storage::internal::EmptyResponse> HybridClient::DeleteBucketAcl(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::DeleteBucketAclRequest const& request) {
  return rest_->DeleteBucketAcl(context, options, request);
}

StatusOr<storage::BucketAccessControl> HybridClient::GetBucketAcl(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::GetBucketAclRequest const& request) {
  return rest_->GetBucketAcl(context, options, request);
}

StatusOr<storage::BucketAccessControl> HybridClient::UpdateBucketAcl(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::UpdateBucketAclRequest const& request) {
  return rest_->UpdateBucketAcl(context, options, request);
}

StatusOr<storage::BucketAccessControl> HybridClient::PatchBucketAcl(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::PatchBucketAclRequest const& request) {
  return rest_->PatchBucketAcl(context, options, request);
}

StatusOr<storage::internal::ListObjectAclResponse> HybridClient::ListObjectAcl(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::ListObjectAclRequest const& request) {
  return rest_->ListObjectAcl(context, options, request);
}

StatusOr<storage::ObjectAccessControl> HybridClient::CreateObjectAcl(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::CreateObjectAclRequest const& request) {
  return rest_->CreateObjectAcl(context, options, request);
}

StatusOr<storage::internal::EmptyResponse> HybridClient::DeleteObjectAcl(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::DeleteObjectAclRequest const& request) {
  return rest_->DeleteObjectAcl(context, options, request);
}

StatusOr<storage::ObjectAccessControl> HybridClient::GetObjectAcl(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::GetObjectAclRequest const& request) {
  return rest_->GetObjectAcl(context, options, request);
}

StatusOr<storage::ObjectAccessControl> HybridClient::UpdateObjectAcl(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::UpdateObjectAclRequest const& request) {
  return rest_->UpdateObjectAcl(context, options, request);
}

StatusOr<storage::ObjectAccessControl> HybridClient::PatchObjectAcl(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::PatchObjectAclRequest const& request) {
  return rest_->PatchObjectAcl(context, options, request);
}

StatusOr<storage::internal::ListDefaultObjectAclResponse>
HybridClient::ListDefaultObjectAcl(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::ListDefaultObjectAclRequest const& request) {
  return rest_->ListDefaultObjectAcl(context, options, request);
}

StatusOr<storage::ObjectAccessControl> HybridClient::CreateDefaultObjectAcl(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::CreateDefaultObjectAclRequest const& request) {
  return rest_->CreateDefaultObjectAcl(context, options, request);
}

StatusOr<storage::internal::EmptyResponse> HybridClient::DeleteDefaultObjectAcl(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::DeleteDefaultObjectAclRequest const& request) {
  return rest_->DeleteDefaultObjectAcl(context, options, request);
}

StatusOr<storage::ObjectAccessControl> HybridClient::GetDefaultObjectAcl(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::GetDefaultObjectAclRequest const& request) {
  return rest_->GetDefaultObjectAcl(context, options, request);
}

StatusOr<storage::ObjectAccessControl> HybridClient::UpdateDefaultObjectAcl(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::UpdateDefaultObjectAclRequest const& request) {
  return rest_->UpdateDefaultObjectAcl(context, options, request);
}

StatusOr<storage::ObjectAccessControl> HybridClient::PatchDefaultObjectAcl(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::PatchDefaultObjectAclRequest const& request) {
  return rest_->PatchDefaultObjectAcl(context, options, request);
}

StatusOr<storage::ServiceAccount> HybridClient::GetServiceAccount(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::GetProjectServiceAccountRequest const& request) {
  return rest_->GetServiceAccount(context, options, request);
}

StatusOr<storage::internal::ListHmacKeysResponse> HybridClient::ListHmacKeys(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::ListHmacKeysRequest const& request) {
  return rest_->ListHmacKeys(context, options, request);
}

StatusOr<storage::internal::CreateHmacKeyResponse> HybridClient::CreateHmacKey(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::CreateHmacKeyRequest const& request) {
  return rest_->CreateHmacKey(context, options, request);
}

StatusOr<storage::internal::EmptyResponse> HybridClient::DeleteHmacKey(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::DeleteHmacKeyRequest const& request) {
  return rest_->DeleteHmacKey(context, options, request);
}

StatusOr<storage::HmacKeyMetadata> HybridClient::GetHmacKey(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::GetHmacKeyRequest const& request) {
  return rest_->GetHmacKey(context, options, request);
}

StatusOr<storage::HmacKeyMetadata> HybridClient::UpdateHmacKey(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::UpdateHmacKeyRequest const& request) {
  return rest_->UpdateHmacKey(context, options, request);
}

StatusOr<storage::internal::SignBlobResponse> HybridClient::SignBlob(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::SignBlobRequest const& request) {
  return rest_->SignBlob(context, options, request);
}

StatusOr<storage::internal::ListNotificationsResponse>
HybridClient::ListNotifications(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::ListNotificationsRequest const& request) {
  return rest_->ListNotifications(context, options, request);
}

StatusOr<storage::NotificationMetadata> HybridClient::CreateNotification(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::CreateNotificationRequest const& request) {
  return rest_->CreateNotification(context, options, request);
}

StatusOr<storage::NotificationMetadata> HybridClient::GetNotification(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::GetNotificationRequest const& request) {
  return rest_->GetNotification(context, options, request);
}

StatusOr<storage::internal::EmptyResponse> HybridClient::DeleteNotification(
    rest_internal::RestContext& context, Options const& options,
    storage::internal::DeleteNotificationRequest const& request) {
  return rest_->DeleteNotification(context, options, request);
}

std::vector<std::string> HybridClient::InspectStackStructure() const {
  auto grpc = grpc_->InspectStackStructure();
  auto rest = rest_->InspectStackStructure();
  grpc.insert(grpc.end(), std::make_move_iterator(rest.begin()),
              std::make_move_iterator(rest.end()));
  grpc.emplace_back("HybridClient");
  return grpc;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
