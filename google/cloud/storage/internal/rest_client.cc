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
#include "google/cloud/storage/internal/curl_handle.h"
#include "google/cloud/storage/internal/generate_message_boundary.h"
#include "google/cloud/storage/internal/hmac_key_metadata_parser.h"
#include "google/cloud/storage/internal/logging_client.h"
#include "google/cloud/storage/internal/notification_metadata_parser.h"
#include "google/cloud/storage/internal/object_access_control_parser.h"
#include "google/cloud/storage/internal/object_metadata_parser.h"
#include "google/cloud/storage/internal/object_read_streambuf.h"
#include "google/cloud/storage/internal/rest_object_read_source.h"
#include "google/cloud/storage/internal/rest_request_builder.h"
#include "google/cloud/storage/internal/service_account_parser.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/auth_header_error.h"
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
using ::google::cloud::internal::AuthHeaderError;
using ::google::cloud::internal::CurrentOptions;

namespace {

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
  if ((*response)->StatusCode() >= rest::kMinNotSuccess) {
    return rest::AsStatus(std::move(**response));
  }
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
  if (!auth_header) return AuthHeaderError(std::move(auth_header).status());
  builder.AddHeader("Authorization", std::string(absl::StripPrefix(
                                         *auth_header, "Authorization: ")));
  return {};
}

}  // namespace

std::shared_ptr<RestClient> RestClient::Create(Options options) {
  auto storage_client = rest::MakePooledRestClient(
      RestEndpoint(options), ResolveStorageAuthority(options));
  auto iam_client = rest::MakePooledRestClient(IamEndpoint(options),
                                               ResolveIamAuthority(options));
  return Create(std::move(options), std::move(storage_client),
                std::move(iam_client));
}

std::shared_ptr<RestClient> RestClient::Create(
    Options options,
    std::shared_ptr<google::cloud::rest_internal::RestClient>
        storage_rest_client,
    std::shared_ptr<google::cloud::rest_internal::RestClient> iam_rest_client) {
  return std::shared_ptr<RestClient>(
      new RestClient(std::move(storage_rest_client), std::move(iam_rest_client),
                     std::move(options)));
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
    google::cloud::Options options)
    : storage_rest_client_(std::move(storage_rest_client)),
      iam_rest_client_(std::move(iam_rest_client)),
      xml_enabled_(XmlEnabled()),
      generator_(google::cloud::internal::MakeDefaultPRNG()),
      options_(std::move(options)),
      backwards_compatibility_options_(
          MakeBackwardsCompatibleClientOptions(options_)) {}

ClientOptions const& RestClient::client_options() const {
  return backwards_compatibility_options_;
}

Options RestClient::options() const { return options_; }

StatusOr<ListBucketsResponse> RestClient::ListBuckets(
    ListBucketsRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(
      absl::StrCat("storage/", current.get<TargetApiVersionOption>(), "/b"));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  builder.AddQueryParameter("project", request.project_id());
  return ParseFromRestResponse<ListBucketsResponse>(
      storage_rest_client_->Get(std::move(builder).BuildRequest()));
}

StatusOr<BucketMetadata> RestClient::CreateBucket(
    CreateBucketRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(
      absl::StrCat("storage/", current.get<TargetApiVersionOption>(), "/b"));
  auto auth = AddAuthorizationHeader(current, builder);
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
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(absl::StrCat("storage/",
                                          current.get<TargetApiVersionOption>(),
                                          "/b/", request.bucket_name()));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  return CheckedFromString<BucketMetadataParser>(
      storage_rest_client_->Get(std::move(builder).BuildRequest()));
}

StatusOr<EmptyResponse> RestClient::DeleteBucket(
    DeleteBucketRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(absl::StrCat("storage/",
                                          current.get<TargetApiVersionOption>(),
                                          "/b/", request.bucket_name()));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  return ReturnEmptyResponse(
      storage_rest_client_->Delete(std::move(builder).BuildRequest()));
}

