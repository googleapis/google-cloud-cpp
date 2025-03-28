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

#include "google/cloud/storage/internal/rest/stub.h"
#include "google/cloud/storage/internal/bucket_access_control_parser.h"
#include "google/cloud/storage/internal/bucket_metadata_parser.h"
#include "google/cloud/storage/internal/bucket_requests.h"
#include "google/cloud/storage/internal/generate_message_boundary.h"
#include "google/cloud/storage/internal/hmac_key_metadata_parser.h"
#include "google/cloud/storage/internal/notification_metadata_parser.h"
#include "google/cloud/storage/internal/object_access_control_parser.h"
#include "google/cloud/storage/internal/object_metadata_parser.h"
#include "google/cloud/storage/internal/object_read_streambuf.h"
#include "google/cloud/storage/internal/rest/object_read_source.h"
#include "google/cloud/storage/internal/rest/request_builder.h"
#include "google/cloud/storage/internal/service_account_parser.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/auth_header_error.h"
#include "google/cloud/internal/curl_wrappers.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/url_encode.h"
#include "absl/strings/match.h"
#include "absl/strings/strip.h"
#include <sstream>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

namespace rest = google::cloud::rest_internal;
using ::google::cloud::internal::AuthHeaderError;
using ::google::cloud::internal::UrlEncode;

namespace {

bool IsHttpError(rest::HttpStatusCode code) {
  return code >= rest::kMinNotSuccess;
}

template <typename ReturnType>
StatusOr<ReturnType> ParseFromRestResponse(
    StatusOr<std::unique_ptr<rest::RestResponse>> response,
    std::function<bool(rest::HttpStatusCode)> const& failure_predicate =
        IsHttpError) {
  if (!response.ok()) return std::move(response).status();
  if (failure_predicate((*response)->StatusCode())) {
    return rest::AsStatus(std::move(**response));
  }

  HttpResponse http_response{
      (*response)->StatusCode(), {}, (*response)->Headers()};
  auto payload = rest::ReadAll(std::move(**response).ExtractPayload());
  if (!payload.ok()) return std::move(payload).status();
  http_response.payload = std::move(*payload);
  return ReturnType::FromHttpResponse(http_response);
}

template <typename Parser>
auto CheckedFromString(StatusOr<std::unique_ptr<rest::RestResponse>> response)
    -> decltype(Parser::FromString(
        *(rest::ReadAll(std::move(**response).ExtractPayload())))) {
  if (!response.ok()) {
    return std::move(response).status();
  }
  if (IsHttpError(**response)) return rest::AsStatus(std::move(**response));
  auto payload = rest::ReadAll(std::move(**response).ExtractPayload());
  if (!payload.ok()) return std::move(payload).status();
  return Parser::FromString(*payload);
}

StatusOr<EmptyResponse> ReturnEmptyResponse(
    StatusOr<std::unique_ptr<rest::RestResponse>> response,
    std::function<bool(rest::HttpStatusCode)> const& failure_predicate =
        IsHttpError) {
  if (!response.ok()) {
    return std::move(response).status();
  }
  if (failure_predicate((*response)->StatusCode())) {
    return rest::AsStatus(std::move(**response));
  }
  return EmptyResponse{};
}

template <typename ReturnType>
StatusOr<ReturnType> CreateFromJson(
    StatusOr<std::unique_ptr<rest::RestResponse>> response) {
  if (!response.ok()) return std::move(response).status();
  if (IsHttpError(**response)) return rest::AsStatus(std::move(**response));
  auto payload = rest::ReadAll(std::move(**response).ExtractPayload());
  if (!payload.ok()) return std::move(payload).status();
  return ReturnType::CreateFromJson(*payload);
}

Status AddCustomHeaders(Options const& options,
  RestRequestBuilder& builder) {
    // In tests this option may not be set. And over time we want to retire it.
    if (!options.has<CustomHeadersOption>()) return {};
    auto custom_headers =
        options.get<CustomHeadersOption>();
    for (auto const& h : options.get<CustomHeadersOption>()) {
      builder.AddHeader(h.first, h.second);
    }
    return {};
}

void AddOptionsToRequestBuilder(Options const& options,
  RestRequestBuilder& builder) {
    Status cust = AddCustomHeaders(options, builder);
    if (options.has<UserProjectOption>()) {
      builder.AddHeader("x-goog-user-project",
                          options.get<UserProjectOption>());
    }
    if (options.has<UserIpOption>() && !options.has<QuotaUserOption>()) {
      builder.AddHeader("x-goog-user-ip", options.get<UserIpOption>());
    }
    if (options.has<QuotaUserOption>()) {
      builder.AddHeader("x-goog-quota-user", options.get<QuotaUserOption>());
    }
    if (options.has<FieldMaskOption>()) {
      builder.AddHeader("x-goog-fieldmask", options.get<FieldMaskOption>());
    }
}

Status AddAuthorizationHeader(Options const& options,
                              RestRequestBuilder& builder) {
  // In tests this option may not be set. And over time we want to retire it.
  if (!options.has<Oauth2CredentialsOption>()) return {};
  auto auth_header =
      options.get<Oauth2CredentialsOption>()->AuthorizationHeader();
  if (!auth_header) return AuthHeaderError(std::move(auth_header).status());
  builder.AddHeader("Authorization", std::string(absl::StripPrefix(
                                         *auth_header, "Authorization: ")));
  return {};
}

}  // namespace

RestStub::RestStub(Options options)
    : options_(std::move(options)),
      storage_rest_client_(rest::MakePooledRestClient(
          RestEndpoint(options_), ResolveStorageAuthority(options_))),
      iam_rest_client_(rest::MakePooledRestClient(
          IamEndpoint(options_), ResolveIamAuthority(options_))) {
  rest_internal::CurlInitializeOnce(options_);
}

