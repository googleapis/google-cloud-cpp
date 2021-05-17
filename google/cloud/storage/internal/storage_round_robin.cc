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

#include "google/cloud/storage/internal/storage_round_robin.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

std::unique_ptr<StorageStub::ObjectMediaStream>
StorageRoundRobin::GetObjectMedia(
    std::unique_ptr<grpc::ClientContext> context,
    google::storage::v1::GetObjectMediaRequest const& request) {
  return Child()->GetObjectMedia(std::move(context), request);
}

std::unique_ptr<StorageStub::InsertStream> StorageRoundRobin::InsertObjectMedia(
    std::unique_ptr<grpc::ClientContext> context) {
  return Child()->InsertObjectMedia(std::move(context));
}

Status StorageRoundRobin::DeleteBucketAccessControl(
    grpc::ClientContext& context,
    google::storage::v1::DeleteBucketAccessControlRequest const& request) {
  return Child()->DeleteBucketAccessControl(context, request);
}

StatusOr<google::storage::v1::BucketAccessControl>
StorageRoundRobin::GetBucketAccessControl(
    grpc::ClientContext& context,
    google::storage::v1::GetBucketAccessControlRequest const& request) {
  return Child()->GetBucketAccessControl(context, request);
}

StatusOr<google::storage::v1::BucketAccessControl>
StorageRoundRobin::InsertBucketAccessControl(
    grpc::ClientContext& context,
    google::storage::v1::InsertBucketAccessControlRequest const& request) {
  return Child()->InsertBucketAccessControl(context, request);
}

StatusOr<google::storage::v1::ListBucketAccessControlsResponse>
StorageRoundRobin::ListBucketAccessControls(
    grpc::ClientContext& context,
    google::storage::v1::ListBucketAccessControlsRequest const& request) {
  return Child()->ListBucketAccessControls(context, request);
}

StatusOr<google::storage::v1::BucketAccessControl>
StorageRoundRobin::UpdateBucketAccessControl(
    grpc::ClientContext& context,
    google::storage::v1::UpdateBucketAccessControlRequest const& request) {
  return Child()->UpdateBucketAccessControl(context, request);
}

Status StorageRoundRobin::DeleteBucket(
    grpc::ClientContext& context,
    google::storage::v1::DeleteBucketRequest const& request) {
  return Child()->DeleteBucket(context, request);
}

StatusOr<google::storage::v1::Bucket> StorageRoundRobin::GetBucket(
    grpc::ClientContext& context,
    google::storage::v1::GetBucketRequest const& request) {
  return Child()->GetBucket(context, request);
}

StatusOr<google::storage::v1::Bucket> StorageRoundRobin::InsertBucket(
    grpc::ClientContext& context,
    google::storage::v1::InsertBucketRequest const& request) {
  return Child()->InsertBucket(context, request);
}

StatusOr<google::storage::v1::ListBucketsResponse>
StorageRoundRobin::ListBuckets(
    grpc::ClientContext& context,
    google::storage::v1::ListBucketsRequest const& request) {
  return Child()->ListBuckets(context, request);
}

StatusOr<google::iam::v1::Policy> StorageRoundRobin::GetBucketIamPolicy(
    grpc::ClientContext& context,
    google::storage::v1::GetIamPolicyRequest const& request) {
  return Child()->GetBucketIamPolicy(context, request);
}

StatusOr<google::iam::v1::Policy> StorageRoundRobin::SetBucketIamPolicy(
    grpc::ClientContext& context,
    google::storage::v1::SetIamPolicyRequest const& request) {
  return Child()->SetBucketIamPolicy(context, request);
}

StatusOr<google::iam::v1::TestIamPermissionsResponse>
StorageRoundRobin::TestBucketIamPermissions(
    grpc::ClientContext& context,
    google::storage::v1::TestIamPermissionsRequest const& request) {
  return Child()->TestBucketIamPermissions(context, request);
}

StatusOr<google::storage::v1::Bucket> StorageRoundRobin::UpdateBucket(
    grpc::ClientContext& context,
    google::storage::v1::UpdateBucketRequest const& request) {
  return Child()->UpdateBucket(context, request);
}

Status StorageRoundRobin::DeleteDefaultObjectAccessControl(
    grpc::ClientContext& context,
    google::storage::v1::DeleteDefaultObjectAccessControlRequest const&
        request) {
  return Child()->DeleteDefaultObjectAccessControl(context, request);
}

StatusOr<google::storage::v1::ObjectAccessControl>
StorageRoundRobin::GetDefaultObjectAccessControl(
    grpc::ClientContext& context,
    google::storage::v1::GetDefaultObjectAccessControlRequest const& request) {
  return Child()->GetDefaultObjectAccessControl(context, request);
}

StatusOr<google::storage::v1::ObjectAccessControl>
StorageRoundRobin::InsertDefaultObjectAccessControl(
    grpc::ClientContext& context,
    google::storage::v1::InsertDefaultObjectAccessControlRequest const&
        request) {
  return Child()->InsertDefaultObjectAccessControl(context, request);
}

StatusOr<google::storage::v1::ListObjectAccessControlsResponse>
StorageRoundRobin::ListDefaultObjectAccessControls(
    grpc::ClientContext& context,
    google::storage::v1::ListDefaultObjectAccessControlsRequest const&
        request) {
  return Child()->ListDefaultObjectAccessControls(context, request);
}

StatusOr<google::storage::v1::ObjectAccessControl>
StorageRoundRobin::UpdateDefaultObjectAccessControl(
    grpc::ClientContext& context,
    google::storage::v1::UpdateDefaultObjectAccessControlRequest const&
        request) {
  return Child()->UpdateDefaultObjectAccessControl(context, request);
}

Status StorageRoundRobin::DeleteNotification(
    grpc::ClientContext& context,
    google::storage::v1::DeleteNotificationRequest const& request) {
  return Child()->DeleteNotification(context, request);
}

StatusOr<google::storage::v1::Notification> StorageRoundRobin::GetNotification(
    grpc::ClientContext& context,
    google::storage::v1::GetNotificationRequest const& request) {
  return Child()->GetNotification(context, request);
}

StatusOr<google::storage::v1::Notification>
StorageRoundRobin::InsertNotification(
    grpc::ClientContext& context,
    google::storage::v1::InsertNotificationRequest const& request) {
  return Child()->InsertNotification(context, request);
}

StatusOr<google::storage::v1::ListNotificationsResponse>
StorageRoundRobin::ListNotifications(
    grpc::ClientContext& context,
    google::storage::v1::ListNotificationsRequest const& request) {
  return Child()->ListNotifications(context, request);
}

Status StorageRoundRobin::DeleteObject(
    grpc::ClientContext& context,
    google::storage::v1::DeleteObjectRequest const& request) {
  return Child()->DeleteObject(context, request);
}

StatusOr<google::storage::v1::StartResumableWriteResponse>
StorageRoundRobin::StartResumableWrite(
    grpc::ClientContext& context,
    google::storage::v1::StartResumableWriteRequest const& request) {
  return Child()->StartResumableWrite(context, request);
}

StatusOr<google::storage::v1::QueryWriteStatusResponse>
StorageRoundRobin::QueryWriteStatus(
    grpc::ClientContext& context,
    google::storage::v1::QueryWriteStatusRequest const& request) {
  return Child()->QueryWriteStatus(context, request);
}

std::shared_ptr<StorageStub> StorageRoundRobin::Child() {
  std::lock_guard<std::mutex> lk(mu_);
  auto child = children_[current_];
  current_ = (current_ + 1) % children_.size();
  return child;
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
