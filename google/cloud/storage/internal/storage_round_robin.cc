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
#include "google/cloud/internal/async_connection_ready.h"
#include "google/cloud/log.h"

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

auto constexpr kRefreshPeriod = std::chrono::hours(1);

void StorageRoundRobin::StartRefreshLoop(
    google::cloud::CompletionQueue cq,  // NOLINT
    std::vector<std::shared_ptr<grpc::Channel>> channels) {
  std::unique_lock<std::mutex> lk(mu_);
  // This is purely defensive, we do not want the channels to change after the
  // refresh loop starts.
  if (!channels_.empty()) return;
  channels_ = std::move(channels);
  lk.unlock();
  // Break the ownership cycle.
  auto wcq = std::weak_ptr<google::cloud::internal::CompletionQueueImpl>(
      google::cloud::internal::GetCompletionQueueImpl(cq));
  for (std::size_t i = 0; i != channels_.size(); ++i) Refresh(i, wcq);
}

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

Status StorageRoundRobin::DeleteNotification(
    grpc::ClientContext& context,
    google::storage::v2::DeleteNotificationRequest const& request) {
  return Child()->DeleteNotification(context, request);
}

StatusOr<google::storage::v2::Notification> StorageRoundRobin::GetNotification(
    grpc::ClientContext& context,
    google::storage::v2::GetNotificationRequest const& request) {
  return Child()->GetNotification(context, request);
}

StatusOr<google::storage::v2::Notification>
StorageRoundRobin::CreateNotification(
    grpc::ClientContext& context,
    google::storage::v2::CreateNotificationRequest const& request) {
  return Child()->CreateNotification(context, request);
}

StatusOr<google::storage::v2::ListNotificationsResponse>
StorageRoundRobin::ListNotifications(
    grpc::ClientContext& context,
    google::storage::v2::ListNotificationsRequest const& request) {
  return Child()->ListNotifications(context, request);
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

StatusOr<google::storage::v2::CancelResumableWriteResponse>
StorageRoundRobin::CancelResumableWrite(
    grpc::ClientContext& context,
    google::storage::v2::CancelResumableWriteRequest const& request) {
  return Child()->CancelResumableWrite(context, request);
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

std::unique_ptr<::google::cloud::internal::AsyncStreamingWriteRpc<
    google::storage::v2::WriteObjectRequest,
    google::storage::v2::WriteObjectResponse>>
StorageRoundRobin::AsyncWriteObject(
    google::cloud::CompletionQueue const& cq,
    std::unique_ptr<grpc::ClientContext> context) {
  return Child()->AsyncWriteObject(cq, std::move(context));
}

std::shared_ptr<StorageStub> StorageRoundRobin::Child() {
  std::lock_guard<std::mutex> lk(mu_);
  auto child = children_[current_];
  current_ = (current_ + 1) % children_.size();
  return child;
}

void StorageRoundRobin::Refresh(
    std::size_t index,
    std::weak_ptr<google::cloud::internal::CompletionQueueImpl> wcq) {
  auto cq = wcq.lock();
  if (!cq) return;
  auto deadline = std::chrono::system_clock::now() + kRefreshPeriod;
  // An invalid index, stop the loop.  There is no need to check lock the mutex,
  // as the channels do not change after the class is initialized.
  if (index >= channels_.size()) return;
  GCP_LOG(INFO) << "Refreshing channel [" << index << "]";
  (void)google::cloud::internal::NotifyOnStateChange::Start(
      std::move(cq), channels_.at(index), deadline)
      .then(
          [index, wcq = std::move(wcq), weak = WeakFromThis()](future<bool> f) {
            if (auto self = weak.lock()) self->OnRefresh(index, wcq, f.get());
          });
}

void StorageRoundRobin::OnRefresh(
    std::size_t index,
    std::weak_ptr<google::cloud::internal::CompletionQueueImpl> wcq, bool ok) {
  // The CQ is shutting down, or the channel is shutdown, stop the loop.
  if (!ok) return;
  Refresh(index, std::move(wcq));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
