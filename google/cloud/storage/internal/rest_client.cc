// Copyright 2022 Google LLC
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

#include "google/cloud/storage/internal/rest_client.h"
#include "google/cloud/storage/internal/bucket_access_control_parser.h"
#include "google/cloud/storage/internal/bucket_metadata_parser.h"
#include "google/cloud/storage/internal/bucket_requests.h"
#include "google/cloud/storage/internal/curl_request_builder.h"
#include "google/cloud/storage/internal/hmac_key_metadata_parser.h"
#include "google/cloud/storage/internal/logging_client.h"
#include "google/cloud/storage/internal/notification_metadata_parser.h"
#include "google/cloud/storage/internal/object_access_control_parser.h"
#include "google/cloud/storage/internal/object_metadata_parser.h"
#include "google/cloud/storage/internal/object_read_streambuf.h"
#include "google/cloud/storage/internal/object_write_streambuf.h"
#include "google/cloud/storage/internal/rest_request_builder.h"
#include "google/cloud/storage/internal/service_account_parser.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/getenv.h"
#include "absl/memory/memory.h"
#include "absl/strings/match.h"
#include "absl/strings/strip.h"
#include <sstream>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

namespace rest = google::cloud::rest_internal;

template <typename ReturnType>
StatusOr<ReturnType> ParseFromRestResponse(
    StatusOr<std::unique_ptr<rest::RestResponse>> response) {
  if (!response.ok()) return std::move(response).status();
  if ((*response)->StatusCode() >= rest::kMinNotSuccess) {
    return rest::AsStatus(std::move(**response));
  }
  auto payload = rest::ReadAll(std::move(**response).ExtractPayload());
  if (!payload.ok()) return std::move(payload).status();
  return ReturnType::FromHttpResponse(*payload);
}

template <typename Parser>
auto CheckedFromString(StatusOr<std::unique_ptr<rest::RestResponse>> response)
    -> decltype(Parser::FromString(
        *(rest::ReadAll(std::move(**response).ExtractPayload())))) {
  if (!response.ok()) {
    return std::move(response).status();
  }
  if ((*response)->StatusCode() >= rest::kMinNotSuccess) {
    return rest::AsStatus(std::move(**response));
  }
  auto payload = rest::ReadAll(std::move(**response).ExtractPayload());
  if (!payload.ok()) return std::move(payload).status();
  return Parser::FromString(*payload);
}

StatusOr<EmptyResponse> ReturnEmptyResponse(
    StatusOr<std::unique_ptr<rest::RestResponse>> response) {
  if (!response.ok()) {
    return std::move(response).status();
  }
  if ((*response)->StatusCode() >= rest::kMinNotSuccess) {
    return rest::AsStatus(std::move(**response));
  }
  return EmptyResponse{};
}

template <typename ReturnType>
StatusOr<ReturnType> CreateFromJson(
    StatusOr<std::unique_ptr<rest::RestResponse>> response) {
  if (!response.ok()) return std::move(response).status();
  if ((*response)->StatusCode() >= rest::kMinNotSuccess) {
    return rest::AsStatus(std::move(**response));
  }
  auto payload = rest::ReadAll(std::move(**response).ExtractPayload());
  if (!payload.ok()) return std::move(payload).status();
  return ReturnType::CreateFromJson(*payload);
}

Status AddAuthorizationHeader(Options const& options,
                              RestRequestBuilder& builder) {
  auto auth_header =
      options.get<Oauth2CredentialsOption>()->AuthorizationHeader();
  if (!auth_header.ok()) {
    return std::move(auth_header).status();
  }
  builder.AddHeader("Authorization", std::string(absl::StripPrefix(
                                         *auth_header, "Authorization: ")));
  return {};
}

std::shared_ptr<RestClient> RestClient::Create(Options options) {
  std::unique_ptr<rest::RestClient> storage_rest_client;
  std::unique_ptr<rest::RestClient> iam_rest_client;
  auto storage_endpoint = RestEndpoint(options);
  auto iam_endpoint = IamRestEndpoint(options);

  if (!options.has<AuthorityOption>() &&
      absl::StrContains(storage_endpoint, "googleapis.com")) {
    auto storage_options = options;
    storage_options.set<AuthorityOption>("storage.googleapis.com");
    storage_rest_client = rest::MakePooledRestClient(
        storage_endpoint, std::move(storage_options));
  } else {
    storage_rest_client =
        rest::MakePooledRestClient(std::move(storage_endpoint), options);
  }

  if (!options.has<AuthorityOption>() &&
      absl::StrContains(iam_endpoint, "googleapis.com")) {
    auto iam_options = options;
    iam_options.set<AuthorityOption>("iamcredentials.googleapis.com");
    iam_rest_client =
        rest::MakePooledRestClient(iam_endpoint, std::move(iam_options));
  } else {
    iam_rest_client =
        rest::MakePooledRestClient(std::move(iam_endpoint), options);
  }

  std::shared_ptr<RawClient> curl_client =
      internal::CurlClient::Create(options);

  return std::shared_ptr<RestClient>(
      new RestClient(std::move(storage_rest_client), std::move(iam_rest_client),
                     std::move(curl_client), std::move(options)));
}

