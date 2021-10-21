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
#include "google/cloud/storage/internal/raw_client_wrapper_utils.h"
#include "google/cloud/storage/internal/retry_object_read_source.h"
#include "google/cloud/storage/internal/retry_resumable_upload_session.h"
#include "google/cloud/internal/retry_policy.h"
#include "absl/memory/memory.h"
#include <sstream>
#include <thread>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

using ::google::cloud::internal::Idempotency;
using ::google::cloud::storage::internal::raw_client_wrapper_utils::Signature;

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
typename Signature<MemberFunction>::ReturnType MakeCall(
    RetryPolicy& retry_policy, BackoffPolicy& backoff_policy,
    Idempotency idempotency, RawClient& client, MemberFunction function,
    typename Signature<MemberFunction>::RequestType const& request,
    char const* error_message) {
  Status last_status(StatusCode::kDeadlineExceeded,
                     "Retry policy exhausted before first attempt was made.");
  auto error = [&last_status](std::string const& msg) {
    return Status(last_status.code(), msg);
  };

  while (!retry_policy.IsExhausted()) {
    auto result = (client.*function)(request);
    if (result.ok()) {
      return result;
    }
    last_status = std::move(result).status();
    if (idempotency == Idempotency::kNonIdempotent) {
      std::ostringstream os;
      os << "Error in non-idempotent operation " << error_message << ": "
         << last_status.message();
      return error(std::move(os).str());
    }
    if (!retry_policy.OnFailure(last_status)) {
      if (internal::StatusTraits::IsPermanentFailure(last_status)) {
        // The last error cannot be retried, but it is not because the retry
        // policy is exhausted, we call these "permanent errors", and they
        // get a special message.
        std::ostringstream os;
        os << "Permanent error in " << error_message << ": "
           << last_status.message();
        return error(std::move(os).str());
      }
      // Exit the loop immediately instead of sleeping before trying again.
      break;
    }
    auto delay = backoff_policy.OnCompletion();
    std::this_thread::sleep_for(delay);
  }
  std::ostringstream os;
  os << "Retry policy exhausted in " << error_message << ": "
     << last_status.message();
  return error(std::move(os).str());
}
}  // namespace

RetryClient::RetryClient(std::shared_ptr<RawClient> client,
                         Options const& options)
    : client_(std::move(client)),
      retry_policy_prototype_(options.get<RetryPolicyOption>()->clone()),
      backoff_policy_prototype_(options.get<BackoffPolicyOption>()->clone()),
      idempotency_policy_(options.get<IdempotencyPolicyOption>()->clone()) {}

ClientOptions const& RetryClient::client_options() const {
  return client_->client_options();
}

StatusOr<ListBucketsResponse> RetryClient::ListBuckets(
    ListBucketsRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::ListBuckets, request, __func__);
}

StatusOr<BucketMetadata> RetryClient::CreateBucket(
    CreateBucketRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::CreateBucket, request, __func__);
}

StatusOr<BucketMetadata> RetryClient::GetBucketMetadata(
    GetBucketMetadataRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::GetBucketMetadata, request, __func__);
}

StatusOr<EmptyResponse> RetryClient::DeleteBucket(
    DeleteBucketRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto idempotency = idempotency_policy_->IsIdempotent(request)
                         ? Idempotency::kIdempotent
                         : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::DeleteBucket, request, __func__);
}

StatusOr<BucketMetadata> RetryClient::UpdateBucket(
    UpdateBucketRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::UpdateBucket, request, __func__);
}

StatusOr<BucketMetadata> RetryClient::PatchBucket(
    PatchBucketRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::PatchBucket, request, __func__);
}

StatusOr<IamPolicy> RetryClient::GetBucketIamPolicy(
    GetBucketIamPolicyRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::GetBucketIamPolicy, request, __func__);
}

StatusOr<NativeIamPolicy> RetryClient::GetNativeBucketIamPolicy(
    GetBucketIamPolicyRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::GetNativeBucketIamPolicy, request, __func__);
}

StatusOr<IamPolicy> RetryClient::SetBucketIamPolicy(
    SetBucketIamPolicyRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::SetBucketIamPolicy, request, __func__);
}

StatusOr<NativeIamPolicy> RetryClient::SetNativeBucketIamPolicy(
    SetNativeBucketIamPolicyRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::SetNativeBucketIamPolicy, request, __func__);
}