RestStub::RestStub(
    Options options,
    std::shared_ptr<google::cloud::rest_internal::RestClient>
        storage_rest_client,
    std::shared_ptr<google::cloud::rest_internal::RestClient> iam_rest_client)
    : options_(std::move(options)),
      storage_rest_client_(std::move(storage_rest_client)),
      iam_rest_client_(std::move(iam_rest_client)) {
  rest_internal::CurlInitializeOnce(options_);
}

Options RestStub::ResolveStorageAuthority(Options const& options) {
  auto endpoint = RestEndpoint(options);
  if (options.has<AuthorityOption>() ||
      !absl::StrContains(endpoint, "googleapis.com")) {
    return options;
  }
  return Options(options).set<AuthorityOption>("storage.googleapis.com");
}

Options RestStub::ResolveIamAuthority(Options const& options) {
  auto endpoint = IamEndpoint(options);
  if (options.has<AuthorityOption>() ||
      !absl::StrContains(endpoint, "googleapis.com")) {
    return options;
  }
  return Options(options).set<AuthorityOption>("iamcredentials.googleapis.com");
}

Options RestStub::options() const { return options_; }

StatusOr<ListBucketsResponse> RestStub::ListBuckets(
    rest_internal::RestContext& context, Options const& options,
    ListBucketsRequest const& request) {
  RestRequestBuilder builder(
      absl::StrCat("storage/", options.get<TargetApiVersionOption>(), "/b"));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  builder.AddQueryParameter("project", request.project_id());
  return ParseFromRestResponse<ListBucketsResponse>(
      storage_rest_client_->Get(context, std::move(builder).BuildRequest()));
}

StatusOr<BucketMetadata> RestStub::CreateBucket(
    rest_internal::RestContext& context, Options const& options,
    CreateBucketRequest const& request) {
  RestRequestBuilder builder(
      absl::StrCat("storage/", options.get<TargetApiVersionOption>(), "/b"));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  builder.AddQueryParameter("project", request.project_id());
  builder.AddHeader("Content-Type", "application/json");
  auto payload = request.json_payload();
  auto response = CheckedFromString<BucketMetadataParser>(
      storage_rest_client_->Post(context, std::move(builder).BuildRequest(),
                                 {absl::MakeConstSpan(payload)}));
  // GCS returns a 409 when buckets already exist:
  //     https://cloud.google.com/storage/docs/json_api/v1/status-codes#409-conflict
  // This seems to be the only case where kAlreadyExists is a better match
  // for 409 than kAborted.
  if (!response && response.status().code() == StatusCode::kAborted) {
    return google::cloud::internal::AlreadyExistsError(
        response.status().message(), response.status().error_info());
  }
  return response;
}

StatusOr<BucketMetadata> RestStub::GetBucketMetadata(
    rest_internal::RestContext& context, Options const& options,
    GetBucketMetadataRequest const& request) {
  RestRequestBuilder builder(absl::StrCat("storage/",
                                          options.get<TargetApiVersionOption>(),
                                          "/b/", request.bucket_name()));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  return CheckedFromString<BucketMetadataParser>(
      storage_rest_client_->Get(context, std::move(builder).BuildRequest()));
}

StatusOr<EmptyResponse> RestStub::DeleteBucket(
    rest_internal::RestContext& context, Options const& options,
    DeleteBucketRequest const& request) {
  RestRequestBuilder builder(absl::StrCat("storage/",
                                          options.get<TargetApiVersionOption>(),
                                          "/b/", request.bucket_name()));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  return ReturnEmptyResponse(
      storage_rest_client_->Delete(context, std::move(builder).BuildRequest()));
}

StatusOr<BucketMetadata> RestStub::UpdateBucket(
    rest_internal::RestContext& context, Options const& options,
    UpdateBucketRequest const& request) {
  RestRequestBuilder builder(absl::StrCat("storage/",
                                          options.get<TargetApiVersionOption>(),
                                          "/b/", request.metadata().name()));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  builder.AddHeader("Content-Type", "application/json");
  auto payload = request.json_payload();
  return CheckedFromString<BucketMetadataParser>(
      storage_rest_client_->Put(context, std::move(builder).BuildRequest(),
                                {absl::MakeConstSpan(payload)}));
}

StatusOr<BucketMetadata> RestStub::PatchBucket(
    rest_internal::RestContext& context, Options const& options,
    PatchBucketRequest const& request) {
  RestRequestBuilder builder(absl::StrCat("storage/",
                                          options.get<TargetApiVersionOption>(),
                                          "/b/", request.bucket()));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  builder.AddHeader("Content-Type", "application/json");
  auto payload = request.payload();
  return CheckedFromString<BucketMetadataParser>(
      storage_rest_client_->Patch(context, std::move(builder).BuildRequest(),
                                  {absl::MakeConstSpan(payload)}));
}

