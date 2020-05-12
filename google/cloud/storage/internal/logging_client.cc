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

#include "google/cloud/storage/internal/logging_client.h"
#include "google/cloud/storage/internal/logging_resumable_upload_session.h"
#include "google/cloud/storage/internal/raw_client_wrapper_utils.h"
#include "google/cloud/log.h"
#include "absl/memory/memory.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

namespace {

using ::google::cloud::storage::internal::raw_client_wrapper_utils::Signature;

/**
 * Logs the input and results of each `RawClient` operation.
 *
 * @tparam MemberFunction the signature of the member function.
 * @param client the storage::RawClient object to make the call through.
 * @param function the pointer to the member function to call.
 * @param request an initialized request parameter for the call.
 * @param error_message include this message in any exception or error log.
 * @return the result from making the call;
 */
template <typename MemberFunction>
static typename Signature<MemberFunction>::ReturnType MakeCall(
    RawClient& client, MemberFunction function,
    typename Signature<MemberFunction>::RequestType const& request,
    char const* context) {
  GCP_LOG(INFO) << context << "() << " << request;
  auto response = (client.*function)(request);
  if (response.ok()) {
    GCP_LOG(INFO) << context << "() >> payload={" << response.value() << "}";
  } else {
    GCP_LOG(INFO) << context << "() >> status={" << response.status() << "}";
  }
  return response;
}

/**
 * Calls a `RawClient` operation logging only the input.
 *
 * This is useful when the result is not something you can easily log, such as
 * a pointer of some kind.
 *
 * @tparam MemberFunction the signature of the member function.
 * @param client the storage::RawClient object to make the call through.
 * @param function the pointer to the member function to call.
 * @param request an initialized request parameter for the call.
 * @param error_message include this message in any exception or error log.
 * @return the result from making the call;
 */
template <typename MemberFunction>
static typename Signature<MemberFunction>::ReturnType MakeCallNoResponseLogging(
    google::cloud::storage::internal::RawClient& client,
    MemberFunction function,
    typename Signature<MemberFunction>::RequestType const& request,
    char const* context) {
  GCP_LOG(INFO) << context << "() << " << request;
  return (client.*function)(request);
}
}  // namespace

LoggingClient::LoggingClient(std::shared_ptr<RawClient> client)
    : client_(std::move(client)) {}

ClientOptions const& LoggingClient::client_options() const {
  return client_->client_options();
}

StatusOr<ListBucketsResponse> LoggingClient::ListBuckets(
    ListBucketsRequest const& request) {
  return MakeCall(*client_, &RawClient::ListBuckets, request, __func__);
}

StatusOr<BucketMetadata> LoggingClient::CreateBucket(
    CreateBucketRequest const& request) {
  return MakeCall(*client_, &RawClient::CreateBucket, request, __func__);
}

StatusOr<BucketMetadata> LoggingClient::GetBucketMetadata(
    GetBucketMetadataRequest const& request) {
  return MakeCall(*client_, &RawClient::GetBucketMetadata, request, __func__);
}

StatusOr<EmptyResponse> LoggingClient::DeleteBucket(
    DeleteBucketRequest const& request) {
  return MakeCall(*client_, &RawClient::DeleteBucket, request, __func__);
}

StatusOr<BucketMetadata> LoggingClient::UpdateBucket(
    UpdateBucketRequest const& request) {
  return MakeCall(*client_, &RawClient::UpdateBucket, request, __func__);
}

StatusOr<BucketMetadata> LoggingClient::PatchBucket(
    PatchBucketRequest const& request) {
  return MakeCall(*client_, &RawClient::PatchBucket, request, __func__);
}

StatusOr<IamPolicy> LoggingClient::GetBucketIamPolicy(
    GetBucketIamPolicyRequest const& request) {
  return MakeCall(*client_, &RawClient::GetBucketIamPolicy, request, __func__);
}

StatusOr<NativeIamPolicy> LoggingClient::GetNativeBucketIamPolicy(
    GetBucketIamPolicyRequest const& request) {
  return MakeCall(*client_, &RawClient::GetNativeBucketIamPolicy, request,
                  __func__);
}

StatusOr<IamPolicy> LoggingClient::SetBucketIamPolicy(
    SetBucketIamPolicyRequest const& request) {
  return MakeCall(*client_, &RawClient::SetBucketIamPolicy, request, __func__);
}

StatusOr<NativeIamPolicy> LoggingClient::SetNativeBucketIamPolicy(
    SetNativeBucketIamPolicyRequest const& request) {
  return MakeCall(*client_, &RawClient::SetNativeBucketIamPolicy, request,
                  __func__);
}

StatusOr<TestBucketIamPermissionsResponse>
LoggingClient::TestBucketIamPermissions(
    TestBucketIamPermissionsRequest const& request) {
  return MakeCall(*client_, &RawClient::TestBucketIamPermissions, request,
                  __func__);
}