StatusOr<TestBucketIamPermissionsResponse>
RetryClient::TestBucketIamPermissions(
    TestBucketIamPermissionsRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::TestBucketIamPermissions, request, __func__);
}

StatusOr<BucketMetadata> RetryClient::LockBucketRetentionPolicy(
    LockBucketRetentionPolicyRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::LockBucketRetentionPolicy, request, __func__);
}

StatusOr<ObjectMetadata> RetryClient::InsertObjectMedia(
    InsertObjectMediaRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::InsertObjectMedia, request, __func__);
}

StatusOr<ObjectMetadata> RetryClient::CopyObject(
    CopyObjectRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::CopyObject, request, __func__);
}

StatusOr<ObjectMetadata> RetryClient::GetObjectMetadata(
    GetObjectMetadataRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::GetObjectMetadata, request, __func__);
}

StatusOr<std::unique_ptr<ObjectReadSource>> RetryClient::ReadObjectNotWrapped(
    ReadObjectRangeRequest const& request, RetryPolicy& retry_policy,
    BackoffPolicy& backoff_policy) {
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(retry_policy, backoff_policy, idempotency, *client_,
                  &RawClient::ReadObject, request, __func__);
}

StatusOr<std::unique_ptr<ObjectReadSource>> RetryClient::ReadObject(
    ReadObjectRangeRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto child = ReadObjectNotWrapped(request, *retry_policy, *backoff_policy);
  if (!child) {
    return child;
  }
  auto self = shared_from_this();
  return std::unique_ptr<ObjectReadSource>(new RetryObjectReadSource(
      self, request, *std::move(child), std::move(retry_policy),
      std::move(backoff_policy)));
}

StatusOr<ListObjectsResponse> RetryClient::ListObjects(
    ListObjectsRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::ListObjects, request, __func__);
}

StatusOr<EmptyResponse> RetryClient::DeleteObject(
    DeleteObjectRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::DeleteObject, request, __func__);
}

StatusOr<ObjectMetadata> RetryClient::UpdateObject(
    UpdateObjectRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::UpdateObject, request, __func__);
}

StatusOr<ObjectMetadata> RetryClient::PatchObject(
    PatchObjectRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::PatchObject, request, __func__);
}

StatusOr<ObjectMetadata> RetryClient::ComposeObject(
    ComposeObjectRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::ComposeObject, request, __func__);
}

StatusOr<RewriteObjectResponse> RetryClient::RewriteObject(
    RewriteObjectRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::RewriteObject, request, __func__);
}

StatusOr<std::unique_ptr<ResumableUploadSession>>
RetryClient::CreateResumableSession(ResumableUploadRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  auto result = MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                         &RawClient::CreateResumableSession, request, __func__);
  if (!result.ok()) {
    return result;
  }

  return std::unique_ptr<ResumableUploadSession>(
      absl::make_unique<RetryResumableUploadSession>(
          std::move(result).value(), std::move(retry_policy),
          std::move(backoff_policy)));
}

StatusOr<std::unique_ptr<ResumableUploadSession>>
RetryClient::RestoreResumableSession(std::string const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  return MakeCall(*retry_policy, *backoff_policy, Idempotency::kIdempotent,
                  *client_, &RawClient::RestoreResumableSession, request,
                  __func__);
}

StatusOr<EmptyResponse> RetryClient::DeleteResumableUpload(
    DeleteResumableUploadRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  return MakeCall(*retry_policy, *backoff_policy, Idempotency::kIdempotent,
                  *client_, &RawClient::DeleteResumableUpload, request,
                  __func__);
}

StatusOr<ListBucketAclResponse> RetryClient::ListBucketAcl(
    ListBucketAclRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::ListBucketAcl, request, __func__);
}

StatusOr<BucketAccessControl> RetryClient::GetBucketAcl(
    GetBucketAclRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::GetBucketAcl, request, __func__);
}

StatusOr<BucketAccessControl> RetryClient::CreateBucketAcl(
    CreateBucketAclRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::CreateBucketAcl, request, __func__);
}

StatusOr<EmptyResponse> RetryClient::DeleteBucketAcl(
    DeleteBucketAclRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::DeleteBucketAcl, request, __func__);
}

StatusOr<ListObjectAclResponse> RetryClient::ListObjectAcl(
    ListObjectAclRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::ListObjectAcl, request, __func__);
}

