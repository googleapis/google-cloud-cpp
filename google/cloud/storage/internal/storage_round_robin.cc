// Copyright 2021 Google LLC
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

#include "google/cloud/storage/internal/storage_round_robin.h"

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

Status StorageRoundRobin::DeleteBucket(
    grpc::ClientContext& context,
    google::storage::v2::DeleteBucketRequest const& request) {
  return Child()->DeleteBucket(context, request);
}

StatusOr<google::storage::v2::Bucket> StorageRoundRobin::GetBucket(
    grpc::ClientContext& context,
    google::storage::v2::GetBucketRequest const& request) {
  return Child()->GetBucket(context, request);
}

StatusOr<google::storage::v2::Bucket> StorageRoundRobin::CreateBucket(
    grpc::ClientContext& context,
    google::storage::v2::CreateBucketRequest const& request) {
  return Child()->CreateBucket(context, request);
}

StatusOr<google::storage::v2::ListBucketsResponse>
StorageRoundRobin::ListBuckets(
    grpc::ClientContext& context,
    google::storage::v2::ListBucketsRequest const& request) {
  return Child()->ListBuckets(context, request);
}

StatusOr<google::storage::v2::Bucket>
StorageRoundRobin::LockBucketRetentionPolicy(
    grpc::ClientContext& context,
    google::storage::v2::LockBucketRetentionPolicyRequest const& request) {
  return Child()->LockBucketRetentionPolicy(context, request);
}

StatusOr<google::iam::v1::Policy> StorageRoundRobin::GetIamPolicy(
    grpc::ClientContext& context,
    google::iam::v1::GetIamPolicyRequest const& request) {
  return Child()->GetIamPolicy(context, request);
}

StatusOr<google::iam::v1::Policy> StorageRoundRobin::SetIamPolicy(
    grpc::ClientContext& context,
    google::iam::v1::SetIamPolicyRequest const& request) {
  return Child()->SetIamPolicy(context, request);
}

StatusOr<google::iam::v1::TestIamPermissionsResponse>
StorageRoundRobin::TestIamPermissions(
    grpc::ClientContext& context,
    google::iam::v1::TestIamPermissionsRequest const& request) {
  return Child()->TestIamPermissions(context, request);
}

StatusOr<google::storage::v2::Bucket> StorageRoundRobin::UpdateBucket(
    grpc::ClientContext& context,
    google::storage::v2::UpdateBucketRequest const& request) {
  return Child()->UpdateBucket(context, request);
}

StatusOr<google::storage::v2::Object> StorageRoundRobin::ComposeObject(
    grpc::ClientContext& context,
    google::storage::v2::ComposeObjectRequest const& request) {
  return Child()->ComposeObject(context, request);
}

Status StorageRoundRobin::DeleteObject(
    grpc::ClientContext& context,
    google::storage::v2::DeleteObjectRequest const& request) {
  return Child()->DeleteObject(context, request);
}

StatusOr<google::storage::v2::Object> StorageRoundRobin::GetObject(
    grpc::ClientContext& context,
    google::storage::v2::GetObjectRequest const& request) {
  return Child()->GetObject(context, request);
}

std::unique_ptr<google::cloud::internal::StreamingReadRpc<
    google::storage::v2::ReadObjectResponse>>
StorageRoundRobin::ReadObject(
    std::unique_ptr<grpc::ClientContext> context,
    google::storage::v2::ReadObjectRequest const& request) {
  return Child()->ReadObject(std::move(context), request);
}

StatusOr<google::storage::v2::Object> StorageRoundRobin::UpdateObject(
    grpc::ClientContext& context,
    google::storage::v2::UpdateObjectRequest const& request) {
  return Child()->UpdateObject(context, request);
}

std::unique_ptr<google::cloud::internal::StreamingWriteRpc<
    google::storage::v2::WriteObjectRequest,
    google::storage::v2::WriteObjectResponse>>
StorageRoundRobin::WriteObject(std::unique_ptr<grpc::ClientContext> context) {
  return Child()->WriteObject(std::move(context));
}

StatusOr<google::storage::v2::ListObjectsResponse>
StorageRoundRobin::ListObjects(
    grpc::ClientContext& context,
    google::storage::v2::ListObjectsRequest const& request) {
  return Child()->ListObjects(context, request);
}

StatusOr<google::storage::v2::RewriteResponse> StorageRoundRobin::RewriteObject(
    grpc::ClientContext& context,
    google::storage::v2::RewriteObjectRequest const& request) {
  return Child()->RewriteObject(context, request);
}

StatusOr<google::storage::v2::StartResumableWriteResponse>
StorageRoundRobin::StartResumableWrite(
    grpc::ClientContext& context,
    google::storage::v2::StartResumableWriteRequest const& request) {
  return Child()->StartResumableWrite(context, request);
}

StatusOr<google::storage::v2::QueryWriteStatusResponse>
StorageRoundRobin::QueryWriteStatus(
    grpc::ClientContext& context,
    google::storage::v2::QueryWriteStatusRequest const& request) {
  return Child()->QueryWriteStatus(context, request);
}

StatusOr<google::storage::v2::ServiceAccount>
StorageRoundRobin::GetServiceAccount(
    grpc::ClientContext& context,
    google::storage::v2::GetServiceAccountRequest const& request) {
  return Child()->GetServiceAccount(context, request);
}

StatusOr<google::storage::v2::CreateHmacKeyResponse>
StorageRoundRobin::CreateHmacKey(
    grpc::ClientContext& context,
    google::storage::v2::CreateHmacKeyRequest const& request) {
  return Child()->CreateHmacKey(context, request);
}

Status StorageRoundRobin::DeleteHmacKey(
    grpc::ClientContext& context,
    google::storage::v2::DeleteHmacKeyRequest const& request) {
  return Child()->DeleteHmacKey(context, request);
}

StatusOr<google::storage::v2::HmacKeyMetadata> StorageRoundRobin::GetHmacKey(
    grpc::ClientContext& context,
    google::storage::v2::GetHmacKeyRequest const& request) {
  return Child()->GetHmacKey(context, request);
}

StatusOr<google::storage::v2::ListHmacKeysResponse>
StorageRoundRobin::ListHmacKeys(
    grpc::ClientContext& context,
    google::storage::v2::ListHmacKeysRequest const& request) {
  return Child()->ListHmacKeys(context, request);
}

StatusOr<google::storage::v2::HmacKeyMetadata> StorageRoundRobin::UpdateHmacKey(
    grpc::ClientContext& context,
    google::storage::v2::UpdateHmacKeyRequest const& request) {
  return Child()->UpdateHmacKey(context, request);
}

future<Status> StorageRoundRobin::AsyncDeleteObject(
    google::cloud::CompletionQueue& cq,
    std::unique_ptr<grpc::ClientContext> context,
    google::storage::v2::DeleteObjectRequest const& request) {
  return Child()->AsyncDeleteObject(cq, std::move(context), request);
}

std::unique_ptr<::google::cloud::internal::AsyncStreamingReadRpc<
    google::storage::v2::ReadObjectResponse>>
StorageRoundRobin::AsyncReadObject(
    google::cloud::CompletionQueue const& cq,
    std::unique_ptr<grpc::ClientContext> context,
    google::storage::v2::ReadObjectRequest const& request) {
  return Child()->AsyncReadObject(cq, std::move(context), request);
}

std::shared_ptr<StorageStub> StorageRoundRobin::Child() {
  std::lock_guard<std::mutex> lk(mu_);
  auto child = children_[current_];
  current_ = (current_ + 1) % children_.size();
  return child;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
