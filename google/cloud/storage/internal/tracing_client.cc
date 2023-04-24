// Copyright 2023 Google LLC
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

#include "google/cloud/storage/internal/tracing_client.h"
#include "google/cloud/internal/opentelemetry.h"

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

TracingClient::TracingClient(std::shared_ptr<RawClient> impl)
    : impl_(std::move(impl)) {}

storage::ClientOptions const& TracingClient::client_options() const {
  return impl_->client_options();
}
Options TracingClient::options() const { return impl_->options(); }

StatusOr<storage::internal::ListBucketsResponse> TracingClient::ListBuckets(
    storage::internal::ListBucketsRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::ListBuckets");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->ListBuckets(request));
}

StatusOr<storage::BucketMetadata> TracingClient::CreateBucket(
    storage::internal::CreateBucketRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::CreateBucket");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->CreateBucket(request));
}

StatusOr<storage::BucketMetadata> TracingClient::GetBucketMetadata(
    storage::internal::GetBucketMetadataRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::GetBucketMetadata");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->GetBucketMetadata(request));
}

StatusOr<storage::internal::EmptyResponse> TracingClient::DeleteBucket(
    storage::internal::DeleteBucketRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::DeleteBucket");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->DeleteBucket(request));
}

StatusOr<storage::BucketMetadata> TracingClient::UpdateBucket(
    storage::internal::UpdateBucketRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::UpdateBucket");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->UpdateBucket(request));
}

StatusOr<storage::BucketMetadata> TracingClient::PatchBucket(
    storage::internal::PatchBucketRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::PatchBucket");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->PatchBucket(request));
}

StatusOr<storage::NativeIamPolicy> TracingClient::GetNativeBucketIamPolicy(
    storage::internal::GetBucketIamPolicyRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::GetNativeBucketIamPolicy");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->GetNativeBucketIamPolicy(request));
}

StatusOr<storage::NativeIamPolicy> TracingClient::SetNativeBucketIamPolicy(
    storage::internal::SetNativeBucketIamPolicyRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::SetNativeBucketIamPolicy");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->SetNativeBucketIamPolicy(request));
}

StatusOr<storage::internal::TestBucketIamPermissionsResponse>
TracingClient::TestBucketIamPermissions(
    storage::internal::TestBucketIamPermissionsRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::TestBucketIamPermissions");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->TestBucketIamPermissions(request));
}

StatusOr<storage::BucketMetadata> TracingClient::LockBucketRetentionPolicy(
    storage::internal::LockBucketRetentionPolicyRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::LockBucketRetentionPolicy");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->LockBucketRetentionPolicy(request));
}

StatusOr<storage::ObjectMetadata> TracingClient::InsertObjectMedia(
    storage::internal::InsertObjectMediaRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::InsertObjectMedia");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->InsertObjectMedia(request));
}

StatusOr<storage::ObjectMetadata> TracingClient::CopyObject(
    storage::internal::CopyObjectRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::CopyObject");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->CopyObject(request));
}

StatusOr<storage::ObjectMetadata> TracingClient::GetObjectMetadata(
    storage::internal::GetObjectMetadataRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::GetObjectMetadata");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->GetObjectMetadata(request));
}

StatusOr<std::unique_ptr<storage::internal::ObjectReadSource>>
TracingClient::ReadObject(
    storage::internal::ReadObjectRangeRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::ReadObject");
  auto scope = opentelemetry::trace::Scope(span);
  // TODO(#.....) - add a wrapper for ReadObjectSource.
  return internal::EndSpan(*span, impl_->ReadObject(request));
}

StatusOr<storage::internal::ListObjectsResponse> TracingClient::ListObjects(
    storage::internal::ListObjectsRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::ListObjects");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->ListObjects(request));
}

StatusOr<storage::internal::EmptyResponse> TracingClient::DeleteObject(
    storage::internal::DeleteObjectRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::DeleteObject");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->DeleteObject(request));
}

StatusOr<storage::ObjectMetadata> TracingClient::UpdateObject(
    storage::internal::UpdateObjectRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::UpdateObject");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->UpdateObject(request));
}