RestClient::RestClient(
    std::unique_ptr<google::cloud::rest_internal::RestClient>
        storage_rest_client,
    std::unique_ptr<google::cloud::rest_internal::RestClient> iam_rest_client,
    std::shared_ptr<RawClient> curl_client, google::cloud::Options options)
    : storage_rest_client_(std::move(storage_rest_client)),
      iam_rest_client_(std::move(iam_rest_client)),
      curl_client_(std::move(curl_client)),
      options_(std::move(options)) {}

StatusOr<ListBucketsResponse> RestClient::ListBuckets(
    ListBucketsRequest const& request) {
  RestRequestBuilder builder(
      absl::StrCat("storage/", options_.get<TargetApiVersionOption>(), "/b"));
  auto auth = AddAuthorizationHeader(options_, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  builder.AddQueryParameter("project", request.project_id());
  return ParseFromRestResponse<ListBucketsResponse>(
      storage_rest_client_->Get(std::move(builder).BuildRequest()));
}

StatusOr<BucketMetadata> RestClient::CreateBucket(
    CreateBucketRequest const& request) {
  RestRequestBuilder builder(
      absl::StrCat("storage/", options_.get<TargetApiVersionOption>(), "/b"));
  auto auth = AddAuthorizationHeader(options_, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  builder.AddQueryParameter("project", request.project_id());
  builder.AddHeader("Content-Type", "application/json");
  auto payload = request.json_payload();
  return CheckedFromString<BucketMetadataParser>(storage_rest_client_->Post(
      std::move(builder).BuildRequest(), {absl::MakeConstSpan(payload)}));
}

StatusOr<BucketMetadata> RestClient::GetBucketMetadata(
    GetBucketMetadataRequest const& request) {
  RestRequestBuilder builder(
      absl::StrCat("storage/", options_.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name()));
  auto auth = AddAuthorizationHeader(options_, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  return CheckedFromString<BucketMetadataParser>(
      storage_rest_client_->Get(std::move(builder).BuildRequest()));
}

StatusOr<EmptyResponse> RestClient::DeleteBucket(
    DeleteBucketRequest const& request) {
  RestRequestBuilder builder(
      absl::StrCat("storage/", options_.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name()));
  auto auth = AddAuthorizationHeader(options_, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  return ReturnEmptyResponse(
      storage_rest_client_->Delete(std::move(builder).BuildRequest()));
}

StatusOr<BucketMetadata> RestClient::UpdateBucket(
    UpdateBucketRequest const& request) {
  RestRequestBuilder builder(
      absl::StrCat("storage/", options_.get<TargetApiVersionOption>(), "/b/",
                   request.metadata().name()));
  auto auth = AddAuthorizationHeader(options_, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  builder.AddHeader("Content-Type", "application/json");
  auto payload = request.json_payload();
  return CheckedFromString<BucketMetadataParser>(storage_rest_client_->Put(
      std::move(builder).BuildRequest(), {absl::MakeConstSpan(payload)}));
}

StatusOr<BucketMetadata> RestClient::PatchBucket(
    PatchBucketRequest const& request) {
  RestRequestBuilder builder(
      absl::StrCat("storage/", options_.get<TargetApiVersionOption>(), "/b/",
                   request.bucket()));
  auto auth = AddAuthorizationHeader(options_, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  builder.AddHeader("Content-Type", "application/json");
  auto payload = request.payload();
  return CheckedFromString<BucketMetadataParser>(storage_rest_client_->Patch(
      std::move(builder).BuildRequest(), {absl::MakeConstSpan(payload)}));
}

StatusOr<NativeIamPolicy> RestClient::GetNativeBucketIamPolicy(
    GetBucketIamPolicyRequest const& request) {
  RestRequestBuilder builder(
      absl::StrCat("storage/", options_.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/iam"));
  auto auth = AddAuthorizationHeader(options_, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  return CreateFromJson<NativeIamPolicy>(
      storage_rest_client_->Get(std::move(builder).BuildRequest()));
}

StatusOr<NativeIamPolicy> RestClient::SetNativeBucketIamPolicy(
    SetNativeBucketIamPolicyRequest const& request) {
  RestRequestBuilder builder(
      absl::StrCat("storage/", options_.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/iam"));
  auto auth = AddAuthorizationHeader(options_, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  builder.AddHeader("Content-Type", "application/json");
  auto const& payload = request.json_payload();
  return CreateFromJson<NativeIamPolicy>(storage_rest_client_->Put(
      std::move(builder).BuildRequest(), {absl::MakeConstSpan(payload)}));
}

StatusOr<TestBucketIamPermissionsResponse> RestClient::TestBucketIamPermissions(
    TestBucketIamPermissionsRequest const& request) {
  RestRequestBuilder builder(
      absl::StrCat("storage/", options_.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/iam/testPermissions"));
  auto auth = AddAuthorizationHeader(options_, builder);
  if (!auth.ok()) return auth;
  for (auto const& p : request.permissions()) {
    builder.AddQueryParameter("permissions", p);
  }
  request.AddOptionsToHttpRequest(builder);
  return ParseFromRestResponse<TestBucketIamPermissionsResponse>(
      storage_rest_client_->Get(std::move(builder).BuildRequest()));
}

StatusOr<BucketMetadata> RestClient::LockBucketRetentionPolicy(
    LockBucketRetentionPolicyRequest const& request) {
  RestRequestBuilder builder(
      absl::StrCat("storage/", options_.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/lockRetentionPolicy"));
  auto auth = AddAuthorizationHeader(options_, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  builder.AddHeader("Content-Type", "application/json");
  return CheckedFromString<BucketMetadataParser>(storage_rest_client_->Post(
      std::move(builder).BuildRequest(), {absl::MakeConstSpan(std::string{})}));
}

StatusOr<ObjectMetadata> RestClient::InsertObjectMedia(
    InsertObjectMediaRequest const& request) {
  return curl_client_->InsertObjectMedia(request);
}

StatusOr<ObjectMetadata> RestClient::CopyObject(
    CopyObjectRequest const& request) {
  return curl_client_->CopyObject(request);
}

StatusOr<ObjectMetadata> RestClient::GetObjectMetadata(
    GetObjectMetadataRequest const& request) {
  return curl_client_->GetObjectMetadata(request);
}

StatusOr<std::unique_ptr<ObjectReadSource>> RestClient::ReadObject(
    ReadObjectRangeRequest const& request) {
  return curl_client_->ReadObject(request);
}

StatusOr<ListObjectsResponse> RestClient::ListObjects(
    ListObjectsRequest const& request) {
  return curl_client_->ListObjects(request);
}

StatusOr<EmptyResponse> RestClient::DeleteObject(
    DeleteObjectRequest const& request) {
  return curl_client_->DeleteObject(request);
}

StatusOr<ObjectMetadata> RestClient::UpdateObject(
    UpdateObjectRequest const& request) {
  return curl_client_->UpdateObject(request);
}

StatusOr<ObjectMetadata> RestClient::PatchObject(
    PatchObjectRequest const& request) {
  return curl_client_->PatchObject(request);
}

StatusOr<ObjectMetadata> RestClient::ComposeObject(
    ComposeObjectRequest const& request) {
  return curl_client_->ComposeObject(request);
}

StatusOr<RewriteObjectResponse> RestClient::RewriteObject(
    RewriteObjectRequest const& request) {
  return curl_client_->RewriteObject(request);
}

StatusOr<CreateResumableUploadResponse> RestClient::CreateResumableUpload(
    ResumableUploadRequest const& request) {
  return curl_client_->CreateResumableUpload(request);
}

StatusOr<QueryResumableUploadResponse> RestClient::QueryResumableUpload(
    QueryResumableUploadRequest const& request) {
  return curl_client_->QueryResumableUpload(request);
}

StatusOr<EmptyResponse> RestClient::DeleteResumableUpload(
    DeleteResumableUploadRequest const& request) {
  return curl_client_->DeleteResumableUpload(request);
}

StatusOr<QueryResumableUploadResponse> RestClient::UploadChunk(
    UploadChunkRequest const& request) {
  return curl_client_->UploadChunk(request);
}

StatusOr<ListBucketAclResponse> RestClient::ListBucketAcl(
    ListBucketAclRequest const& request) {
  return curl_client_->ListBucketAcl(request);
}

StatusOr<BucketAccessControl> RestClient::GetBucketAcl(
    GetBucketAclRequest const& request) {
  return curl_client_->GetBucketAcl(request);
}

StatusOr<BucketAccessControl> RestClient::CreateBucketAcl(
    CreateBucketAclRequest const& request) {
  return curl_client_->CreateBucketAcl(request);
}

StatusOr<EmptyResponse> RestClient::DeleteBucketAcl(
    DeleteBucketAclRequest const& request) {
  return curl_client_->DeleteBucketAcl(request);
}

StatusOr<BucketAccessControl> RestClient::UpdateBucketAcl(
    UpdateBucketAclRequest const& request) {
  return curl_client_->UpdateBucketAcl(request);
}

StatusOr<BucketAccessControl> RestClient::PatchBucketAcl(
    PatchBucketAclRequest const& request) {
  return curl_client_->PatchBucketAcl(request);
}

StatusOr<ListObjectAclResponse> RestClient::ListObjectAcl(
    ListObjectAclRequest const& request) {
  return curl_client_->ListObjectAcl(request);
}

StatusOr<ObjectAccessControl> RestClient::CreateObjectAcl(
    CreateObjectAclRequest const& request) {
  return curl_client_->CreateObjectAcl(request);
}

StatusOr<EmptyResponse> RestClient::DeleteObjectAcl(
    DeleteObjectAclRequest const& request) {
  return curl_client_->DeleteObjectAcl(request);
}

StatusOr<ObjectAccessControl> RestClient::GetObjectAcl(
    GetObjectAclRequest const& request) {
  return curl_client_->GetObjectAcl(request);
}

StatusOr<ObjectAccessControl> RestClient::UpdateObjectAcl(
    UpdateObjectAclRequest const& request) {
  return curl_client_->UpdateObjectAcl(request);
}

StatusOr<ObjectAccessControl> RestClient::PatchObjectAcl(
    PatchObjectAclRequest const& request) {
  return curl_client_->PatchObjectAcl(request);
}

StatusOr<ListDefaultObjectAclResponse> RestClient::ListDefaultObjectAcl(
    ListDefaultObjectAclRequest const& request) {
  return curl_client_->ListDefaultObjectAcl(request);
}

StatusOr<ObjectAccessControl> RestClient::CreateDefaultObjectAcl(
    CreateDefaultObjectAclRequest const& request) {
  return curl_client_->CreateDefaultObjectAcl(request);
}

StatusOr<EmptyResponse> RestClient::DeleteDefaultObjectAcl(
    DeleteDefaultObjectAclRequest const& request) {
  return curl_client_->DeleteDefaultObjectAcl(request);
}

StatusOr<ObjectAccessControl> RestClient::GetDefaultObjectAcl(
    GetDefaultObjectAclRequest const& request) {
  return curl_client_->GetDefaultObjectAcl(request);
}

StatusOr<ObjectAccessControl> RestClient::UpdateDefaultObjectAcl(
    UpdateDefaultObjectAclRequest const& request) {
  return curl_client_->UpdateDefaultObjectAcl(request);
}

StatusOr<ObjectAccessControl> RestClient::PatchDefaultObjectAcl(
    PatchDefaultObjectAclRequest const& request) {
  return curl_client_->PatchDefaultObjectAcl(request);
}

StatusOr<ServiceAccount> RestClient::GetServiceAccount(
    GetProjectServiceAccountRequest const& request) {
  return curl_client_->GetServiceAccount(request);
}

StatusOr<ListHmacKeysResponse> RestClient::ListHmacKeys(
    ListHmacKeysRequest const& request) {
  return curl_client_->ListHmacKeys(request);
}

StatusOr<CreateHmacKeyResponse> RestClient::CreateHmacKey(
    CreateHmacKeyRequest const& request) {
  return curl_client_->CreateHmacKey(request);
}

StatusOr<EmptyResponse> RestClient::DeleteHmacKey(
    DeleteHmacKeyRequest const& request) {
  return curl_client_->DeleteHmacKey(request);
}

StatusOr<HmacKeyMetadata> RestClient::GetHmacKey(
    GetHmacKeyRequest const& request) {
  return curl_client_->GetHmacKey(request);
}

StatusOr<HmacKeyMetadata> RestClient::UpdateHmacKey(
    UpdateHmacKeyRequest const& request) {
  return curl_client_->UpdateHmacKey(request);
}

StatusOr<SignBlobResponse> RestClient::SignBlob(
    SignBlobRequest const& request) {
  return curl_client_->SignBlob(request);
}

StatusOr<ListNotificationsResponse> RestClient::ListNotifications(
    ListNotificationsRequest const& request) {
  return curl_client_->ListNotifications(request);
}

StatusOr<NotificationMetadata> RestClient::CreateNotification(
    CreateNotificationRequest const& request) {
  return curl_client_->CreateNotification(request);
}

StatusOr<NotificationMetadata> RestClient::GetNotification(
    GetNotificationRequest const& request) {
  return curl_client_->GetNotification(request);
}

StatusOr<EmptyResponse> RestClient::DeleteNotification(
    DeleteNotificationRequest const& request) {
  return curl_client_->DeleteNotification(request);
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