StatusOr<BucketMetadata> LoggingClient::LockBucketRetentionPolicy(
    LockBucketRetentionPolicyRequest const& request) {
  return MakeCall(*client_, &RawClient::LockBucketRetentionPolicy, request,
                  __func__);
}

StatusOr<ObjectMetadata> LoggingClient::InsertObjectMedia(
    InsertObjectMediaRequest const& request) {
  return MakeCall(*client_, &RawClient::InsertObjectMedia, request, __func__);
}

StatusOr<ObjectMetadata> LoggingClient::CopyObject(
    CopyObjectRequest const& request) {
  return MakeCall(*client_, &RawClient::CopyObject, request, __func__);
}

StatusOr<ObjectMetadata> LoggingClient::GetObjectMetadata(
    GetObjectMetadataRequest const& request) {
  return MakeCall(*client_, &RawClient::GetObjectMetadata, request, __func__);
}

StatusOr<std::unique_ptr<ObjectReadSource>> LoggingClient::ReadObject(
    ReadObjectRangeRequest const& request) {
  return MakeCallNoResponseLogging(*client_, &RawClient::ReadObject, request,
                                   __func__);
}

StatusOr<ListObjectsResponse> LoggingClient::ListObjects(
    ListObjectsRequest const& request) {
  return MakeCall(*client_, &RawClient::ListObjects, request, __func__);
}

StatusOr<EmptyResponse> LoggingClient::DeleteObject(
    DeleteObjectRequest const& request) {
  return MakeCall(*client_, &RawClient::DeleteObject, request, __func__);
}

StatusOr<ObjectMetadata> LoggingClient::UpdateObject(
    UpdateObjectRequest const& request) {
  return MakeCall(*client_, &RawClient::UpdateObject, request, __func__);
}

StatusOr<ObjectMetadata> LoggingClient::PatchObject(
    PatchObjectRequest const& request) {
  return MakeCall(*client_, &RawClient::PatchObject, request, __func__);
}

StatusOr<ObjectMetadata> LoggingClient::ComposeObject(
    ComposeObjectRequest const& request) {
  return MakeCall(*client_, &RawClient::ComposeObject, request, __func__);
}

StatusOr<RewriteObjectResponse> LoggingClient::RewriteObject(
    RewriteObjectRequest const& request) {
  return MakeCall(*client_, &RawClient::RewriteObject, request, __func__);
}

StatusOr<std::unique_ptr<ResumableUploadSession>>
LoggingClient::CreateResumableSession(ResumableUploadRequest const& request) {
  auto result = MakeCallNoResponseLogging(
      *client_, &RawClient::CreateResumableSession, request, __func__);
  if (!result.ok()) {
    GCP_LOG(INFO) << __func__ << "() >> status={" << result.status() << "}";
    return std::move(result).status();
  }
  return std::unique_ptr<ResumableUploadSession>(
      absl::make_unique<LoggingResumableUploadSession>(
          std::move(result).value()));
}

StatusOr<std::unique_ptr<ResumableUploadSession>>
LoggingClient::RestoreResumableSession(std::string const& request) {
  return MakeCallNoResponseLogging(
      *client_, &RawClient::RestoreResumableSession, request, __func__);
}

StatusOr<ListBucketAclResponse> LoggingClient::ListBucketAcl(
    ListBucketAclRequest const& request) {
  return MakeCall(*client_, &RawClient::ListBucketAcl, request, __func__);
}

StatusOr<BucketAccessControl> LoggingClient::GetBucketAcl(
    GetBucketAclRequest const& request) {
  return MakeCall(*client_, &RawClient::GetBucketAcl, request, __func__);
}

StatusOr<BucketAccessControl> LoggingClient::CreateBucketAcl(
    CreateBucketAclRequest const& request) {
  return MakeCall(*client_, &RawClient::CreateBucketAcl, request, __func__);
}

StatusOr<EmptyResponse> LoggingClient::DeleteBucketAcl(
    DeleteBucketAclRequest const& request) {
  return MakeCall(*client_, &RawClient::DeleteBucketAcl, request, __func__);
}

StatusOr<BucketAccessControl> LoggingClient::UpdateBucketAcl(
    UpdateBucketAclRequest const& request) {
  return MakeCall(*client_, &RawClient::UpdateBucketAcl, request, __func__);
}

StatusOr<BucketAccessControl> LoggingClient::PatchBucketAcl(
    PatchBucketAclRequest const& request) {
  return MakeCall(*client_, &RawClient::PatchBucketAcl, request, __func__);
}

StatusOr<ListObjectAclResponse> LoggingClient::ListObjectAcl(
    ListObjectAclRequest const& request) {
  return MakeCall(*client_, &RawClient::ListObjectAcl, request, __func__);
}

StatusOr<ObjectAccessControl> LoggingClient::CreateObjectAcl(
    CreateObjectAclRequest const& request) {
  return MakeCall(*client_, &RawClient::CreateObjectAcl, request, __func__);
}