StatusOr<NativeIamPolicy> RestStub::GetNativeBucketIamPolicy(
    rest_internal::RestContext& context, Options const& options,
    GetBucketIamPolicyRequest const& request) {
  RestRequestBuilder builder(
      absl::StrCat("storage/", options.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/iam"));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  return CreateFromJson<NativeIamPolicy>(
      storage_rest_client_->Get(context, std::move(builder).BuildRequest()));
}

StatusOr<NativeIamPolicy> RestStub::SetNativeBucketIamPolicy(
    rest_internal::RestContext& context, Options const& options,
    SetNativeBucketIamPolicyRequest const& request) {
  RestRequestBuilder builder(
      absl::StrCat("storage/", options.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/iam"));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  builder.AddHeader("Content-Type", "application/json");
  auto const& payload = request.json_payload();
  return CreateFromJson<NativeIamPolicy>(
      storage_rest_client_->Put(context, std::move(builder).BuildRequest(),
                                {absl::MakeConstSpan(payload)}));
}

StatusOr<TestBucketIamPermissionsResponse> RestStub::TestBucketIamPermissions(
    rest_internal::RestContext& context, Options const& options,
    TestBucketIamPermissionsRequest const& request) {
  RestRequestBuilder builder(
      absl::StrCat("storage/", options.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/iam/testPermissions"));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  for (auto const& p : request.permissions()) {
    builder.AddQueryParameter("permissions", p);
  }
  AddOptionsToRequestBuilder(options, builder);
  return ParseFromRestResponse<TestBucketIamPermissionsResponse>(
      storage_rest_client_->Get(context, std::move(builder).BuildRequest()));
}

StatusOr<BucketMetadata> RestStub::LockBucketRetentionPolicy(
    rest_internal::RestContext& context, Options const& options,
    LockBucketRetentionPolicyRequest const& request) {
  RestRequestBuilder builder(
      absl::StrCat("storage/", options.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/lockRetentionPolicy"));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  builder.AddHeader("Content-Type", "application/json");
  builder.AddOption(IfMetagenerationMatch(request.metageneration()));
  return CheckedFromString<BucketMetadataParser>(
      storage_rest_client_->Post(context, std::move(builder).BuildRequest(),
                                 {absl::MakeConstSpan(std::string{})}));
}

std::string RestStub::MakeBoundary() {
  std::unique_lock<std::mutex> lk(mu_);
  return GenerateMessageBoundaryCandidate(generator_);
}

StatusOr<ObjectMetadata> RestStub::InsertObjectMediaMultipart(
    rest_internal::RestContext& context, Options const& options,
    InsertObjectMediaRequest const& request) {
  RestRequestBuilder builder(absl::StrCat("upload/storage/",
                                          options.get<TargetApiVersionOption>(),
                                          "/b/", request.bucket_name(), "/o"));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;

  AddOptionsWithSkip<RestRequestBuilder, ContentType> no_content_type{builder};
  request.ForEachOption(no_content_type);

  if (request.HasOption<UserIp>()) {
    builder.AddQueryParameter(UserIp::name(),
                              request.GetOption<UserIp>().value());
  }

  // 2. create a random separator which is unlikely to exist in the payload.
  auto const boundary = MakeBoundary();
  builder.AddHeader("content-type", "multipart/related; boundary=" + boundary);
  builder.AddQueryParameter("uploadType", "multipart");
  builder.AddQueryParameter("name", request.object_name());

  // 3. Use a std::ostringstream to compute the full payload because computing
  // the size upfront is more complicated than it is worth.
  std::ostringstream writer;

  nlohmann::json metadata = nlohmann::json::object();
  if (request.HasOption<WithObjectMetadata>()) {
    metadata = ObjectMetadataJsonForInsert(
        request.GetOption<WithObjectMetadata>().value());
  }

  request.hash_function().Update(/*offset=*/0, request.payload());
  auto hashes = storage::internal::FinishHashes(request);
  if (!hashes.crc32c.empty()) metadata["crc32c"] = hashes.crc32c;
  if (!hashes.md5.empty()) metadata["md5Hash"] = hashes.md5;

  std::string crlf = "\r\n";
  std::string marker = "--" + boundary;

  // 4. Format the first part, including the separators and the headers.
  writer << marker << crlf << "content-type: application/json; charset=UTF-8"
         << crlf << crlf << metadata.dump() << crlf << marker << crlf;

  // 5. Format the second part, which includes all the contents and a final
  //    separator.
  if (request.HasOption<ContentType>()) {
    writer << "content-type: " << request.GetOption<ContentType>().value()
           << crlf;
  } else if (metadata.count("contentType") != 0) {
    writer << "content-type: "
           << metadata.value("contentType", "application/octet-stream") << crlf;
  } else {
    writer << "content-type: application/octet-stream" << crlf;
  }

  writer << crlf;
  auto header = std::move(writer).str();
  auto trailer = crlf + marker + "--" + crlf;

  // 6. Return the results as usual.
  return CheckedFromString<ObjectMetadataParser>(storage_rest_client_->Post(
      context, std::move(builder).BuildRequest(),
      {absl::MakeConstSpan(header), absl::MakeConstSpan(request.payload()),
       absl::MakeConstSpan(trailer)}));
}

StatusOr<ObjectMetadata> RestStub::InsertObjectMediaSimple(
    rest_internal::RestContext& context, Options const& options,
    InsertObjectMediaRequest const& request) {
  RestRequestBuilder builder(absl::StrCat("upload/storage/",
                                          options.get<TargetApiVersionOption>(),
                                          "/b/", request.bucket_name(), "/o"));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  if (request.HasOption<UserIp>()) {
    builder.AddQueryParameter(UserIp::name(),
                              request.GetOption<UserIp>().value());
  }

  // Set the content type to a sensible value, the application can override this
  // in the options for the request.
  if (!request.HasOption<ContentType>()) {
    builder.AddHeader("Content-Type", "application/octet-stream");
  }
  builder.AddQueryParameter("uploadType", "media");
  builder.AddQueryParameter("name", request.object_name());
  return CheckedFromString<ObjectMetadataParser>(
      storage_rest_client_->Post(context, std::move(builder).BuildRequest(),
                                 {absl::MakeConstSpan(request.payload())}));
}

StatusOr<ObjectMetadata> RestStub::InsertObjectMedia(
    rest_internal::RestContext& context, Options const& options,
    InsertObjectMediaRequest const& request) {
  // If the object metadata is specified, then we need to do a multipart upload.
  if (request.HasOption<WithObjectMetadata>()) {
    return InsertObjectMediaMultipart(context, options, request);
  }

  // If the application has set an explicit hash value we need to use multipart
  // uploads. `DisableMD5Hash` and `DisableCrc32cChecksum` should not be
  // dependent on each other.
  if (!request.GetOption<DisableMD5Hash>().value_or(false) ||
      !request.GetOption<DisableCrc32cChecksum>().value_or(false) ||
      request.HasOption<MD5HashValue>() ||
      request.HasOption<Crc32cChecksumValue>()) {
    return InsertObjectMediaMultipart(context, options, request);
  }

  // Otherwise do a simple upload.
  return InsertObjectMediaSimple(context, options, request);
}

StatusOr<ObjectMetadata> RestStub::CopyObject(
    rest_internal::RestContext& context, Options const& options,
    CopyObjectRequest const& request) {
  RestRequestBuilder builder(absl::StrCat(
      "storage/", options.get<TargetApiVersionOption>(), "/b/",
      request.source_bucket(), "/o/", UrlEncode(request.source_object()),
      "/copyTo/b/", request.destination_bucket(), "/o/",
      UrlEncode(request.destination_object())));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  builder.AddHeader("Content-Type", "application/json");
  std::string json_payload("{}");
  if (request.HasOption<WithObjectMetadata>()) {
    json_payload = ObjectMetadataJsonForCopy(
                       request.GetOption<WithObjectMetadata>().value())
                       .dump();
  }

  return CheckedFromString<ObjectMetadataParser>(
      storage_rest_client_->Post(context, std::move(builder).BuildRequest(),
                                 {absl::MakeConstSpan(json_payload)}));
}

StatusOr<ObjectMetadata> RestStub::GetObjectMetadata(
    rest_internal::RestContext& context, Options const& options,
    GetObjectMetadataRequest const& request) {
  RestRequestBuilder builder(absl::StrCat(
      "storage/", options.get<TargetApiVersionOption>(), "/b/",
      request.bucket_name(), "/o/", UrlEncode(request.object_name())));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  return CheckedFromString<ObjectMetadataParser>(
      storage_rest_client_->Get(context, std::move(builder).BuildRequest()));
}

StatusOr<std::unique_ptr<ObjectReadSource>> RestStub::ReadObject(
    rest_internal::RestContext& context, Options const& options,
    ReadObjectRangeRequest const& request) {
  RestRequestBuilder builder(absl::StrCat(
      "storage/", options.get<TargetApiVersionOption>(), "/b/",
      request.bucket_name(), "/o/", UrlEncode(request.object_name())));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);

  builder.AddQueryParameter("alt", "media");
  // We should not guess the intent in this case.
  if (request.HasOption<storage::ReadLast>() &&
      request.HasOption<storage::ReadRange>()) {
    return google::cloud::internal::InvalidArgumentError(
        "Cannot use ReadLast() and ReadRange() at the same time",
        GCP_ERROR_INFO());
  }
  // We should not guess the intent in this case.
  if (request.HasOption<storage::ReadLast>() &&
      request.HasOption<storage::ReadFromOffset>()) {
    return google::cloud::internal::InvalidArgumentError(
        "Cannot use ReadLast() and ReadFromOffset() at the same time",
        GCP_ERROR_INFO());
  }
  if (request.RequiresRangeHeader()) {
    builder.AddHeader("Range", request.RangeHeaderValue());
  }
  if (request.RequiresNoCache()) {
    builder.AddHeader("Cache-Control", "no-transform");
  }

  auto response =
      storage_rest_client_->Get(context, std::move(builder).BuildRequest());
  if (!response.ok()) return response.status();
  if (IsHttpError(**response)) return rest::AsStatus(std::move(**response));
  return std::unique_ptr<ObjectReadSource>(
      new RestObjectReadSource(*std::move(response)));
}

StatusOr<ListObjectsResponse> RestStub::ListObjects(
    rest_internal::RestContext& context, Options const& options,
    ListObjectsRequest const& request) {
  RestRequestBuilder builder(absl::StrCat("storage/",
                                          options.get<TargetApiVersionOption>(),
                                          "/b/", request.bucket_name(), "/o"));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  builder.AddQueryParameter("pageToken", request.page_token());
  return ParseFromRestResponse<ListObjectsResponse>(
      storage_rest_client_->Get(context, std::move(builder).BuildRequest()));
}

StatusOr<EmptyResponse> RestStub::DeleteObject(
    rest_internal::RestContext& context, Options const& options,
    DeleteObjectRequest const& request) {
  RestRequestBuilder builder(absl::StrCat(
      "storage/", options.get<TargetApiVersionOption>(), "/b/",
      request.bucket_name(), "/o/", UrlEncode(request.object_name())));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  return ReturnEmptyResponse(
      storage_rest_client_->Delete(context, std::move(builder).BuildRequest()));
}

StatusOr<ObjectMetadata> RestStub::UpdateObject(
    rest_internal::RestContext& context, Options const& options,
    UpdateObjectRequest const& request) {
  RestRequestBuilder builder(absl::StrCat(
      "storage/", options.get<TargetApiVersionOption>(), "/b/",
      request.bucket_name(), "/o/", UrlEncode(request.object_name())));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  builder.AddHeader("Content-Type", "application/json");
  auto payload = request.json_payload();
  return CheckedFromString<ObjectMetadataParser>(
      storage_rest_client_->Put(context, std::move(builder).BuildRequest(),
                                {absl::MakeConstSpan(payload)}));
}

StatusOr<ObjectMetadata> RestStub::MoveObject(
    rest_internal::RestContext& context, Options const& options,
    MoveObjectRequest const& request) {
  RestRequestBuilder builder(absl::StrCat(
      "storage/", options.get<TargetApiVersionOption>(), "/b/",
      request.bucket_name(), "/o/", UrlEncode(request.source_object_name()),
      "/moveTo/o/", UrlEncode(request.destination_object_name())));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  builder.AddHeader("Content-Type", "application/json");
  std::string json_payload("{}");

  return CheckedFromString<ObjectMetadataParser>(
      storage_rest_client_->Post(context, std::move(builder).BuildRequest(),
                                 {absl::MakeConstSpan(json_payload)}));
}

StatusOr<ObjectMetadata> RestStub::PatchObject(
    rest_internal::RestContext& context, Options const& options,
    PatchObjectRequest const& request) {
  RestRequestBuilder builder(absl::StrCat(
      "storage/", options.get<TargetApiVersionOption>(), "/b/",
      request.bucket_name(), "/o/", UrlEncode(request.object_name())));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  builder.AddHeader("Content-Type", "application/json");
  auto payload = request.payload();
  return CheckedFromString<ObjectMetadataParser>(
      storage_rest_client_->Patch(context, std::move(builder).BuildRequest(),
                                  {absl::MakeConstSpan(payload)}));
}

StatusOr<ObjectMetadata> RestStub::ComposeObject(
    rest_internal::RestContext& context, Options const& options,
    ComposeObjectRequest const& request) {
  RestRequestBuilder builder(
      absl::StrCat("storage/", options.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/o/",
                   UrlEncode(request.object_name()), "/compose"));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  builder.AddHeader("Content-Type", "application/json");
  auto payload = request.JsonPayload();
  return CheckedFromString<ObjectMetadataParser>(
      storage_rest_client_->Post(context, std::move(builder).BuildRequest(),
                                 {absl::MakeConstSpan(payload)}));
}

StatusOr<RewriteObjectResponse> RestStub::RewriteObject(
    rest_internal::RestContext& context, Options const& options,
    RewriteObjectRequest const& request) {
  RestRequestBuilder builder(absl::StrCat(
      "storage/", options.get<TargetApiVersionOption>(), "/b/",
      request.source_bucket(), "/o/", UrlEncode(request.source_object()),
      "/rewriteTo/b/", request.destination_bucket(), "/o/",
      UrlEncode(request.destination_object())));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  if (!request.rewrite_token().empty()) {
    builder.AddQueryParameter("rewriteToken", request.rewrite_token());
  }
  builder.AddHeader("Content-Type", "application/json");
  std::string json_payload("{}");
  if (request.HasOption<WithObjectMetadata>()) {
    json_payload = ObjectMetadataJsonForRewrite(
                       request.GetOption<WithObjectMetadata>().value())
                       .dump();
  }

  return ParseFromRestResponse<RewriteObjectResponse>(
      storage_rest_client_->Post(context, std::move(builder).BuildRequest(),
                                 {absl::MakeConstSpan(json_payload)}));
}

StatusOr<ObjectMetadata> RestStub::RestoreObject(
    rest_internal::RestContext& context, Options const& options,
    RestoreObjectRequest const& request) {
  RestRequestBuilder builder(absl::StrCat(
      "storage/", options.get<TargetApiVersionOption>(), "/b/",
      request.bucket_name(), "/o/", UrlEncode(request.object_name()),
      "/restore?generation=", request.generation()));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  builder.AddHeader("Content-Type", "application/json");
  std::string json_payload("{}");

  return CheckedFromString<ObjectMetadataParser>(
      storage_rest_client_->Post(context, std::move(builder).BuildRequest(),
                                 {absl::MakeConstSpan(json_payload)}));
}

StatusOr<CreateResumableUploadResponse> RestStub::CreateResumableUpload(
    rest_internal::RestContext& context, Options const& options,
    ResumableUploadRequest const& request) {
  RestRequestBuilder builder(absl::StrCat("upload/storage/",
                                          options.get<TargetApiVersionOption>(),
                                          "/b/", request.bucket_name(), "/o"));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;

  AddOptionsWithSkip<RestRequestBuilder, ContentType> no_content_type{builder};
  request.ForEachOption(no_content_type);
  builder.AddQueryParameter("uploadType", "resumable");
  builder.AddHeader("Content-Type", "application/json; charset=UTF-8");
  nlohmann::json resource;
  if (request.HasOption<WithObjectMetadata>()) {
    resource = ObjectMetadataJsonForInsert(
        request.GetOption<WithObjectMetadata>().value());
  }
  if (request.HasOption<ContentEncoding>()) {
    resource["contentEncoding"] = request.GetOption<ContentEncoding>().value();
  }
  if (request.HasOption<ContentType>()) {
    resource["contentType"] = request.GetOption<ContentType>().value();
  }
  if (request.HasOption<Crc32cChecksumValue>()) {
    resource["crc32c"] = request.GetOption<Crc32cChecksumValue>().value();
  }
  if (request.HasOption<MD5HashValue>()) {
    resource["md5Hash"] = request.GetOption<MD5HashValue>().value();
  }

  if (resource.empty()) {
    builder.AddQueryParameter("name", request.object_name());
  } else {
    resource["name"] = request.object_name();
  }

  std::string request_payload;
  if (!resource.empty()) request_payload = resource.dump();

  return ParseFromRestResponse<CreateResumableUploadResponse>(
      storage_rest_client_->Post(context, std::move(builder).BuildRequest(),
                                 {absl::MakeConstSpan(request_payload)}));
}

StatusOr<QueryResumableUploadResponse> RestStub::QueryResumableUpload(
    rest_internal::RestContext& context, Options const& options,
    QueryResumableUploadRequest const& request) {
  RestRequestBuilder builder(request.upload_session_url());
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  builder.AddHeader("Content-Range", "bytes */*");
  builder.AddHeader("Content-Type", "application/octet-stream");

  auto failure_predicate = [](rest::HttpStatusCode code) {
    return (code != rest::HttpStatusCode::kResumeIncomplete &&
            code >= rest::HttpStatusCode::kMinNotSuccess);
  };

  return ParseFromRestResponse<QueryResumableUploadResponse>(
      storage_rest_client_->Put(context, std::move(builder).BuildRequest(), {}),
      failure_predicate);
}

StatusOr<EmptyResponse> RestStub::DeleteResumableUpload(
    rest_internal::RestContext& context, Options const& options,
    DeleteResumableUploadRequest const& request) {
  RestRequestBuilder builder(request.upload_session_url());
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);

  auto failure_predicate = [](rest::HttpStatusCode code) {
    return (code != rest::HttpStatusCode::kClientClosedRequest &&
            code >= rest::HttpStatusCode::kMinNotSuccess);
  };

  return ReturnEmptyResponse(
      storage_rest_client_->Delete(context, std::move(builder).BuildRequest()),
      failure_predicate);
}

StatusOr<QueryResumableUploadResponse> RestStub::UploadChunk(
    rest_internal::RestContext& context, Options const& options,
    UploadChunkRequest const& request) {
  RestRequestBuilder builder(request.upload_session_url());
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  builder.AddHeader("Content-Range", request.RangeHeaderValue());
  builder.AddHeader("Content-Type", "application/octet-stream");
  // We need to explicitly disable chunked transfer encoding. libcurl uses is by
  // default (at least in this case), and that wastes bandwidth as the content
  // length is known.
  builder.AddHeader("Transfer-Encoding", {});
  auto offset = request.offset();
  for (auto const& b : request.payload()) {
    request.hash_function().Update(offset,
                                   absl::string_view{b.data(), b.size()});
    offset += b.size();
  }

  auto failure_predicate = [](rest::HttpStatusCode code) {
    return (code != rest::HttpStatusCode::kResumeIncomplete &&
            code >= rest::HttpStatusCode::kMinNotSuccess);
  };

  return ParseFromRestResponse<QueryResumableUploadResponse>(
      storage_rest_client_->Put(context, std::move(builder).BuildRequest(),
                                request.payload()),
      failure_predicate);
}

StatusOr<ListBucketAclResponse> RestStub::ListBucketAcl(
    rest_internal::RestContext& context, Options const& options,
    ListBucketAclRequest const& request) {
  RestRequestBuilder builder(
      absl::StrCat("storage/", options.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/acl"));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  return ParseFromRestResponse<ListBucketAclResponse>(
      storage_rest_client_->Get(context, std::move(builder).BuildRequest()));
}

StatusOr<BucketAccessControl> RestStub::GetBucketAcl(
    rest_internal::RestContext& context, Options const& options,
    GetBucketAclRequest const& request) {
  RestRequestBuilder builder(absl::StrCat(
      "storage/", options.get<TargetApiVersionOption>(), "/b/",
      request.bucket_name(), "/acl/", UrlEncode(request.entity())));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  return CheckedFromString<BucketAccessControlParser>(
      storage_rest_client_->Get(context, std::move(builder).BuildRequest()));
}

StatusOr<BucketAccessControl> RestStub::CreateBucketAcl(
    rest_internal::RestContext& context, Options const& options,
    CreateBucketAclRequest const& request) {
  RestRequestBuilder builder(
      absl::StrCat("storage/", options.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/acl"));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  builder.AddHeader("Content-Type", "application/json");
  nlohmann::json object;
  object["entity"] = request.entity();
  object["role"] = request.role();
  auto payload = object.dump();
  return CheckedFromString<BucketAccessControlParser>(
      storage_rest_client_->Post(context, std::move(builder).BuildRequest(),
                                 {absl::MakeConstSpan(payload)}));
}

StatusOr<EmptyResponse> RestStub::DeleteBucketAcl(
    rest_internal::RestContext& context, Options const& options,
    DeleteBucketAclRequest const& request) {
  RestRequestBuilder builder(absl::StrCat(
      "storage/", options.get<TargetApiVersionOption>(), "/b/",
      request.bucket_name(), "/acl/", UrlEncode(request.entity())));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  return ReturnEmptyResponse(
      storage_rest_client_->Delete(context, std::move(builder).BuildRequest()));
}

StatusOr<BucketAccessControl> RestStub::UpdateBucketAcl(
    rest_internal::RestContext& context, Options const& options,
    UpdateBucketAclRequest const& request) {
  RestRequestBuilder builder(absl::StrCat(
      "storage/", options.get<TargetApiVersionOption>(), "/b/",
      request.bucket_name(), "/acl/", UrlEncode(request.entity())));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  builder.AddHeader("Content-Type", "application/json");
  nlohmann::json object;
  object["entity"] = request.entity();
  object["role"] = request.role();
  auto payload = object.dump();
  return CheckedFromString<BucketAccessControlParser>(
      storage_rest_client_->Put(context, std::move(builder).BuildRequest(),
                                {absl::MakeConstSpan(payload)}));
}

StatusOr<BucketAccessControl> RestStub::PatchBucketAcl(
    rest_internal::RestContext& context, Options const& options,
    PatchBucketAclRequest const& request) {
  RestRequestBuilder builder(absl::StrCat(
      "storage/", options.get<TargetApiVersionOption>(), "/b/",
      request.bucket_name(), "/acl/", UrlEncode(request.entity())));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  builder.AddHeader("Content-Type", "application/json");
  auto payload = request.payload();
  return CheckedFromString<BucketAccessControlParser>(
      storage_rest_client_->Patch(context, std::move(builder).BuildRequest(),
                                  {absl::MakeConstSpan(payload)}));
}

StatusOr<ListObjectAclResponse> RestStub::ListObjectAcl(
    rest_internal::RestContext& context, Options const& options,
    ListObjectAclRequest const& request) {
  RestRequestBuilder builder(absl::StrCat(
      "storage/", options.get<TargetApiVersionOption>(), "/b/",
      request.bucket_name(), "/o/", UrlEncode(request.object_name()), "/acl"));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  return ParseFromRestResponse<ListObjectAclResponse>(
      storage_rest_client_->Get(context, std::move(builder).BuildRequest()));
}

StatusOr<ObjectAccessControl> RestStub::CreateObjectAcl(
    rest_internal::RestContext& context, Options const& options,
    CreateObjectAclRequest const& request) {
  RestRequestBuilder builder(absl::StrCat(
      "storage/", options.get<TargetApiVersionOption>(), "/b/",
      request.bucket_name(), "/o/", UrlEncode(request.object_name()), "/acl"));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  builder.AddHeader("Content-Type", "application/json");
  nlohmann::json object;
  object["entity"] = request.entity();
  object["role"] = request.role();
  auto payload = object.dump();
  return CheckedFromString<ObjectAccessControlParser>(
      storage_rest_client_->Post(context, std::move(builder).BuildRequest(),
                                 {absl::MakeConstSpan(payload)}));
}

StatusOr<EmptyResponse> RestStub::DeleteObjectAcl(
    rest_internal::RestContext& context, Options const& options,
    DeleteObjectAclRequest const& request) {
  RestRequestBuilder builder(absl::StrCat(
      "storage/", options.get<TargetApiVersionOption>(), "/b/",
      request.bucket_name(), "/o/", UrlEncode(request.object_name()), "/acl/",
      UrlEncode(request.entity())));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  return ReturnEmptyResponse(
      storage_rest_client_->Delete(context, std::move(builder).BuildRequest()));
}

StatusOr<ObjectAccessControl> RestStub::GetObjectAcl(
    rest_internal::RestContext& context, Options const& options,
    GetObjectAclRequest const& request) {
  RestRequestBuilder builder(absl::StrCat(
      "storage/", options.get<TargetApiVersionOption>(), "/b/",
      request.bucket_name(), "/o/", UrlEncode(request.object_name()), "/acl/",
      UrlEncode(request.entity())));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  return CheckedFromString<ObjectAccessControlParser>(
      storage_rest_client_->Get(context, std::move(builder).BuildRequest()));
}

StatusOr<ObjectAccessControl> RestStub::UpdateObjectAcl(
    rest_internal::RestContext& context, Options const& options,
    UpdateObjectAclRequest const& request) {
  RestRequestBuilder builder(absl::StrCat(
      "storage/", options.get<TargetApiVersionOption>(), "/b/",
      request.bucket_name(), "/o/", UrlEncode(request.object_name()), "/acl/",
      UrlEncode(request.entity())));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  builder.AddHeader("Content-Type", "application/json");
  nlohmann::json object;
  object["entity"] = request.entity();
  object["role"] = request.role();
  auto payload = object.dump();
  return CheckedFromString<ObjectAccessControlParser>(
      storage_rest_client_->Put(context, std::move(builder).BuildRequest(),
                                {absl::MakeConstSpan(payload)}));
}

StatusOr<ObjectAccessControl> RestStub::PatchObjectAcl(
    rest_internal::RestContext& context, Options const& options,
    PatchObjectAclRequest const& request) {
  RestRequestBuilder builder(absl::StrCat(
      "storage/", options.get<TargetApiVersionOption>(), "/b/",
      request.bucket_name(), "/o/", UrlEncode(request.object_name()), "/acl/",
      UrlEncode(request.entity())));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  builder.AddHeader("Content-Type", "application/json");
  auto payload = request.payload();
  return CheckedFromString<ObjectAccessControlParser>(
      storage_rest_client_->Patch(context, std::move(builder).BuildRequest(),
                                  {absl::MakeConstSpan(payload)}));
}

StatusOr<ListDefaultObjectAclResponse> RestStub::ListDefaultObjectAcl(
    rest_internal::RestContext& context, Options const& options,
    ListDefaultObjectAclRequest const& request) {
  RestRequestBuilder builder(
      absl::StrCat("storage/", options.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/defaultObjectAcl"));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  return ParseFromRestResponse<ListDefaultObjectAclResponse>(
      storage_rest_client_->Get(context, std::move(builder).BuildRequest()));
}

StatusOr<ObjectAccessControl> RestStub::CreateDefaultObjectAcl(
    rest_internal::RestContext& context, Options const& options,
    CreateDefaultObjectAclRequest const& request) {
  RestRequestBuilder builder(
      absl::StrCat("storage/", options.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/defaultObjectAcl"));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  builder.AddHeader("Content-Type", "application/json");
  nlohmann::json object;
  object["entity"] = request.entity();
  object["role"] = request.role();
  auto payload = object.dump();
  return CheckedFromString<ObjectAccessControlParser>(
      storage_rest_client_->Post(context, std::move(builder).BuildRequest(),
                                 {absl::MakeConstSpan(payload)}));
}

StatusOr<EmptyResponse> RestStub::DeleteDefaultObjectAcl(
    rest_internal::RestContext& context, Options const& options,
    DeleteDefaultObjectAclRequest const& request) {
  RestRequestBuilder builder(
      absl::StrCat("storage/", options.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/defaultObjectAcl/",
                   UrlEncode(request.entity())));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  return ReturnEmptyResponse(
      storage_rest_client_->Delete(context, std::move(builder).BuildRequest()));
}

StatusOr<ObjectAccessControl> RestStub::GetDefaultObjectAcl(
    rest_internal::RestContext& context, Options const& options,
    GetDefaultObjectAclRequest const& request) {
  RestRequestBuilder builder(
      absl::StrCat("storage/", options.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/defaultObjectAcl/",
                   UrlEncode(request.entity())));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  return CheckedFromString<ObjectAccessControlParser>(
      storage_rest_client_->Get(context, std::move(builder).BuildRequest()));
}

StatusOr<ObjectAccessControl> RestStub::UpdateDefaultObjectAcl(
    rest_internal::RestContext& context, Options const& options,
    UpdateDefaultObjectAclRequest const& request) {
  RestRequestBuilder builder(
      absl::StrCat("storage/", options.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/defaultObjectAcl/",
                   UrlEncode(request.entity())));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  builder.AddHeader("Content-Type", "application/json");
  nlohmann::json object;
  object["entity"] = request.entity();
  object["role"] = request.role();
  auto payload = object.dump();
  return CheckedFromString<ObjectAccessControlParser>(
      storage_rest_client_->Put(context, std::move(builder).BuildRequest(),
                                {absl::MakeConstSpan(payload)}));
}

StatusOr<ObjectAccessControl> RestStub::PatchDefaultObjectAcl(
    rest_internal::RestContext& context, Options const& options,
    PatchDefaultObjectAclRequest const& request) {
  RestRequestBuilder builder(
      absl::StrCat("storage/", options.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/defaultObjectAcl/",
                   UrlEncode(request.entity())));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  builder.AddHeader("Content-Type", "application/json");
  auto payload = request.payload();
  return CheckedFromString<ObjectAccessControlParser>(
      storage_rest_client_->Patch(context, std::move(builder).BuildRequest(),
                                  {absl::MakeConstSpan(payload)}));
}

StatusOr<ServiceAccount> RestStub::GetServiceAccount(
    rest_internal::RestContext& context, Options const& options,
    GetProjectServiceAccountRequest const& request) {
  RestRequestBuilder builder(
      absl::StrCat("storage/", options.get<TargetApiVersionOption>(),
                   "/projects/", request.project_id(), "/serviceAccount"));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  return CheckedFromString<ServiceAccountParser>(
      storage_rest_client_->Get(context, std::move(builder).BuildRequest()));
}

StatusOr<ListHmacKeysResponse> RestStub::ListHmacKeys(
    rest_internal::RestContext& context, Options const& options,
    ListHmacKeysRequest const& request) {
  RestRequestBuilder builder(
      absl::StrCat("storage/", options.get<TargetApiVersionOption>(),
                   "/projects/", request.project_id(), "/hmacKeys"));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  return ParseFromRestResponse<ListHmacKeysResponse>(
      storage_rest_client_->Get(context, std::move(builder).BuildRequest()));
}

StatusOr<CreateHmacKeyResponse> RestStub::CreateHmacKey(
    rest_internal::RestContext& context, Options const& options,
    CreateHmacKeyRequest const& request) {
  RestRequestBuilder builder(
      absl::StrCat("storage/", options.get<TargetApiVersionOption>(),
                   "/projects/", request.project_id(), "/hmacKeys"));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  builder.AddQueryParameter("serviceAccountEmail", request.service_account());
  return ParseFromRestResponse<CreateHmacKeyResponse>(
      storage_rest_client_->Post(
          context, std::move(builder).BuildRequest(),
          std::vector<std::pair<std::string, std::string>>{}));
}

StatusOr<EmptyResponse> RestStub::DeleteHmacKey(
    rest_internal::RestContext& context, Options const& options,
    DeleteHmacKeyRequest const& request) {
  RestRequestBuilder builder(absl::StrCat(
      "storage/", options.get<TargetApiVersionOption>(), "/projects/",
      request.project_id(), "/hmacKeys/", request.access_id()));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  return ReturnEmptyResponse(
      storage_rest_client_->Delete(context, std::move(builder).BuildRequest()));
}

StatusOr<HmacKeyMetadata> RestStub::GetHmacKey(
    rest_internal::RestContext& context, Options const& options,
    GetHmacKeyRequest const& request) {
  RestRequestBuilder builder(absl::StrCat(
      "storage/", options.get<TargetApiVersionOption>(), "/projects/",
      request.project_id(), "/hmacKeys/", request.access_id()));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  return CheckedFromString<HmacKeyMetadataParser>(
      storage_rest_client_->Get(context, std::move(builder).BuildRequest()));
}

StatusOr<HmacKeyMetadata> RestStub::UpdateHmacKey(
    rest_internal::RestContext& context, Options const& options,
    UpdateHmacKeyRequest const& request) {
  RestRequestBuilder builder(absl::StrCat(
      "storage/", options.get<TargetApiVersionOption>(), "/projects/",
      request.project_id(), "/hmacKeys/", request.access_id()));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  nlohmann::json json_payload;
  if (!request.resource().state().empty()) {
    json_payload["state"] = request.resource().state();
  }
  if (!request.resource().etag().empty()) {
    json_payload["etag"] = request.resource().etag();
  }
  builder.AddHeader("Content-Type", "application/json");
  auto payload = json_payload.dump();
  return CheckedFromString<HmacKeyMetadataParser>(
      storage_rest_client_->Put(context, std::move(builder).BuildRequest(),
                                {absl::MakeConstSpan(payload)}));
}

StatusOr<SignBlobResponse> RestStub::SignBlob(
    rest_internal::RestContext& context, Options const& options,
    SignBlobRequest const& request) {
  RestRequestBuilder builder(absl::StrCat(
      "projects/-/serviceAccounts/", request.service_account(), ":signBlob"));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  nlohmann::json json_payload;
  json_payload["payload"] = request.base64_encoded_blob();
  if (!request.delegates().empty()) {
    json_payload["delegates"] = request.delegates();
  }
  builder.AddHeader("Content-Type", "application/json");
  auto payload = json_payload.dump();
  return ParseFromRestResponse<SignBlobResponse>(
      iam_rest_client_->Post(context, std::move(builder).BuildRequest(),
                             {absl::MakeConstSpan(payload)}));
}

StatusOr<ListNotificationsResponse> RestStub::ListNotifications(
    rest_internal::RestContext& context, Options const& options,
    ListNotificationsRequest const& request) {
  RestRequestBuilder builder(
      absl::StrCat("storage/", options.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/notificationConfigs"));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  return ParseFromRestResponse<ListNotificationsResponse>(
      storage_rest_client_->Get(context, std::move(builder).BuildRequest()));
}

StatusOr<NotificationMetadata> RestStub::CreateNotification(
    rest_internal::RestContext& context, Options const& options,
    CreateNotificationRequest const& request) {
  RestRequestBuilder builder(
      absl::StrCat("storage/", options.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/notificationConfigs"));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  builder.AddHeader("Content-Type", "application/json");
  auto payload = request.json_payload();
  return CheckedFromString<NotificationMetadataParser>(
      storage_rest_client_->Post(context, std::move(builder).BuildRequest(),
                                 {absl::MakeConstSpan(payload)}));
}

StatusOr<NotificationMetadata> RestStub::GetNotification(
    rest_internal::RestContext& context, Options const& options,
    GetNotificationRequest const& request) {
  RestRequestBuilder builder(
      absl::StrCat("storage/", options.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/notificationConfigs/",
                   request.notification_id()));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  return CheckedFromString<NotificationMetadataParser>(
      storage_rest_client_->Get(context, std::move(builder).BuildRequest()));
}

StatusOr<EmptyResponse> RestStub::DeleteNotification(
    rest_internal::RestContext& context, Options const& options,
    DeleteNotificationRequest const& request) {
  RestRequestBuilder builder(
      absl::StrCat("storage/", options.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/notificationConfigs/",
                   request.notification_id()));
  auto auth = AddAuthorizationHeader(options, builder);
  if (!auth.ok()) return auth;
  AddOptionsToRequestBuilder(options, builder);
  return ReturnEmptyResponse(
      storage_rest_client_->Delete(context, std::move(builder).BuildRequest()));
}

std::vector<std::string> RestStub::InspectStackStructure() const {
  return {"RestStub"};
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