StatusOr<storage::ObjectMetadata> TracingClient::PatchObject(
    storage::internal::PatchObjectRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::PatchObject");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->PatchObject(request));
}

StatusOr<storage::ObjectMetadata> TracingClient::ComposeObject(
    storage::internal::ComposeObjectRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::ComposeObject");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->ComposeObject(request));
}

StatusOr<storage::internal::RewriteObjectResponse> TracingClient::RewriteObject(
    storage::internal::RewriteObjectRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::RewriteObject");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->RewriteObject(request));
}

StatusOr<storage::internal::CreateResumableUploadResponse>
TracingClient::CreateResumableUpload(
    storage::internal::ResumableUploadRequest const& request) {
  auto span = internal::MakeSpan("storage::RawClient::CreateResumableUpload");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->CreateResumableUpload(request));
}

StatusOr<storage::internal::QueryResumableUploadResponse>
TracingClient::QueryResumableUpload(
    storage::internal::QueryResumableUploadRequest const& request) {
  auto span = internal::MakeSpan("storage::RawClient::QueryResumableUpload");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->QueryResumableUpload(request));
}

StatusOr<storage::internal::EmptyResponse> TracingClient::DeleteResumableUpload(
    storage::internal::DeleteResumableUploadRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::DeleteResumableUpload");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->DeleteResumableUpload(request));
}

StatusOr<storage::internal::QueryResumableUploadResponse>
TracingClient::UploadChunk(
    storage::internal::UploadChunkRequest const& request) {
  auto span = internal::MakeSpan("storage::RawClient::UploadChunk");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->UploadChunk(request));
}

StatusOr<storage::internal::ListBucketAclResponse> TracingClient::ListBucketAcl(
    storage::internal::ListBucketAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::ListBucketAcl");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->ListBucketAcl(request));
}

StatusOr<storage::BucketAccessControl> TracingClient::CreateBucketAcl(
    storage::internal::CreateBucketAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::CreateBucketAcl");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->CreateBucketAcl(request));
}

StatusOr<storage::internal::EmptyResponse> TracingClient::DeleteBucketAcl(
    storage::internal::DeleteBucketAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::DeleteBucketAcl");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->DeleteBucketAcl(request));
}

StatusOr<storage::BucketAccessControl> TracingClient::GetBucketAcl(
    storage::internal::GetBucketAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::GetBucketAcl");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->GetBucketAcl(request));
}

StatusOr<storage::BucketAccessControl> TracingClient::UpdateBucketAcl(
    storage::internal::UpdateBucketAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::UpdateBucketAcl");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->UpdateBucketAcl(request));
}

StatusOr<storage::BucketAccessControl> TracingClient::PatchBucketAcl(
    storage::internal::PatchBucketAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::PatchBucketAcl");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->PatchBucketAcl(request));
}

StatusOr<storage::internal::ListObjectAclResponse> TracingClient::ListObjectAcl(
    storage::internal::ListObjectAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::ListObjectAcl");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->ListObjectAcl(request));
}

StatusOr<storage::ObjectAccessControl> TracingClient::CreateObjectAcl(
    storage::internal::CreateObjectAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::CreateObjectAcl");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->CreateObjectAcl(request));
}

StatusOr<storage::internal::EmptyResponse> TracingClient::DeleteObjectAcl(
    storage::internal::DeleteObjectAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::DeleteObjectAcl");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->DeleteObjectAcl(request));
}

StatusOr<storage::ObjectAccessControl> TracingClient::GetObjectAcl(
    storage::internal::GetObjectAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::GetObjectAcl");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->GetObjectAcl(request));
}

StatusOr<storage::ObjectAccessControl> TracingClient::UpdateObjectAcl(
    storage::internal::UpdateObjectAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::UpdateObjectAcl");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->UpdateObjectAcl(request));
}

StatusOr<storage::ObjectAccessControl> TracingClient::PatchObjectAcl(
    storage::internal::PatchObjectAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::PatchObjectAcl");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->PatchObjectAcl(request));
}