StatusOr<EmptyResponse> LoggingClient::DeleteObjectAcl(
    DeleteObjectAclRequest const& request) {
  return MakeCall(*client_, &RawClient::DeleteObjectAcl, request, __func__);
}

StatusOr<ObjectAccessControl> LoggingClient::GetObjectAcl(
    GetObjectAclRequest const& request) {
  return MakeCall(*client_, &RawClient::GetObjectAcl, request, __func__);
}

StatusOr<ObjectAccessControl> LoggingClient::UpdateObjectAcl(
    UpdateObjectAclRequest const& request) {
  return MakeCall(*client_, &RawClient::UpdateObjectAcl, request, __func__);
}

StatusOr<ObjectAccessControl> LoggingClient::PatchObjectAcl(
    PatchObjectAclRequest const& request) {
  return MakeCall(*client_, &RawClient::PatchObjectAcl, request, __func__);
}

StatusOr<ListDefaultObjectAclResponse> LoggingClient::ListDefaultObjectAcl(
    ListDefaultObjectAclRequest const& request) {
  return MakeCall(*client_, &RawClient::ListDefaultObjectAcl, request,
                  __func__);
}

StatusOr<ObjectAccessControl> LoggingClient::CreateDefaultObjectAcl(
    CreateDefaultObjectAclRequest const& request) {
  return MakeCall(*client_, &RawClient::CreateDefaultObjectAcl, request,
                  __func__);
}

StatusOr<EmptyResponse> LoggingClient::DeleteDefaultObjectAcl(
    DeleteDefaultObjectAclRequest const& request) {
  return MakeCall(*client_, &RawClient::DeleteDefaultObjectAcl, request,
                  __func__);
}

StatusOr<ObjectAccessControl> LoggingClient::GetDefaultObjectAcl(
    GetDefaultObjectAclRequest const& request) {
  return MakeCall(*client_, &RawClient::GetDefaultObjectAcl, request, __func__);
}

StatusOr<ObjectAccessControl> LoggingClient::UpdateDefaultObjectAcl(
    UpdateDefaultObjectAclRequest const& request) {
  return MakeCall(*client_, &RawClient::UpdateDefaultObjectAcl, request,
                  __func__);
}

StatusOr<ObjectAccessControl> LoggingClient::PatchDefaultObjectAcl(
    PatchDefaultObjectAclRequest const& request) {
  return MakeCall(*client_, &RawClient::PatchDefaultObjectAcl, request,
                  __func__);
}

StatusOr<ServiceAccount> LoggingClient::GetServiceAccount(
    GetProjectServiceAccountRequest const& request) {
  return MakeCall(*client_, &RawClient::GetServiceAccount, request, __func__);
}

StatusOr<ListHmacKeysResponse> LoggingClient::ListHmacKeys(
    ListHmacKeysRequest const& request) {
  return MakeCall(*client_, &RawClient::ListHmacKeys, request, __func__);
}

StatusOr<CreateHmacKeyResponse> LoggingClient::CreateHmacKey(
    CreateHmacKeyRequest const& request) {
  return MakeCall(*client_, &RawClient::CreateHmacKey, request, __func__);
}

StatusOr<EmptyResponse> LoggingClient::DeleteHmacKey(
    DeleteHmacKeyRequest const& request) {
  return MakeCall(*client_, &RawClient::DeleteHmacKey, request, __func__);
}

StatusOr<HmacKeyMetadata> LoggingClient::GetHmacKey(
    GetHmacKeyRequest const& request) {
  return MakeCall(*client_, &RawClient::GetHmacKey, request, __func__);
}

StatusOr<HmacKeyMetadata> LoggingClient::UpdateHmacKey(
    UpdateHmacKeyRequest const& request) {
  return MakeCall(*client_, &RawClient::UpdateHmacKey, request, __func__);
}

StatusOr<SignBlobResponse> LoggingClient::SignBlob(
    SignBlobRequest const& request) {
  return MakeCall(*client_, &RawClient::SignBlob, request, __func__);
}

StatusOr<ListNotificationsResponse> LoggingClient::ListNotifications(
    ListNotificationsRequest const& request) {
  return MakeCall(*client_, &RawClient::ListNotifications, request, __func__);
}

StatusOr<NotificationMetadata> LoggingClient::CreateNotification(
    CreateNotificationRequest const& request) {
  return MakeCall(*client_, &RawClient::CreateNotification, request, __func__);
}

StatusOr<NotificationMetadata> LoggingClient::GetNotification(
    GetNotificationRequest const& request) {
  return MakeCall(*client_, &RawClient::GetNotification, request, __func__);
}

StatusOr<EmptyResponse> LoggingClient::DeleteNotification(
    DeleteNotificationRequest const& request) {
  return MakeCall(*client_, &RawClient::DeleteNotification, request, __func__);
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
