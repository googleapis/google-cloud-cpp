// Copyright 2018 Google LLC
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

#include "google/cloud/storage/internal/retry_client.h"
#include "google/cloud/internal/make_unique.h"
#include "google/cloud/storage/internal/raw_client_wrapper_utils.h"
#include "google/cloud/storage/internal/retry_resumable_upload_session.h"
#include <sstream>
#include <thread>

// Define the defaults using a pre-processor macro, this allows the application
// developers to change the defaults for their application by compiling with
// different values.
#ifndef STORAGE_CLIENT_DEFAULT_MAXIMUM_RETRY_PERIOD
#define STORAGE_CLIENT_DEFAULT_MAXIMUM_RETRY_PERIOD std::chrono::minutes(15)
#endif  // STORAGE_CLIENT_DEFAULT_MAXIMUM_RETRY_PERIOD

#ifndef STORAGE_CLIENT_DEFAULT_INITIAL_BACKOFF_DELAY
#define STORAGE_CLIENT_DEFAULT_INITIAL_BACKOFF_DELAY \
  std::chrono::milliseconds(10)
#endif  // STORAGE_CLIENT_DEFAULT_INITIAL_BACKOFF_DELAY

#ifndef STORAGE_CLIENT_DEFAULT_MAXIMUM_BACKOFF_DELAY
#define STORAGE_CLIENT_DEFAULT_MAXIMUM_BACKOFF_DELAY std::chrono::minutes(5)
#endif  // STORAGE_CLIENT_DEFAULT_MAXIMUM_BACKOFF_DELAY

#ifndef STORAGE_CLIENT_DEFAULT_BACKOFF_SCALING
#define STORAGE_CLIENT_DEFAULT_BACKOFF_SCALING 2.0
#endif  //  STORAGE_CLIENT_DEFAULT_BACKOFF_SCALING

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

using ::google::cloud::storage::internal::raw_client_wrapper_utils::
    CheckSignature;

/**
 * Calls a client operation with retries borrowing the RPC policies.
 *
 * @tparam MemberFunction the signature of the member function.
 * @param client the storage::Client object to make the call through.
 * @param retry_policy the policy controlling what failures are retryable, and
 *     for how long we can retry
 * @param backoff_policy the policy controlling how long to wait before
 *     retrying.
 * @param function the pointer to the member function to call.
 * @param request an initialized request parameter for the call.
 * @param error_message include this message in any exception or error log.
 * @return the result from making the call;
 * @throw std::exception with a description of the last error.
 */
template <typename MemberFunction>
typename std::enable_if<
    CheckSignature<MemberFunction>::value,
    typename CheckSignature<MemberFunction>::ReturnType>::type
MakeCall(RetryPolicy& retry_policy, BackoffPolicy& backoff_policy,
         bool is_idempotent, RawClient& client, MemberFunction function,
         typename CheckSignature<MemberFunction>::RequestType const& request,
         char const* error_message) {
  Status last_status;
  auto error = [&last_status](std::string const& msg) {
    return Status(last_status.code(), msg);
  };

  while (!retry_policy.IsExhausted()) {
    auto result = (client.*function)(request);
    if (result.ok()) {
      return result;
    }
    last_status = std::move(result).status();
    if (!is_idempotent) {
      std::ostringstream os;
      os << "Error in non-idempotent operation " << error_message << ": "
         << last_status;
      return error(std::move(os).str());
    }
    if (!retry_policy.OnFailure(last_status)) {
      if (!retry_policy.IsExhausted()) {
        // The last error cannot be retried, but it is not because the retry
        // policy is exhausted, we call these "permanent errors", and they
        // get a special message.
        std::ostringstream os;
        os << "Permanent error in " << error_message << ": " << last_status;
        return error(std::move(os).str());
      }
      // Exit the loop immediately instead of sleeping before trying again.
      break;
    }
    auto delay = backoff_policy.OnCompletion();
    std::this_thread::sleep_for(delay);
  }
  std::ostringstream os;
  os << "Retry policy exhausted in " << error_message << ": " << last_status;
  return error(std::move(os).str());
}
}  // namespace

