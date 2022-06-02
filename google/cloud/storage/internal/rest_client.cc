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
#include "google/cloud/storage/internal/generate_message_boundary.h"
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
#include "google/cloud/internal/curl_handle.h"
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

bool XmlEnabled() {
  auto const config =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_STORAGE_REST_CONFIG")
          .value_or("");
  return config != "disable-xml";
}

std::string UrlEscapeString(std::string const& value) {
  CurlHandle handle;
  return std::string(handle.MakeEscapedString(value).get());
}

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
  auto storage_client = rest::MakePooledRestClient(
      RestEndpoint(options), ResolveStorageAuthority(options));
  auto iam_client = rest::MakePooledRestClient(IamEndpoint(options),
                                               ResolveIamAuthority(options));
  auto curl_client = internal::CurlClient::Create(options);
  return Create(std::move(options), std::move(storage_client),
                std::move(iam_client), std::move(curl_client));
}
std::shared_ptr<RestClient> RestClient::Create(
    Options options,
    std::shared_ptr<google::cloud::rest_internal::RestClient>
        storage_rest_client,
    std::shared_ptr<google::cloud::rest_internal::RestClient> iam_rest_client,
    std::shared_ptr<RawClient> curl_client) {
  return std::shared_ptr<RestClient>(
      new RestClient(std::move(storage_rest_client), std::move(iam_rest_client),
                     std::move(curl_client), std::move(options)));
}

Options RestClient::ResolveStorageAuthority(Options const& options) {
  auto endpoint = RestEndpoint(options);
  if (options.has<AuthorityOption>() ||
      !absl::StrContains(endpoint, "googleapis.com"))
    return options;
  return Options(options).set<AuthorityOption>("storage.googleapis.com");
}

Options RestClient::ResolveIamAuthority(Options const& options) {
  auto endpoint = IamEndpoint(options);
  if (options.has<AuthorityOption>() ||
      !absl::StrContains(endpoint, "googleapis.com"))
    return options;
  return Options(options).set<AuthorityOption>("iamcredentials.googleapis.com");
}

RestClient::RestClient(
    std::shared_ptr<google::cloud::rest_internal::RestClient>
        storage_rest_client,
    std::shared_ptr<google::cloud::rest_internal::RestClient> iam_rest_client,
    std::shared_ptr<RawClient> curl_client, google::cloud::Options options)
    : storage_rest_client_(std::move(storage_rest_client)),
      iam_rest_client_(std::move(iam_rest_client)),
      curl_client_(std::move(curl_client)),
      xml_enabled_(XmlEnabled()),
      generator_(google::cloud::internal::MakeDefaultPRNG()),
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

