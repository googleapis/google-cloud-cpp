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

#include "google/cloud/storage/internal/tracing_connection.h"
#include "google/cloud/storage/internal/tracing_object_read_source.h"
#include "google/cloud/storage/parallel_upload.h"
#include "google/cloud/internal/opentelemetry.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

TracingConnection::TracingConnection(std::shared_ptr<StorageConnection> impl)
    : impl_(std::move(impl)) {}

storage::ClientOptions const& TracingConnection::client_options() const {
  return impl_->client_options();
}
Options TracingConnection::options() const { return impl_->options(); }

StatusOr<storage::internal::ListBucketsResponse> TracingConnection::ListBuckets(
    storage::internal::ListBucketsRequest const& request) {
  // TODO(#11395) - use a internal::MakeTracedStreamRange in storage::Client
  auto span = internal::MakeSpan("storage::Client::ListBuckets");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->ListBuckets(request));
}

StatusOr<storage::BucketMetadata> TracingConnection::CreateBucket(
    storage::internal::CreateBucketRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::CreateBucket");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->CreateBucket(request));
}

StatusOr<storage::BucketMetadata> TracingConnection::GetBucketMetadata(
    storage::internal::GetBucketMetadataRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::GetBucketMetadata");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->GetBucketMetadata(request));
}

StatusOr<storage::internal::EmptyResponse> TracingConnection::DeleteBucket(
    storage::internal::DeleteBucketRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::DeleteBucket");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->DeleteBucket(request));
}

StatusOr<storage::BucketMetadata> TracingConnection::UpdateBucket(
    storage::internal::UpdateBucketRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::UpdateBucket");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->UpdateBucket(request));
}

StatusOr<storage::BucketMetadata> TracingConnection::PatchBucket(
    storage::internal::PatchBucketRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::PatchBucket");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->PatchBucket(request));
}

StatusOr<storage::NativeIamPolicy> TracingConnection::GetNativeBucketIamPolicy(
    storage::internal::GetBucketIamPolicyRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::GetNativeBucketIamPolicy");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->GetNativeBucketIamPolicy(request));
}

StatusOr<storage::NativeIamPolicy> TracingConnection::SetNativeBucketIamPolicy(
    storage::internal::SetNativeBucketIamPolicyRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::SetNativeBucketIamPolicy");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->SetNativeBucketIamPolicy(request));
}

StatusOr<storage::internal::TestBucketIamPermissionsResponse>
TracingConnection::TestBucketIamPermissions(
    storage::internal::TestBucketIamPermissionsRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::TestBucketIamPermissions");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->TestBucketIamPermissions(request));
}

StatusOr<storage::BucketMetadata> TracingConnection::LockBucketRetentionPolicy(
    storage::internal::LockBucketRetentionPolicyRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::LockBucketRetentionPolicy");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->LockBucketRetentionPolicy(request));
}

StatusOr<storage::ObjectMetadata> TracingConnection::InsertObjectMedia(
    storage::internal::InsertObjectMediaRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::InsertObjectMedia");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->InsertObjectMedia(request));
}

StatusOr<storage::ObjectMetadata> TracingConnection::CopyObject(
    storage::internal::CopyObjectRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::CopyObject");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->CopyObject(request));
}

StatusOr<storage::ObjectMetadata> TracingConnection::GetObjectMetadata(
    storage::internal::GetObjectMetadataRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::GetObjectMetadata");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->GetObjectMetadata(request));
}

StatusOr<std::unique_ptr<storage::internal::ObjectReadSource>>
TracingConnection::ReadObject(
    storage::internal::ReadObjectRangeRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::ReadObject");
  auto scope = opentelemetry::trace::Scope(span);
  auto reader = impl_->ReadObject(request);
  if (!reader) return internal::EndSpan(*span, std::move(reader));
  return std::unique_ptr<storage::internal::ObjectReadSource>(
      std::make_unique<TracingObjectReadSource>(std::move(span),
                                                *std::move(reader)));
}