RetryClient::RetryClient(std::shared_ptr<RawClient> client, DefaultPolicies)
    : client_(std::move(client)) {
  retry_policy_ =
      LimitedTimeRetryPolicy(STORAGE_CLIENT_DEFAULT_MAXIMUM_RETRY_PERIOD)
          .clone();
  backoff_policy_ =
      ExponentialBackoffPolicy(STORAGE_CLIENT_DEFAULT_INITIAL_BACKOFF_DELAY,
                               STORAGE_CLIENT_DEFAULT_MAXIMUM_BACKOFF_DELAY,
                               STORAGE_CLIENT_DEFAULT_BACKOFF_SCALING)
          .clone();
  idempotency_policy_ = AlwaysRetryIdempotencyPolicy().clone();
}

ClientOptions const& RetryClient::client_options() const {
  return client_->client_options();
}

StatusOr<ListBucketsResponse> RetryClient::ListBuckets(
    ListBucketsRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::ListBuckets, request, __func__);
}

StatusOr<BucketMetadata> RetryClient::CreateBucket(
    CreateBucketRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::CreateBucket, request, __func__);
}

StatusOr<BucketMetadata> RetryClient::GetBucketMetadata(
    GetBucketMetadataRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::GetBucketMetadata, request, __func__);
}

StatusOr<EmptyResponse> RetryClient::DeleteBucket(
    DeleteBucketRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::DeleteBucket, request, __func__);
}

StatusOr<BucketMetadata> RetryClient::UpdateBucket(
    UpdateBucketRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::UpdateBucket, request, __func__);
}

StatusOr<BucketMetadata> RetryClient::PatchBucket(
    PatchBucketRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::PatchBucket, request, __func__);
}

StatusOr<IamPolicy> RetryClient::GetBucketIamPolicy(
    GetBucketIamPolicyRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::GetBucketIamPolicy, request, __func__);
}

StatusOr<IamPolicy> RetryClient::SetBucketIamPolicy(
    SetBucketIamPolicyRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::SetBucketIamPolicy, request, __func__);
}

StatusOr<TestBucketIamPermissionsResponse>
RetryClient::TestBucketIamPermissions(
    TestBucketIamPermissionsRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::TestBucketIamPermissions, request, __func__);
}

StatusOr<BucketMetadata> RetryClient::LockBucketRetentionPolicy(
    LockBucketRetentionPolicyRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::LockBucketRetentionPolicy, request, __func__);
}

StatusOr<ObjectMetadata> RetryClient::InsertObjectMedia(
    InsertObjectMediaRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::InsertObjectMedia, request, __func__);
}

StatusOr<ObjectMetadata> RetryClient::CopyObject(
    CopyObjectRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::CopyObject, request, __func__);
}

StatusOr<ObjectMetadata> RetryClient::GetObjectMetadata(
    GetObjectMetadataRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::GetObjectMetadata, request, __func__);
}

StatusOr<std::unique_ptr<ObjectReadStreambuf>> RetryClient::ReadObject(
    ReadObjectRangeRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::ReadObject, request, __func__);
}

StatusOr<std::unique_ptr<ObjectWriteStreambuf>> RetryClient::WriteObject(
    internal::InsertObjectStreamingRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::WriteObject, request, __func__);
}

StatusOr<ListObjectsResponse> RetryClient::ListObjects(
    ListObjectsRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::ListObjects, request, __func__);
}

StatusOr<EmptyResponse> RetryClient::DeleteObject(
    DeleteObjectRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::DeleteObject, request, __func__);
}

StatusOr<ObjectMetadata> RetryClient::UpdateObject(
    UpdateObjectRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::UpdateObject, request, __func__);
}

StatusOr<ObjectMetadata> RetryClient::PatchObject(
    PatchObjectRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::PatchObject, request, __func__);
}

StatusOr<ObjectMetadata> RetryClient::ComposeObject(
    ComposeObjectRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::ComposeObject, request, __func__);
}

StatusOr<RewriteObjectResponse> RetryClient::RewriteObject(
    RewriteObjectRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::RewriteObject, request, __func__);
}

StatusOr<std::unique_ptr<ResumableUploadSession>>
RetryClient::CreateResumableSession(ResumableUploadRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  auto result =
      MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
               &RawClient::CreateResumableSession, request, __func__);
  if (!result.ok()) {
    return result;
  }

  return std::unique_ptr<ResumableUploadSession>(
      google::cloud::internal::make_unique<RetryResumableUploadSession>(
          std::move(result).value(), std::move(retry_policy),
          std::move(backoff_policy)));
}