StatusOr<BucketAccessControl> RetryClient::UpdateBucketAcl(
    UpdateBucketAclRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::UpdateBucketAcl, request, __func__);
}

StatusOr<BucketAccessControl> RetryClient::PatchBucketAcl(
    PatchBucketAclRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::PatchBucketAcl, request, __func__);
}

StatusOr<ObjectAccessControl> RetryClient::CreateObjectAcl(
    CreateObjectAclRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::CreateObjectAcl, request, __func__);
}

StatusOr<EmptyResponse> RetryClient::DeleteObjectAcl(
    DeleteObjectAclRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::DeleteObjectAcl, request, __func__);
}

StatusOr<ObjectAccessControl> RetryClient::GetObjectAcl(
    GetObjectAclRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::GetObjectAcl, request, __func__);
}

StatusOr<ObjectAccessControl> RetryClient::UpdateObjectAcl(
    UpdateObjectAclRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::UpdateObjectAcl, request, __func__);
}

StatusOr<ObjectAccessControl> RetryClient::PatchObjectAcl(
    PatchObjectAclRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::PatchObjectAcl, request, __func__);
}

StatusOr<ListDefaultObjectAclResponse> RetryClient::ListDefaultObjectAcl(
    ListDefaultObjectAclRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::ListDefaultObjectAcl, request, __func__);
}

StatusOr<ObjectAccessControl> RetryClient::CreateDefaultObjectAcl(
    CreateDefaultObjectAclRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::CreateDefaultObjectAcl, request, __func__);
}

StatusOr<EmptyResponse> RetryClient::DeleteDefaultObjectAcl(
    DeleteDefaultObjectAclRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::DeleteDefaultObjectAcl, request, __func__);
}

StatusOr<ObjectAccessControl> RetryClient::GetDefaultObjectAcl(
    GetDefaultObjectAclRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::GetDefaultObjectAcl, request, __func__);
}

StatusOr<ObjectAccessControl> RetryClient::UpdateDefaultObjectAcl(
    UpdateDefaultObjectAclRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::UpdateDefaultObjectAcl, request, __func__);
}

StatusOr<ObjectAccessControl> RetryClient::PatchDefaultObjectAcl(
    PatchDefaultObjectAclRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::PatchDefaultObjectAcl, request, __func__);
}

StatusOr<ServiceAccount> RetryClient::GetServiceAccount(
    GetProjectServiceAccountRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::GetServiceAccount, request, __func__);
}

StatusOr<ListHmacKeysResponse> RetryClient::ListHmacKeys(
    ListHmacKeysRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::ListHmacKeys, request, __func__);
}

StatusOr<CreateHmacKeyResponse> RetryClient::CreateHmacKey(
    CreateHmacKeyRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::CreateHmacKey, request, __func__);
}

StatusOr<EmptyResponse> RetryClient::DeleteHmacKey(
    DeleteHmacKeyRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::DeleteHmacKey, request, __func__);
}

StatusOr<HmacKeyMetadata> RetryClient::GetHmacKey(
    GetHmacKeyRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::GetHmacKey, request, __func__);
}

StatusOr<HmacKeyMetadata> RetryClient::UpdateHmacKey(
    UpdateHmacKeyRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::UpdateHmacKey, request, __func__);
}

StatusOr<SignBlobResponse> RetryClient::SignBlob(
    SignBlobRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::SignBlob, request, __func__);
}

StatusOr<ListNotificationsResponse> RetryClient::ListNotifications(
    ListNotificationsRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::ListNotifications, request, __func__);
}

StatusOr<NotificationMetadata> RetryClient::CreateNotification(
    CreateNotificationRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::CreateNotification, request, __func__);
}

StatusOr<NotificationMetadata> RetryClient::GetNotification(
    GetNotificationRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::GetNotification, request, __func__);
}

StatusOr<EmptyResponse> RetryClient::DeleteNotification(
    DeleteNotificationRequest const& request) {
  auto retry_policy = retry_policy_prototype_->clone();
  auto backoff_policy = backoff_policy_prototype_->clone();
  auto const idempotency = idempotency_policy_->IsIdempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  return MakeCall(*retry_policy, *backoff_policy, idempotency, *client_,
                  &RawClient::DeleteNotification, request, __func__);
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