StatusOr<storage::internal::ListObjectsResponse> TracingConnection::ListObjects(
    storage::internal::ListObjectsRequest const& request) {
  // TODO(#11395) - use a internal::MakeTracedStreamRange in storage::Client
  auto span = internal::MakeSpan("storage::Client::ListObjects");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->ListObjects(request));
}

StatusOr<storage::internal::EmptyResponse> TracingConnection::DeleteObject(
    storage::internal::DeleteObjectRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::DeleteObject");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->DeleteObject(request));
}

StatusOr<storage::ObjectMetadata> TracingConnection::UpdateObject(
    storage::internal::UpdateObjectRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::UpdateObject");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->UpdateObject(request));
}

StatusOr<storage::ObjectMetadata> TracingConnection::MoveObject(
    storage::internal::MoveObjectRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::MoveObject");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->MoveObject(request));
}

StatusOr<storage::ObjectMetadata> TracingConnection::PatchObject(
    storage::internal::PatchObjectRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::PatchObject");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->PatchObject(request));
}

StatusOr<storage::ObjectMetadata> TracingConnection::ComposeObject(
    storage::internal::ComposeObjectRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::ComposeObject");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->ComposeObject(request));
}

StatusOr<storage::internal::RewriteObjectResponse>
TracingConnection::RewriteObject(
    storage::internal::RewriteObjectRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::RewriteObject");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->RewriteObject(request));
}

StatusOr<storage::ObjectMetadata> TracingConnection::RestoreObject(
    storage::internal::RestoreObjectRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::RestoreObject");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->RestoreObject(request));
}

StatusOr<storage::internal::CreateResumableUploadResponse>
TracingConnection::CreateResumableUpload(
    storage::internal::ResumableUploadRequest const& request) {
  // TODO(#11394) - add a wrapper for WriteObject().
  auto span =
      internal::MakeSpan("storage::Client::WriteObject/CreateResumableUpload");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->CreateResumableUpload(request));
}

StatusOr<storage::internal::QueryResumableUploadResponse>
TracingConnection::QueryResumableUpload(
    storage::internal::QueryResumableUploadRequest const& request) {
  // TODO(#11394) - add a wrapper for WriteObject().
  auto span =
      internal::MakeSpan("storage::Client::WriteObject/QueryResumableUpload");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->QueryResumableUpload(request));
}

StatusOr<storage::internal::EmptyResponse>
TracingConnection::DeleteResumableUpload(
    storage::internal::DeleteResumableUploadRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::DeleteResumableUpload");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->DeleteResumableUpload(request));
}

StatusOr<storage::internal::QueryResumableUploadResponse>
TracingConnection::UploadChunk(
    storage::internal::UploadChunkRequest const& request) {
  // TODO(#11394) - add a wrapper for WriteObject().
  auto span = internal::MakeSpan("storage::Client::WriteObject/UploadChunk");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->UploadChunk(request));
}

StatusOr<std::unique_ptr<std::string>> TracingConnection::UploadFileSimple(
    std::string const& file_name, std::size_t file_size,
    storage::internal::InsertObjectMediaRequest& request) {
  auto span =
      internal::MakeSpan("storage::Client::UploadFile/UploadFileSimple");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(
      *span, impl_->UploadFileSimple(file_name, file_size, request));
}

StatusOr<std::unique_ptr<std::istream>> TracingConnection::UploadFileResumable(
    std::string const& file_name,
    storage::internal::ResumableUploadRequest& request) {
  auto span =
      internal::MakeSpan("storage::Client::UploadFile/UploadFileResumable");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span,
                           impl_->UploadFileResumable(file_name, request));
}

StatusOr<storage::ObjectMetadata> TracingConnection::ExecuteParallelUploadFile(
    std::vector<std::thread> threads,
    std::vector<storage::internal::ParallelUploadFileShard> shards,
    bool ignore_cleanup_failures) {
  auto span = internal::MakeSpan(
      "storage::ParallelUploadFile/ExecuteParallelUploadFile");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->ExecuteParallelUploadFile(
                                      std::move(threads), std::move(shards),
                                      ignore_cleanup_failures));
}