StatusOr<std::unique_ptr<ResumableUploadSession>>
RetryClient::RestoreResumableSession(std::string const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = true;
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::RestoreResumableSession, request, __func__);
}

StatusOr<ListBucketAclResponse> RetryClient::ListBucketAcl(
    ListBucketAclRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::ListBucketAcl, request, __func__);
}

StatusOr<BucketAccessControl> RetryClient::GetBucketAcl(
    GetBucketAclRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::GetBucketAcl, request, __func__);
}

StatusOr<BucketAccessControl> RetryClient::CreateBucketAcl(
    CreateBucketAclRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::CreateBucketAcl, request, __func__);
}

StatusOr<EmptyResponse> RetryClient::DeleteBucketAcl(
    DeleteBucketAclRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::DeleteBucketAcl, request, __func__);
}

StatusOr<ListObjectAclResponse> RetryClient::ListObjectAcl(
    ListObjectAclRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::ListObjectAcl, request, __func__);
}

StatusOr<BucketAccessControl> RetryClient::UpdateBucketAcl(
    UpdateBucketAclRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::UpdateBucketAcl, request, __func__);
}

StatusOr<BucketAccessControl> RetryClient::PatchBucketAcl(
    PatchBucketAclRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::PatchBucketAcl, request, __func__);
}

StatusOr<ObjectAccessControl> RetryClient::CreateObjectAcl(
    CreateObjectAclRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::CreateObjectAcl, request, __func__);
}

StatusOr<EmptyResponse> RetryClient::DeleteObjectAcl(
    DeleteObjectAclRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::DeleteObjectAcl, request, __func__);
}

StatusOr<ObjectAccessControl> RetryClient::GetObjectAcl(
    GetObjectAclRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::GetObjectAcl, request, __func__);
}

StatusOr<ObjectAccessControl> RetryClient::UpdateObjectAcl(
    UpdateObjectAclRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::UpdateObjectAcl, request, __func__);
}

StatusOr<ObjectAccessControl> RetryClient::PatchObjectAcl(
    PatchObjectAclRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::PatchObjectAcl, request, __func__);
}

StatusOr<ListDefaultObjectAclResponse> RetryClient::ListDefaultObjectAcl(
    ListDefaultObjectAclRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::ListDefaultObjectAcl, request, __func__);
}

StatusOr<ObjectAccessControl> RetryClient::CreateDefaultObjectAcl(
    CreateDefaultObjectAclRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::CreateDefaultObjectAcl, request, __func__);
}

StatusOr<EmptyResponse> RetryClient::DeleteDefaultObjectAcl(
    DeleteDefaultObjectAclRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::DeleteDefaultObjectAcl, request, __func__);
}

StatusOr<ObjectAccessControl> RetryClient::GetDefaultObjectAcl(
    GetDefaultObjectAclRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::GetDefaultObjectAcl, request, __func__);
}

StatusOr<ObjectAccessControl> RetryClient::UpdateDefaultObjectAcl(
    UpdateDefaultObjectAclRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::UpdateDefaultObjectAcl, request, __func__);
}

StatusOr<ObjectAccessControl> RetryClient::PatchDefaultObjectAcl(
    PatchDefaultObjectAclRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::PatchDefaultObjectAcl, request, __func__);
}

StatusOr<ServiceAccount> RetryClient::GetServiceAccount(
    GetProjectServiceAccountRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::GetServiceAccount, request, __func__);
}

StatusOr<ListNotificationsResponse> RetryClient::ListNotifications(
    ListNotificationsRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::ListNotifications, request, __func__);
}

StatusOr<NotificationMetadata> RetryClient::CreateNotification(
    CreateNotificationRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::CreateNotification, request, __func__);
}

StatusOr<NotificationMetadata> RetryClient::GetNotification(
    GetNotificationRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::GetNotification, request, __func__);
}

StatusOr<EmptyResponse> RetryClient::DeleteNotification(
    DeleteNotificationRequest const& request) {
  auto retry_policy = retry_policy_->clone();
  auto backoff_policy = backoff_policy_->clone();
  auto is_idempotent = idempotency_policy_->IsIdempotent(request);
  return MakeCall(*retry_policy, *backoff_policy, is_idempotent, *client_,
                  &RawClient::DeleteNotification, request, __func__);
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