std::string RestClient::PickBoundary(std::string const& text_to_avoid) {
  // We need to find a string that is *not* found in `text_to_avoid`, we pick
  // a string at random, and see if it is in `text_to_avoid`, if it is, we grow
  // the string with random characters and start from where we last found a
  // the candidate.  Eventually we will find something, though it might be
  // larger than `text_to_avoid`.  And we only make (approximately) one pass
  // over `text_to_avoid`.
  auto generate_candidate = [this](int n) {
    static auto const* const kChars = new std::string(
        "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    std::unique_lock<std::mutex> lk(mu_);
    return google::cloud::internal::Sample(generator_, n, *kChars);
  };
  constexpr int kCandidateInitialSize = 16;
  constexpr int kCandidateGrowthSize = 4;
  return GenerateMessageBoundary(text_to_avoid, std::move(generate_candidate),
                                 kCandidateInitialSize, kCandidateGrowthSize);
}

StatusOr<ObjectMetadata> RestClient::InsertObjectMediaMultipart(
    InsertObjectMediaRequest const& request) {
  RestRequestBuilder builder(
      absl::StrCat("upload/storage/", options_.get<TargetApiVersionOption>(),
                   "/b/", request.bucket_name(), "/o"));
  auto auth = AddAuthorizationHeader(options_, builder);
  if (!auth.ok()) return auth;

  AddOptionsWithSkip<RestRequestBuilder, ContentType> no_content_type{builder};
  request.ForEachOption(no_content_type);

  if (request.HasOption<UserIp>()) {
    builder.AddQueryParameter(UserIp::name(),
                              request.GetOption<UserIp>().value());
  }

  // 2. Pick a separator that does not conflict with the request contents.
  auto boundary = PickBoundary(request.contents());
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

  if (request.HasOption<MD5HashValue>()) {
    metadata["md5Hash"] = request.GetOption<MD5HashValue>().value();
  } else if (!request.GetOption<DisableMD5Hash>().value_or(false)) {
    metadata["md5Hash"] = ComputeMD5Hash(request.contents());
  }

  if (request.HasOption<Crc32cChecksumValue>()) {
    metadata["crc32c"] = request.GetOption<Crc32cChecksumValue>().value();
  } else if (!request.GetOption<DisableCrc32cChecksum>().value_or(false)) {
    metadata["crc32c"] = ComputeCrc32cChecksum(request.contents());
  }

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
  writer << crlf << request.contents() << crlf << marker << "--" << crlf;

  // 6. Return the results as usual.
  auto contents = std::move(writer).str();

  return CheckedFromString<ObjectMetadataParser>(storage_rest_client_->Post(
      std::move(builder).BuildRequest(), {absl::MakeConstSpan(contents)}));
}

StatusOr<ObjectMetadata> RestClient::InsertObjectMediaXml(
    InsertObjectMediaRequest const& request) {
  RestRequestBuilder builder(absl::StrCat(
      request.bucket_name(), "/", UrlEscapeString(request.object_name())));
  auto auth = AddAuthorizationHeader(options_, builder);
  if (!auth.ok()) return auth;
  // Apply the options from InsertObjectMediaRequest that are set, translating
  // to the XML format for them.
  builder.AddOption(request.GetOption<ContentEncoding>());

  // Set the content type of a sensible value, the application can override this
  // in the options for the request.
  if (!request.HasOption<ContentType>()) {
    builder.AddHeader("content-type", "application/octet-stream");
  } else {
    builder.AddOption(request.GetOption<ContentType>());
  }
  builder.AddOption(request.GetOption<EncryptionKey>());

  if (request.HasOption<IfGenerationMatch>()) {
    builder.AddHeader(
        "x-goog-if-generation-match",
        std::to_string(request.GetOption<IfGenerationMatch>().value()));
  }

  // IfGenerationNotMatch cannot be set, checked by the caller.
  if (request.HasOption<IfMetagenerationMatch>()) {
    builder.AddHeader(
        "x-goog-if-metageneration-match",
        std::to_string(request.GetOption<IfMetagenerationMatch>().value()));
  }

  // IfMetagenerationNotMatch cannot be set, checked by the caller.
  if (request.HasOption<KmsKeyName>()) {
    builder.AddHeader("x-goog-encryption-kms-key-name",
                      request.GetOption<KmsKeyName>().value());
  }

  if (request.HasOption<MD5HashValue>()) {
    builder.AddHeader(
        "x-goog-hash",
        absl::StrCat("md5=", request.GetOption<MD5HashValue>().value()));
  } else if (!request.GetOption<DisableMD5Hash>().value_or(false)) {
    builder.AddHeader(
        "x-goog-hash",
        absl::StrCat("md5=" + ComputeMD5Hash(request.contents())));
  }

  if (request.HasOption<Crc32cChecksumValue>()) {
    builder.AddHeader(
        "x-goog-hash",
        absl::StrCat("crc32c=",
                     request.GetOption<Crc32cChecksumValue>().value()));
  } else if (!request.GetOption<DisableCrc32cChecksum>().value_or(false)) {
    builder.AddHeader(
        "x-goog-hash",
        absl::StrCat("crc32c=", ComputeCrc32cChecksum(request.contents())));
  }

  if (request.HasOption<PredefinedAcl>()) {
    builder.AddHeader("x-goog-acl",
                      request.GetOption<PredefinedAcl>().HeaderName());
  }
  builder.AddOption(request.GetOption<UserProject>());

  //
  // Apply the options from GenericRequestBase<> that are set, translating
  // to the XML format for them.
  //
  // Fields cannot be set, checked by the caller.
  builder.AddOption(request.GetOption<CustomHeader>());
  builder.AddOption(request.GetOption<IfMatchEtag>());
  builder.AddOption(request.GetOption<IfNoneMatchEtag>());
  // QuotaUser cannot be set, checked by the caller.
  // UserIp cannot be set, checked by the caller.

  auto response =
      storage_rest_client_->Put(std::move(builder).BuildRequest(),
                                {absl::MakeConstSpan(request.contents())});
  if (!response.ok()) return std::move(response).status();
  if ((*response)->StatusCode() >= rest::kMinNotSuccess) {
    return rest::AsStatus(std::move(**response));
  }
  auto payload = rest::ReadAll(std::move(**response).ExtractPayload());
  if (!payload.ok()) return std::move(payload).status();
  return internal::ObjectMetadataParser::FromJson(nlohmann::json{
      {"name", request.object_name()},
      {"bucket", request.bucket_name()},
  });
}

StatusOr<ObjectMetadata> RestClient::InsertObjectMediaSimple(
    InsertObjectMediaRequest const& request) {
  RestRequestBuilder builder(
      absl::StrCat("upload/storage/", options_.get<TargetApiVersionOption>(),
                   "/b/", request.bucket_name(), "/o"));
  auto auth = AddAuthorizationHeader(options_, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
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
      storage_rest_client_->Post(std::move(builder).BuildRequest(),
                                 {absl::MakeConstSpan(request.contents())}));
}

StatusOr<ObjectMetadata> RestClient::InsertObjectMedia(
    InsertObjectMediaRequest const& request) {
  // If the object metadata is specified, then we need to do a multipart upload.
  if (request.HasOption<WithObjectMetadata>()) {
    return InsertObjectMediaMultipart(request);
  }

  // Unless the request uses a feature that disables it, prefer to use XML.
  if (xml_enabled_ && !request.HasOption<IfMetagenerationNotMatch>() &&
      !request.HasOption<IfGenerationNotMatch>() &&
      !request.HasOption<QuotaUser>() && !request.HasOption<UserIp>() &&
      !request.HasOption<Projection>() && request.HasOption<Fields>() &&
      request.GetOption<Fields>().value().empty()) {
    return InsertObjectMediaXml(request);
  }

  // If the application has set an explicit hash value we need to use multipart
  // uploads. `DisableMD5Hash` and `DisableCrc32cChecksum` should not be
  // dependent on each other.
  if (!request.GetOption<DisableMD5Hash>().value_or(false) ||
      !request.GetOption<DisableCrc32cChecksum>().value_or(false) ||
      request.HasOption<MD5HashValue>() ||
      request.HasOption<Crc32cChecksumValue>()) {
    return InsertObjectMediaMultipart(request);
  }

  // Otherwise do a simple upload.
  return InsertObjectMediaSimple(request);
}

StatusOr<ObjectMetadata> RestClient::CopyObject(
    CopyObjectRequest const& request) {
  return curl_client_->CopyObject(request);
}

StatusOr<ObjectMetadata> RestClient::GetObjectMetadata(
    GetObjectMetadataRequest const& request) {
  RestRequestBuilder builder(absl::StrCat(
      "storage/", options_.get<TargetApiVersionOption>(), "/b/",
      request.bucket_name(), "/o/", UrlEscapeString(request.object_name())));
  auto auth = AddAuthorizationHeader(options_, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  return CheckedFromString<ObjectMetadataParser>(
      storage_rest_client_->Get(std::move(builder).BuildRequest()));
}

StatusOr<std::unique_ptr<ObjectReadSource>> RestClient::ReadObject(
    ReadObjectRangeRequest const& request) {
  return curl_client_->ReadObject(request);
}

StatusOr<ListObjectsResponse> RestClient::ListObjects(
    ListObjectsRequest const& request) {
  RestRequestBuilder builder(
      absl::StrCat("storage/", options_.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/o"));
  auto auth = AddAuthorizationHeader(options_, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  builder.AddQueryParameter("pageToken", request.page_token());
  return ParseFromRestResponse<ListObjectsResponse>(
      storage_rest_client_->Get(std::move(builder).BuildRequest()));
}

StatusOr<EmptyResponse> RestClient::DeleteObject(
    DeleteObjectRequest const& request) {
  RestRequestBuilder builder(absl::StrCat(
      "storage/", options_.get<TargetApiVersionOption>(), "/b/",
      request.bucket_name(), "/o/", UrlEscapeString(request.object_name())));
  auto auth = AddAuthorizationHeader(options_, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  return ReturnEmptyResponse(
      storage_rest_client_->Delete(std::move(builder).BuildRequest()));
}

StatusOr<ObjectMetadata> RestClient::UpdateObject(
    UpdateObjectRequest const& request) {
  RestRequestBuilder builder(absl::StrCat(
      "storage/", options_.get<TargetApiVersionOption>(), "/b/",
      request.bucket_name(), "/o/", UrlEscapeString(request.object_name())));
  auto auth = AddAuthorizationHeader(options_, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  builder.AddHeader("Content-Type", "application/json");
  auto payload = request.json_payload();
  return CheckedFromString<ObjectMetadataParser>(storage_rest_client_->Put(
      std::move(builder).BuildRequest(), {absl::MakeConstSpan(payload)}));
}

StatusOr<ObjectMetadata> RestClient::PatchObject(
    PatchObjectRequest const& request) {
  RestRequestBuilder builder(absl::StrCat(
      "storage/", options_.get<TargetApiVersionOption>(), "/b/",
      request.bucket_name(), "/o/", UrlEscapeString(request.object_name())));
  auto auth = AddAuthorizationHeader(options_, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  builder.AddHeader("Content-Type", "application/json");
  auto payload = request.payload();
  return CheckedFromString<ObjectMetadataParser>(storage_rest_client_->Patch(
      std::move(builder).BuildRequest(), {absl::MakeConstSpan(payload)}));
}

StatusOr<ObjectMetadata> RestClient::ComposeObject(
    ComposeObjectRequest const& request) {
  RestRequestBuilder builder(
      absl::StrCat("storage/", options_.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/o/",
                   UrlEscapeString(request.object_name()), "/compose"));
  auto auth = AddAuthorizationHeader(options_, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  builder.AddHeader("Content-Type", "application/json");
  auto payload = request.JsonPayload();
  return CheckedFromString<ObjectMetadataParser>(storage_rest_client_->Post(
      std::move(builder).BuildRequest(), {absl::MakeConstSpan(payload)}));
}

StatusOr<RewriteObjectResponse> RestClient::RewriteObject(
    RewriteObjectRequest const& request) {
  RestRequestBuilder builder(absl::StrCat(
      "storage/", options_.get<TargetApiVersionOption>(), "/b/",
      request.source_bucket(), "/o/", UrlEscapeString(request.source_object()),
      "/rewriteTo/b/", request.destination_bucket(), "/o/",
      UrlEscapeString(request.destination_object())));
  auto auth = AddAuthorizationHeader(options_, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
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
      storage_rest_client_->Post(std::move(builder).BuildRequest(),
                                 {absl::MakeConstSpan(json_payload)}));
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