StatusOr<storage::internal::ObjectWriteStreamParams>
TracingConnection::SetupObjectWriteStream(
    storage::internal::ResumableUploadRequest const& request) {
  auto span =
      internal::MakeSpan("storage::Client::WriteObject/SetupObjectWriteStream");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->SetupObjectWriteStream(request));
}

StatusOr<storage::internal::ListBucketAclResponse>
TracingConnection::ListBucketAcl(
    storage::internal::ListBucketAclRequest const& request) {
  // TODO(#11395) - use a internal::MakeTracedStreamRange in storage::Client
  auto span = internal::MakeSpan("storage::Client::ListBucketAcl");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->ListBucketAcl(request));
}

StatusOr<storage::BucketAccessControl> TracingConnection::CreateBucketAcl(
    storage::internal::CreateBucketAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::CreateBucketAcl");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->CreateBucketAcl(request));
}

StatusOr<storage::internal::EmptyResponse> TracingConnection::DeleteBucketAcl(
    storage::internal::DeleteBucketAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::DeleteBucketAcl");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->DeleteBucketAcl(request));
}

StatusOr<storage::BucketAccessControl> TracingConnection::GetBucketAcl(
    storage::internal::GetBucketAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::GetBucketAcl");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->GetBucketAcl(request));
}

StatusOr<storage::BucketAccessControl> TracingConnection::UpdateBucketAcl(
    storage::internal::UpdateBucketAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::UpdateBucketAcl");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->UpdateBucketAcl(request));
}

StatusOr<storage::BucketAccessControl> TracingConnection::PatchBucketAcl(
    storage::internal::PatchBucketAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::PatchBucketAcl");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->PatchBucketAcl(request));
}

StatusOr<storage::internal::ListObjectAclResponse>
TracingConnection::ListObjectAcl(
    storage::internal::ListObjectAclRequest const& request) {
  // TODO(#11395) - use a internal::MakeTracedStreamRange in storage::Client
  auto span = internal::MakeSpan("storage::Client::ListObjectAcl");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->ListObjectAcl(request));
}

StatusOr<storage::ObjectAccessControl> TracingConnection::CreateObjectAcl(
    storage::internal::CreateObjectAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::CreateObjectAcl");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->CreateObjectAcl(request));
}

StatusOr<storage::internal::EmptyResponse> TracingConnection::DeleteObjectAcl(
    storage::internal::DeleteObjectAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::DeleteObjectAcl");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->DeleteObjectAcl(request));
}

StatusOr<storage::ObjectAccessControl> TracingConnection::GetObjectAcl(
    storage::internal::GetObjectAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::GetObjectAcl");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->GetObjectAcl(request));
}

StatusOr<storage::ObjectAccessControl> TracingConnection::UpdateObjectAcl(
    storage::internal::UpdateObjectAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::UpdateObjectAcl");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->UpdateObjectAcl(request));
}

StatusOr<storage::ObjectAccessControl> TracingConnection::PatchObjectAcl(
    storage::internal::PatchObjectAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::PatchObjectAcl");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->PatchObjectAcl(request));
}

StatusOr<storage::internal::ListDefaultObjectAclResponse>
TracingConnection::ListDefaultObjectAcl(
    storage::internal::ListDefaultObjectAclRequest const& request) {
  // TODO(#11395) - use a internal::MakeTracedStreamRange in storage::Client
  auto span = internal::MakeSpan("storage::Client::ListDefaultObjectAcl");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->ListDefaultObjectAcl(request));
}

StatusOr<storage::ObjectAccessControl>
TracingConnection::CreateDefaultObjectAcl(
    storage::internal::CreateDefaultObjectAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::CreateDefaultObjectAcl");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->CreateDefaultObjectAcl(request));
}

StatusOr<storage::internal::EmptyResponse>
TracingConnection::DeleteDefaultObjectAcl(
    storage::internal::DeleteDefaultObjectAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::DeleteDefaultObjectAcl");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->DeleteDefaultObjectAcl(request));
}