StatusOr<storage::internal::ListDefaultObjectAclResponse>
TracingClient::ListDefaultObjectAcl(
    storage::internal::ListDefaultObjectAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::ListDefaultObjectAcl");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->ListDefaultObjectAcl(request));
}

StatusOr<storage::ObjectAccessControl> TracingClient::CreateDefaultObjectAcl(
    storage::internal::CreateDefaultObjectAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::CreateDefaultObjectAcl");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->CreateDefaultObjectAcl(request));
}

StatusOr<storage::internal::EmptyResponse>
TracingClient::DeleteDefaultObjectAcl(
    storage::internal::DeleteDefaultObjectAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::DeleteDefaultObjectAcl");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->DeleteDefaultObjectAcl(request));
}

StatusOr<storage::ObjectAccessControl> TracingClient::GetDefaultObjectAcl(
    storage::internal::GetDefaultObjectAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::GetDefaultObjectAcl");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->GetDefaultObjectAcl(request));
}

StatusOr<storage::ObjectAccessControl> TracingClient::UpdateDefaultObjectAcl(
    storage::internal::UpdateDefaultObjectAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::UpdateDefaultObjectAcl");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->UpdateDefaultObjectAcl(request));
}

StatusOr<storage::ObjectAccessControl> TracingClient::PatchDefaultObjectAcl(
    storage::internal::PatchDefaultObjectAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::PatchDefaultObjectAcl");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->PatchDefaultObjectAcl(request));
}

StatusOr<storage::ServiceAccount> TracingClient::GetServiceAccount(
    storage::internal::GetProjectServiceAccountRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::GetServiceAccount");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->GetServiceAccount(request));
}

StatusOr<storage::internal::ListHmacKeysResponse> TracingClient::ListHmacKeys(
    storage::internal::ListHmacKeysRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::ListHmacKeys");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->ListHmacKeys(request));
}

StatusOr<storage::internal::CreateHmacKeyResponse> TracingClient::CreateHmacKey(
    storage::internal::CreateHmacKeyRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::CreateHmacKey");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->CreateHmacKey(request));
}

StatusOr<storage::internal::EmptyResponse> TracingClient::DeleteHmacKey(
    storage::internal::DeleteHmacKeyRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::DeleteHmacKey");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->DeleteHmacKey(request));
}

StatusOr<storage::HmacKeyMetadata> TracingClient::GetHmacKey(
    storage::internal::GetHmacKeyRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::GetHmacKey");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->GetHmacKey(request));
}

StatusOr<storage::HmacKeyMetadata> TracingClient::UpdateHmacKey(
    storage::internal::UpdateHmacKeyRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::UpdateHmacKey");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->UpdateHmacKey(request));
}

StatusOr<storage::internal::SignBlobResponse> TracingClient::SignBlob(
    storage::internal::SignBlobRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::SignBlob");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->SignBlob(request));
}

StatusOr<storage::internal::ListNotificationsResponse>
TracingClient::ListNotifications(
    storage::internal::ListNotificationsRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::ListNotifications");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->ListNotifications(request));
}

StatusOr<storage::NotificationMetadata> TracingClient::CreateNotification(
    storage::internal::CreateNotificationRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::CreateNotification");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->CreateNotification(request));
}

StatusOr<storage::NotificationMetadata> TracingClient::GetNotification(
    storage::internal::GetNotificationRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::GetNotification");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->GetNotification(request));
}

StatusOr<storage::internal::EmptyResponse> TracingClient::DeleteNotification(
    storage::internal::DeleteNotificationRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::DeleteNotification");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->DeleteNotification(request));
}

std::shared_ptr<storage::internal::RawClient> MakeTracingClient(
    std::shared_ptr<storage::internal::RawClient> impl) {
  return std::make_shared<TracingClient>(std::move(impl));
}

#else

std::shared_ptr<storage::internal::RawClient> MakeTracingClient(
    std::shared_ptr<storage::internal::RawClient> impl) {
  return impl;
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