StatusOr<BucketMetadata> RestClient::UpdateBucket(
    UpdateBucketRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(absl::StrCat("storage/",
                                          current.get<TargetApiVersionOption>(),
                                          "/b/", request.metadata().name()));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  builder.AddHeader("Content-Type", "application/json");
  auto payload = request.json_payload();
  return CheckedFromString<BucketMetadataParser>(storage_rest_client_->Put(
      std::move(builder).BuildRequest(), {absl::MakeConstSpan(payload)}));
}

StatusOr<BucketMetadata> RestClient::PatchBucket(
    PatchBucketRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(absl::StrCat("storage/",
                                          current.get<TargetApiVersionOption>(),
                                          "/b/", request.bucket()));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  builder.AddHeader("Content-Type", "application/json");
  auto payload = request.payload();
  return CheckedFromString<BucketMetadataParser>(storage_rest_client_->Patch(
      std::move(builder).BuildRequest(), {absl::MakeConstSpan(payload)}));
}

StatusOr<NativeIamPolicy> RestClient::GetNativeBucketIamPolicy(
    GetBucketIamPolicyRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(
      absl::StrCat("storage/", current.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/iam"));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  return CreateFromJson<NativeIamPolicy>(
      storage_rest_client_->Get(std::move(builder).BuildRequest()));
}

StatusOr<NativeIamPolicy> RestClient::SetNativeBucketIamPolicy(
    SetNativeBucketIamPolicyRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(
      absl::StrCat("storage/", current.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/iam"));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  builder.AddHeader("Content-Type", "application/json");
  auto const& payload = request.json_payload();
  return CreateFromJson<NativeIamPolicy>(storage_rest_client_->Put(
      std::move(builder).BuildRequest(), {absl::MakeConstSpan(payload)}));
}

StatusOr<TestBucketIamPermissionsResponse> RestClient::TestBucketIamPermissions(
    TestBucketIamPermissionsRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(
      absl::StrCat("storage/", current.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/iam/testPermissions"));
  auto auth = AddAuthorizationHeader(current, builder);
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
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(
      absl::StrCat("storage/", current.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/lockRetentionPolicy"));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  builder.AddHeader("Content-Type", "application/json");
  builder.AddOption(IfMetagenerationMatch(request.metageneration()));
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
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(absl::StrCat("upload/storage/",
                                          current.get<TargetApiVersionOption>(),
                                          "/b/", request.bucket_name(), "/o"));
  auto auth = AddAuthorizationHeader(current, builder);
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

  writer << crlf;
  auto header = std::move(writer).str();
  auto trailer = crlf + marker + "--" + crlf;

  // 6. Return the results as usual.
  return CheckedFromString<ObjectMetadataParser>(storage_rest_client_->Post(
      std::move(builder).BuildRequest(),
      {absl::MakeConstSpan(header), absl::MakeConstSpan(request.contents()),
       absl::MakeConstSpan(trailer)}));
}

StatusOr<ObjectMetadata> RestClient::InsertObjectMediaXml(
    InsertObjectMediaRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(absl::StrCat(
      request.bucket_name(), "/", UrlEscapeString(request.object_name())));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  // Apply the options from InsertObjectMediaRequest that are set, translating
  // to the XML format for them.
  builder.AddOption(request.GetOption<ContentEncoding>());

  // Set the content type to a sensible value. The application can override this
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
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(absl::StrCat("upload/storage/",
                                          current.get<TargetApiVersionOption>(),
                                          "/b/", request.bucket_name(), "/o"));
  auto auth = AddAuthorizationHeader(current, builder);
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
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(absl::StrCat(
      "storage/", current.get<TargetApiVersionOption>(), "/b/",
      request.source_bucket(), "/o/", UrlEscapeString(request.source_object()),
      "/copyTo/b/", request.destination_bucket(), "/o/",
      UrlEscapeString(request.destination_object())));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  builder.AddHeader("Content-Type", "application/json");
  std::string json_payload("{}");
  if (request.HasOption<WithObjectMetadata>()) {
    json_payload = ObjectMetadataJsonForCopy(
                       request.GetOption<WithObjectMetadata>().value())
                       .dump();
  }

  return CheckedFromString<ObjectMetadataParser>(storage_rest_client_->Post(
      std::move(builder).BuildRequest(), {absl::MakeConstSpan(json_payload)}));
}

StatusOr<ObjectMetadata> RestClient::GetObjectMetadata(
    GetObjectMetadataRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(absl::StrCat(
      "storage/", current.get<TargetApiVersionOption>(), "/b/",
      request.bucket_name(), "/o/", UrlEscapeString(request.object_name())));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  return CheckedFromString<ObjectMetadataParser>(
      storage_rest_client_->Get(std::move(builder).BuildRequest()));
}

StatusOr<std::unique_ptr<ObjectReadSource>> RestClient::ReadObjectXml(
    ReadObjectRangeRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(absl::StrCat(
      request.bucket_name(), "/", UrlEscapeString(request.object_name())));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  //
  // Apply the options from ReadObjectMediaRequest that are set, translating
  // to the XML format for them.
  //
  builder.AddOption(request.GetOption<EncryptionKey>());
  builder.AddOption(request.GetOption<Generation>());
  // None of the IfGeneration*Match nor IfMetageneration*Match can be set. This
  // is checked by the caller (in this class).
  builder.AddOption(request.GetOption<UserProject>());
  builder.AddOption(request.GetOption<AcceptEncoding>());

  //
  // Apply the options from GenericRequestBase<> that are set, translating
  // to the XML format for them.
  //
  builder.AddOption(request.GetOption<CustomHeader>());
  builder.AddOption(request.GetOption<IfMatchEtag>());
  builder.AddOption(request.GetOption<IfNoneMatchEtag>());
  // QuotaUser cannot be set, checked by the caller.
  // UserIp cannot be set, checked by the caller.

  if (request.RequiresRangeHeader()) {
    builder.AddHeader("Range", request.RangeHeaderValue());
  }
  if (request.RequiresNoCache()) {
    builder.AddHeader("Cache-Control", "no-transform");
  }

  auto response = storage_rest_client_->Get(std::move(builder).BuildRequest());
  if (!response.ok()) return std::move(response).status();

  return std::unique_ptr<ObjectReadSource>(
      new RestObjectReadSource(*std::move(response)));
}

StatusOr<std::unique_ptr<ObjectReadSource>> RestClient::ReadObject(
    ReadObjectRangeRequest const& request) {
  if (xml_enabled_ && !request.HasOption<IfMetagenerationNotMatch>() &&
      !request.HasOption<IfGenerationNotMatch>() &&
      !request.HasOption<IfMetagenerationMatch>() &&
      !request.HasOption<IfGenerationMatch>() &&
      !request.HasOption<QuotaUser>() && !request.HasOption<UserIp>()) {
    return ReadObjectXml(request);
  }

  auto const& current = CurrentOptions();
  RestRequestBuilder builder(absl::StrCat(
      "storage/", current.get<TargetApiVersionOption>(), "/b/",
      request.bucket_name(), "/o/", UrlEscapeString(request.object_name())));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);

  builder.AddQueryParameter("alt", "media");
  if (request.RequiresRangeHeader()) {
    builder.AddHeader("Range", request.RangeHeaderValue());
  }
  if (request.RequiresNoCache()) {
    builder.AddHeader("Cache-Control", "no-transform");
  }

  auto response = storage_rest_client_->Get(std::move(builder).BuildRequest());
  if (!response.ok()) return response.status();

  return std::unique_ptr<ObjectReadSource>(
      new RestObjectReadSource(*std::move(response)));
}

StatusOr<ListObjectsResponse> RestClient::ListObjects(
    ListObjectsRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(absl::StrCat("storage/",
                                          current.get<TargetApiVersionOption>(),
                                          "/b/", request.bucket_name(), "/o"));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  builder.AddQueryParameter("pageToken", request.page_token());
  return ParseFromRestResponse<ListObjectsResponse>(
      storage_rest_client_->Get(std::move(builder).BuildRequest()));
}

StatusOr<EmptyResponse> RestClient::DeleteObject(
    DeleteObjectRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(absl::StrCat(
      "storage/", current.get<TargetApiVersionOption>(), "/b/",
      request.bucket_name(), "/o/", UrlEscapeString(request.object_name())));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  return ReturnEmptyResponse(
      storage_rest_client_->Delete(std::move(builder).BuildRequest()));
}

StatusOr<ObjectMetadata> RestClient::UpdateObject(
    UpdateObjectRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(absl::StrCat(
      "storage/", current.get<TargetApiVersionOption>(), "/b/",
      request.bucket_name(), "/o/", UrlEscapeString(request.object_name())));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  builder.AddHeader("Content-Type", "application/json");
  auto payload = request.json_payload();
  return CheckedFromString<ObjectMetadataParser>(storage_rest_client_->Put(
      std::move(builder).BuildRequest(), {absl::MakeConstSpan(payload)}));
}

StatusOr<ObjectMetadata> RestClient::PatchObject(
    PatchObjectRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(absl::StrCat(
      "storage/", current.get<TargetApiVersionOption>(), "/b/",
      request.bucket_name(), "/o/", UrlEscapeString(request.object_name())));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  builder.AddHeader("Content-Type", "application/json");
  auto payload = request.payload();
  return CheckedFromString<ObjectMetadataParser>(storage_rest_client_->Patch(
      std::move(builder).BuildRequest(), {absl::MakeConstSpan(payload)}));
}

StatusOr<ObjectMetadata> RestClient::ComposeObject(
    ComposeObjectRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(
      absl::StrCat("storage/", current.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/o/",
                   UrlEscapeString(request.object_name()), "/compose"));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  builder.AddHeader("Content-Type", "application/json");
  auto payload = request.JsonPayload();
  return CheckedFromString<ObjectMetadataParser>(storage_rest_client_->Post(
      std::move(builder).BuildRequest(), {absl::MakeConstSpan(payload)}));
}

StatusOr<RewriteObjectResponse> RestClient::RewriteObject(
    RewriteObjectRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(absl::StrCat(
      "storage/", current.get<TargetApiVersionOption>(), "/b/",
      request.source_bucket(), "/o/", UrlEscapeString(request.source_object()),
      "/rewriteTo/b/", request.destination_bucket(), "/o/",
      UrlEscapeString(request.destination_object())));
  auto auth = AddAuthorizationHeader(current, builder);
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
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(absl::StrCat("upload/storage/",
                                          current.get<TargetApiVersionOption>(),
                                          "/b/", request.bucket_name(), "/o"));
  auto auth = AddAuthorizationHeader(current, builder);
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
      storage_rest_client_->Post(std::move(builder).BuildRequest(),
                                 {absl::MakeConstSpan(request_payload)}));
}

StatusOr<QueryResumableUploadResponse> RestClient::QueryResumableUpload(
    QueryResumableUploadRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(request.upload_session_url());
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  builder.AddHeader("Content-Range", "bytes */*");
  builder.AddHeader("Content-Type", "application/octet-stream");

  auto failure_predicate = [](rest::HttpStatusCode code) {
    return (code != rest::HttpStatusCode::kResumeIncomplete &&
            code >= rest::HttpStatusCode::kMinNotSuccess);
  };

  return ParseFromRestResponse<QueryResumableUploadResponse>(
      storage_rest_client_->Put(std::move(builder).BuildRequest(), {}),
      failure_predicate);
}

StatusOr<EmptyResponse> RestClient::DeleteResumableUpload(
    DeleteResumableUploadRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(request.upload_session_url());
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);

  auto failure_predicate = [](rest::HttpStatusCode code) {
    return (code != rest::HttpStatusCode::kClientClosedRequest &&
            code >= rest::HttpStatusCode::kMinNotSuccess);
  };

  return ReturnEmptyResponse(
      storage_rest_client_->Delete(std::move(builder).BuildRequest()),
      failure_predicate);
}

StatusOr<QueryResumableUploadResponse> RestClient::UploadChunk(
    UploadChunkRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(request.upload_session_url());
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  builder.AddHeader("Content-Range", request.RangeHeaderValue());
  builder.AddHeader("Content-Type", "application/octet-stream");
  // We need to explicitly disable chunked transfer encoding. libcurl uses is by
  // default (at least in this case), and that wastes bandwidth as the content
  // length is known.
  builder.AddHeader("Transfer-Encoding", {});

  auto failure_predicate = [](rest::HttpStatusCode code) {
    return (code != rest::HttpStatusCode::kResumeIncomplete &&
            code >= rest::HttpStatusCode::kMinNotSuccess);
  };

  return ParseFromRestResponse<QueryResumableUploadResponse>(
      storage_rest_client_->Put(std::move(builder).BuildRequest(),
                                request.payload()),
      failure_predicate);
}

StatusOr<ListBucketAclResponse> RestClient::ListBucketAcl(
    ListBucketAclRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(
      absl::StrCat("storage/", current.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/acl"));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  return ParseFromRestResponse<ListBucketAclResponse>(
      storage_rest_client_->Get(std::move(builder).BuildRequest()));
}

StatusOr<BucketAccessControl> RestClient::GetBucketAcl(
    GetBucketAclRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(absl::StrCat(
      "storage/", current.get<TargetApiVersionOption>(), "/b/",
      request.bucket_name(), "/acl/", UrlEscapeString(request.entity())));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  return CheckedFromString<BucketAccessControlParser>(
      storage_rest_client_->Get(std::move(builder).BuildRequest()));
}

StatusOr<BucketAccessControl> RestClient::CreateBucketAcl(
    CreateBucketAclRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(
      absl::StrCat("storage/", current.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/acl"));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  builder.AddHeader("Content-Type", "application/json");
  nlohmann::json object;
  object["entity"] = request.entity();
  object["role"] = request.role();
  auto payload = object.dump();
  return CheckedFromString<BucketAccessControlParser>(
      storage_rest_client_->Post(std::move(builder).BuildRequest(),
                                 {absl::MakeConstSpan(payload)}));
}

StatusOr<EmptyResponse> RestClient::DeleteBucketAcl(
    DeleteBucketAclRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(absl::StrCat(
      "storage/", current.get<TargetApiVersionOption>(), "/b/",
      request.bucket_name(), "/acl/", UrlEscapeString(request.entity())));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  return ReturnEmptyResponse(
      storage_rest_client_->Delete(std::move(builder).BuildRequest()));
}

StatusOr<BucketAccessControl> RestClient::UpdateBucketAcl(
    UpdateBucketAclRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(absl::StrCat(
      "storage/", current.get<TargetApiVersionOption>(), "/b/",
      request.bucket_name(), "/acl/", UrlEscapeString(request.entity())));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  builder.AddHeader("Content-Type", "application/json");
  nlohmann::json object;
  object["entity"] = request.entity();
  object["role"] = request.role();
  auto payload = object.dump();
  return CheckedFromString<BucketAccessControlParser>(storage_rest_client_->Put(
      std::move(builder).BuildRequest(), {absl::MakeConstSpan(payload)}));
}

StatusOr<BucketAccessControl> RestClient::PatchBucketAcl(
    PatchBucketAclRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(absl::StrCat(
      "storage/", current.get<TargetApiVersionOption>(), "/b/",
      request.bucket_name(), "/acl/", UrlEscapeString(request.entity())));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  builder.AddHeader("Content-Type", "application/json");
  auto payload = request.payload();
  return CheckedFromString<BucketAccessControlParser>(
      storage_rest_client_->Patch(std::move(builder).BuildRequest(),
                                  {absl::MakeConstSpan(payload)}));
}

StatusOr<ListObjectAclResponse> RestClient::ListObjectAcl(
    ListObjectAclRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(
      absl::StrCat("storage/", current.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/o/",
                   UrlEscapeString(request.object_name()), "/acl"));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  return ParseFromRestResponse<ListObjectAclResponse>(
      storage_rest_client_->Get(std::move(builder).BuildRequest()));
}

StatusOr<ObjectAccessControl> RestClient::CreateObjectAcl(
    CreateObjectAclRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(
      absl::StrCat("storage/", current.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/o/",
                   UrlEscapeString(request.object_name()), "/acl"));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  builder.AddHeader("Content-Type", "application/json");
  nlohmann::json object;
  object["entity"] = request.entity();
  object["role"] = request.role();
  auto payload = object.dump();
  return CheckedFromString<ObjectAccessControlParser>(
      storage_rest_client_->Post(std::move(builder).BuildRequest(),
                                 {absl::MakeConstSpan(payload)}));
}

StatusOr<EmptyResponse> RestClient::DeleteObjectAcl(
    DeleteObjectAclRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(absl::StrCat(
      "storage/", current.get<TargetApiVersionOption>(), "/b/",
      request.bucket_name(), "/o/", UrlEscapeString(request.object_name()),
      "/acl/", UrlEscapeString(request.entity())));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  return ReturnEmptyResponse(
      storage_rest_client_->Delete(std::move(builder).BuildRequest()));
}

StatusOr<ObjectAccessControl> RestClient::GetObjectAcl(
    GetObjectAclRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(absl::StrCat(
      "storage/", current.get<TargetApiVersionOption>(), "/b/",
      request.bucket_name(), "/o/", UrlEscapeString(request.object_name()),
      "/acl/", UrlEscapeString(request.entity())));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  return CheckedFromString<ObjectAccessControlParser>(
      storage_rest_client_->Get(std::move(builder).BuildRequest()));
}

StatusOr<ObjectAccessControl> RestClient::UpdateObjectAcl(
    UpdateObjectAclRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(absl::StrCat(
      "storage/", current.get<TargetApiVersionOption>(), "/b/",
      request.bucket_name(), "/o/", UrlEscapeString(request.object_name()),
      "/acl/", UrlEscapeString(request.entity())));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  builder.AddHeader("Content-Type", "application/json");
  nlohmann::json object;
  object["entity"] = request.entity();
  object["role"] = request.role();
  auto payload = object.dump();
  return CheckedFromString<ObjectAccessControlParser>(storage_rest_client_->Put(
      std::move(builder).BuildRequest(), {absl::MakeConstSpan(payload)}));
}

StatusOr<ObjectAccessControl> RestClient::PatchObjectAcl(
    PatchObjectAclRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(absl::StrCat(
      "storage/", current.get<TargetApiVersionOption>(), "/b/",
      request.bucket_name(), "/o/", UrlEscapeString(request.object_name()),
      "/acl/", UrlEscapeString(request.entity())));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  builder.AddHeader("Content-Type", "application/json");
  auto payload = request.payload();
  return CheckedFromString<ObjectAccessControlParser>(
      storage_rest_client_->Patch(std::move(builder).BuildRequest(),
                                  {absl::MakeConstSpan(payload)}));
}

StatusOr<ListDefaultObjectAclResponse> RestClient::ListDefaultObjectAcl(
    ListDefaultObjectAclRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(
      absl::StrCat("storage/", current.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/defaultObjectAcl"));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  return ParseFromRestResponse<ListDefaultObjectAclResponse>(
      storage_rest_client_->Get(std::move(builder).BuildRequest()));
}

StatusOr<ObjectAccessControl> RestClient::CreateDefaultObjectAcl(
    CreateDefaultObjectAclRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(
      absl::StrCat("storage/", current.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/defaultObjectAcl"));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  builder.AddHeader("Content-Type", "application/json");
  nlohmann::json object;
  object["entity"] = request.entity();
  object["role"] = request.role();
  auto payload = object.dump();
  return CheckedFromString<ObjectAccessControlParser>(
      storage_rest_client_->Post(std::move(builder).BuildRequest(),
                                 {absl::MakeConstSpan(payload)}));
}

StatusOr<EmptyResponse> RestClient::DeleteDefaultObjectAcl(
    DeleteDefaultObjectAclRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(
      absl::StrCat("storage/", current.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/defaultObjectAcl/",
                   UrlEscapeString(request.entity())));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  return ReturnEmptyResponse(
      storage_rest_client_->Delete(std::move(builder).BuildRequest()));
}

StatusOr<ObjectAccessControl> RestClient::GetDefaultObjectAcl(
    GetDefaultObjectAclRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(
      absl::StrCat("storage/", current.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/defaultObjectAcl/",
                   UrlEscapeString(request.entity())));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  return CheckedFromString<ObjectAccessControlParser>(
      storage_rest_client_->Get(std::move(builder).BuildRequest()));
}

StatusOr<ObjectAccessControl> RestClient::UpdateDefaultObjectAcl(
    UpdateDefaultObjectAclRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(
      absl::StrCat("storage/", current.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/defaultObjectAcl/",
                   UrlEscapeString(request.entity())));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  builder.AddHeader("Content-Type", "application/json");
  nlohmann::json object;
  object["entity"] = request.entity();
  object["role"] = request.role();
  auto payload = object.dump();
  return CheckedFromString<ObjectAccessControlParser>(storage_rest_client_->Put(
      std::move(builder).BuildRequest(), {absl::MakeConstSpan(payload)}));
}

StatusOr<ObjectAccessControl> RestClient::PatchDefaultObjectAcl(
    PatchDefaultObjectAclRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(
      absl::StrCat("storage/", current.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/defaultObjectAcl/",
                   UrlEscapeString(request.entity())));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  builder.AddHeader("Content-Type", "application/json");
  auto payload = request.payload();
  return CheckedFromString<ObjectAccessControlParser>(
      storage_rest_client_->Patch(std::move(builder).BuildRequest(),
                                  {absl::MakeConstSpan(payload)}));
}

StatusOr<ServiceAccount> RestClient::GetServiceAccount(
    GetProjectServiceAccountRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(
      absl::StrCat("storage/", current.get<TargetApiVersionOption>(),
                   "/projects/", request.project_id(), "/serviceAccount"));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  return CheckedFromString<ServiceAccountParser>(
      storage_rest_client_->Get(std::move(builder).BuildRequest()));
}

StatusOr<ListHmacKeysResponse> RestClient::ListHmacKeys(
    ListHmacKeysRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(
      absl::StrCat("storage/", current.get<TargetApiVersionOption>(),
                   "/projects/", request.project_id(), "/hmacKeys"));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  return ParseFromRestResponse<ListHmacKeysResponse>(
      storage_rest_client_->Get(std::move(builder).BuildRequest()));
}

StatusOr<CreateHmacKeyResponse> RestClient::CreateHmacKey(
    CreateHmacKeyRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(
      absl::StrCat("storage/", current.get<TargetApiVersionOption>(),
                   "/projects/", request.project_id(), "/hmacKeys"));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  builder.AddQueryParameter("serviceAccountEmail", request.service_account());
  return ParseFromRestResponse<CreateHmacKeyResponse>(
      storage_rest_client_->Post(
          std::move(builder).BuildRequest(),
          std::vector<std::pair<std::string, std::string>>{}));
}

StatusOr<EmptyResponse> RestClient::DeleteHmacKey(
    DeleteHmacKeyRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(absl::StrCat(
      "storage/", current.get<TargetApiVersionOption>(), "/projects/",
      request.project_id(), "/hmacKeys/", request.access_id()));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  return ReturnEmptyResponse(
      storage_rest_client_->Delete(std::move(builder).BuildRequest()));
}

StatusOr<HmacKeyMetadata> RestClient::GetHmacKey(
    GetHmacKeyRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(absl::StrCat(
      "storage/", current.get<TargetApiVersionOption>(), "/projects/",
      request.project_id(), "/hmacKeys/", request.access_id()));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  return CheckedFromString<HmacKeyMetadataParser>(
      storage_rest_client_->Get(std::move(builder).BuildRequest()));
}

StatusOr<HmacKeyMetadata> RestClient::UpdateHmacKey(
    UpdateHmacKeyRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(absl::StrCat(
      "storage/", current.get<TargetApiVersionOption>(), "/projects/",
      request.project_id(), "/hmacKeys/", request.access_id()));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  nlohmann::json json_payload;
  if (!request.resource().state().empty()) {
    json_payload["state"] = request.resource().state();
  }
  if (!request.resource().etag().empty()) {
    json_payload["etag"] = request.resource().etag();
  }
  builder.AddHeader("Content-Type", "application/json");
  auto payload = json_payload.dump();
  return CheckedFromString<HmacKeyMetadataParser>(storage_rest_client_->Put(
      std::move(builder).BuildRequest(), {absl::MakeConstSpan(payload)}));
}

StatusOr<SignBlobResponse> RestClient::SignBlob(
    SignBlobRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(absl::StrCat(
      "projects/-/serviceAccounts/", request.service_account(), ":signBlob"));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  nlohmann::json json_payload;
  json_payload["payload"] = request.base64_encoded_blob();
  if (!request.delegates().empty()) {
    json_payload["delegates"] = request.delegates();
  }
  builder.AddHeader("Content-Type", "application/json");
  auto payload = json_payload.dump();
  return ParseFromRestResponse<SignBlobResponse>(iam_rest_client_->Post(
      std::move(builder).BuildRequest(), {absl::MakeConstSpan(payload)}));
}

StatusOr<ListNotificationsResponse> RestClient::ListNotifications(
    ListNotificationsRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(
      absl::StrCat("storage/", current.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/notificationConfigs"));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  return ParseFromRestResponse<ListNotificationsResponse>(
      storage_rest_client_->Get(std::move(builder).BuildRequest()));
}

StatusOr<NotificationMetadata> RestClient::CreateNotification(
    CreateNotificationRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(
      absl::StrCat("storage/", current.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/notificationConfigs"));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  builder.AddHeader("Content-Type", "application/json");
  auto payload = request.json_payload();
  return CheckedFromString<NotificationMetadataParser>(
      storage_rest_client_->Post(std::move(builder).BuildRequest(),
                                 {absl::MakeConstSpan(payload)}));
}

StatusOr<NotificationMetadata> RestClient::GetNotification(
    GetNotificationRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(
      absl::StrCat("storage/", current.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/notificationConfigs/",
                   request.notification_id()));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  return CheckedFromString<NotificationMetadataParser>(
      storage_rest_client_->Get(std::move(builder).BuildRequest()));
}

StatusOr<EmptyResponse> RestClient::DeleteNotification(
    DeleteNotificationRequest const& request) {
  auto const& current = CurrentOptions();
  RestRequestBuilder builder(
      absl::StrCat("storage/", current.get<TargetApiVersionOption>(), "/b/",
                   request.bucket_name(), "/notificationConfigs/",
                   request.notification_id()));
  auto auth = AddAuthorizationHeader(current, builder);
  if (!auth.ok()) return auth;
  request.AddOptionsToHttpRequest(builder);
  return ReturnEmptyResponse(
      storage_rest_client_->Delete(std::move(builder).BuildRequest()));
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