StatusOr<storage::ObjectAccessControl> TracingConnection::GetDefaultObjectAcl(
    storage::internal::GetDefaultObjectAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::GetDefaultObjectAcl");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->GetDefaultObjectAcl(request));
}

StatusOr<storage::ObjectAccessControl>
TracingConnection::UpdateDefaultObjectAcl(
    storage::internal::UpdateDefaultObjectAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::UpdateDefaultObjectAcl");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->UpdateDefaultObjectAcl(request));
}

StatusOr<storage::ObjectAccessControl> TracingConnection::PatchDefaultObjectAcl(
    storage::internal::PatchDefaultObjectAclRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::PatchDefaultObjectAcl");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->PatchDefaultObjectAcl(request));
}

StatusOr<storage::ServiceAccount> TracingConnection::GetServiceAccount(
    storage::internal::GetProjectServiceAccountRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::GetServiceAccount");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->GetServiceAccount(request));
}

StatusOr<storage::internal::ListHmacKeysResponse>
TracingConnection::ListHmacKeys(
    storage::internal::ListHmacKeysRequest const& request) {
  // TODO(#11395) - use a internal::MakeTracedStreamRange in storage::Client
  auto span = internal::MakeSpan("storage::Client::ListHmacKeys");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->ListHmacKeys(request));
}

StatusOr<storage::internal::CreateHmacKeyResponse>
TracingConnection::CreateHmacKey(
    storage::internal::CreateHmacKeyRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::CreateHmacKey");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->CreateHmacKey(request));
}

StatusOr<storage::internal::EmptyResponse> TracingConnection::DeleteHmacKey(
    storage::internal::DeleteHmacKeyRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::DeleteHmacKey");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->DeleteHmacKey(request));
}

StatusOr<storage::HmacKeyMetadata> TracingConnection::GetHmacKey(
    storage::internal::GetHmacKeyRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::GetHmacKey");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->GetHmacKey(request));
}

StatusOr<storage::HmacKeyMetadata> TracingConnection::UpdateHmacKey(
    storage::internal::UpdateHmacKeyRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::UpdateHmacKey");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->UpdateHmacKey(request));
}

StatusOr<storage::internal::SignBlobResponse> TracingConnection::SignBlob(
    storage::internal::SignBlobRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::SignBlob");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->SignBlob(request));
}

StatusOr<storage::internal::ListNotificationsResponse>
TracingConnection::ListNotifications(
    storage::internal::ListNotificationsRequest const& request) {
  // TODO(#11395) - use a internal::MakeTracedStreamRange in storage::Client
  auto span = internal::MakeSpan("storage::Client::ListNotifications");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->ListNotifications(request));
}

StatusOr<storage::NotificationMetadata> TracingConnection::CreateNotification(
    storage::internal::CreateNotificationRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::CreateNotification");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->CreateNotification(request));
}

StatusOr<storage::NotificationMetadata> TracingConnection::GetNotification(
    storage::internal::GetNotificationRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::GetNotification");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->GetNotification(request));
}

StatusOr<storage::internal::EmptyResponse>
TracingConnection::DeleteNotification(
    storage::internal::DeleteNotificationRequest const& request) {
  auto span = internal::MakeSpan("storage::Client::DeleteNotification");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, impl_->DeleteNotification(request));
}

std::vector<std::string> TracingConnection::InspectStackStructure() const {
  auto stack = impl_->InspectStackStructure();
  stack.emplace_back("TracingConnection");
  return stack;
}

std::shared_ptr<storage::internal::StorageConnection> MakeTracingClient(
    std::shared_ptr<storage::internal::StorageConnection> impl) {
  return std::make_shared<TracingConnection>(std::move(impl));
}

#else

std::shared_ptr<storage::internal::StorageConnection> MakeTracingClient(
    std::shared_ptr<storage::internal::StorageConnection> impl) {
  return impl;
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
