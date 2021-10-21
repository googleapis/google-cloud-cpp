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

#include "google/cloud/storage/internal/curl_client.h"
#include "google/cloud/storage/internal/bucket_access_control_parser.h"
#include "google/cloud/storage/internal/bucket_metadata_parser.h"
#include "google/cloud/storage/internal/curl_request_builder.h"
#include "google/cloud/storage/internal/curl_resumable_upload_session.h"
#include "google/cloud/storage/internal/generate_message_boundary.h"
#include "google/cloud/storage/internal/hmac_key_metadata_parser.h"
#include "google/cloud/storage/internal/notification_metadata_parser.h"
#include "google/cloud/storage/internal/object_access_control_parser.h"
#include "google/cloud/storage/internal/object_metadata_parser.h"
#include "google/cloud/storage/internal/object_streambuf.h"
#include "google/cloud/storage/internal/service_account_parser.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/getenv.h"
#include "absl/memory/memory.h"
#include "absl/strings/match.h"
#include <sstream>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

std::shared_ptr<CurlHandleFactory> CreateHandleFactory(Options const& options) {
  auto const pool_size = options.get<ConnectionPoolSizeOption>();
  if (pool_size == 0) {
    return std::make_shared<DefaultCurlHandleFactory>(options);
  }
  return std::make_shared<PooledCurlHandleFactory>(pool_size, options);
}

std::string UrlEscapeString(std::string const& value) {
  CurlHandle handle;
  return std::string(handle.MakeEscapedString(value).get());
}

template <typename ReturnType>
StatusOr<ReturnType> ParseFromString(StatusOr<HttpResponse> response) {
  if (!response.ok()) {
    return std::move(response).status();
  }
  if (response->status_code >= HttpStatusCode::kMinNotSuccess) {
    return AsStatus(*response);
  }
  return ReturnType::ParseFromString(response->payload);
}

template <typename Parser>
auto CheckedFromString(StatusOr<HttpResponse> response)
    -> decltype(Parser::FromString(response->payload)) {
  if (!response.ok()) {
    return std::move(response).status();
  }
  if (response->status_code >= HttpStatusCode::kMinNotSuccess) {
    return AsStatus(*response);
  }
  return Parser::FromString(response->payload);
}

StatusOr<EmptyResponse> ReturnEmptyResponse(StatusOr<HttpResponse> response) {
  if (!response.ok()) {
    return std::move(response).status();
  }
  if (response->status_code >= HttpStatusCode::kMinNotSuccess) {
    return AsStatus(*response);
  }
  return EmptyResponse{};
}

template <typename ReturnType>
StatusOr<ReturnType> ParseFromHttpResponse(StatusOr<HttpResponse> response) {
  if (!response.ok()) {
    return std::move(response).status();
  }
  if (response->status_code >= HttpStatusCode::kMinNotSuccess) {
    return AsStatus(*response);
  }
  return ReturnType::FromHttpResponse(response->payload);
}

bool XmlEnabled() {
  auto const config =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_STORAGE_REST_CONFIG")
          .value_or("");
  return config != "disable-xml";
}

}  // namespace

std::string HostHeader(Options const& options, char const* service) {
  // If this function returns an empty string libcurl will fill out the `Host: `
  // header based on the URL. In most cases this is the correct value. The main
  // exception are applications using `VPC-SC`:
  //     https://cloud.google.com/vpc/docs/configure-private-google-access
  // In those cases the application would target an URL like
  // `https://restricted.googleapis.com`, or `https://private.googleapis.com`,
  // or their own proxy, and need to provide the target's service host.
  auto const& endpoint = options.get<RestEndpointOption>();
  if (absl::StrContains(endpoint, "googleapis.com")) {
    return absl::StrCat("Host: ", service, ".googleapis.com");
  }
  return {};
}

Status CurlClient::SetupBuilderCommon(CurlRequestBuilder& builder,
                                      char const* method, char const* service) {
  auto auth_header =
      opts_.get<Oauth2CredentialsOption>()->AuthorizationHeader();
  if (!auth_header.ok()) {
    return std::move(auth_header).status();
  }
  builder.SetMethod(method)
      .ApplyClientOptions(opts_)
      .AddHeader(auth_header.value())
      .AddHeader(HostHeader(opts_, service))
      .AddHeader(x_goog_api_client_header_);
  return Status();
}

template <typename Request>
void SetupBuilderUserIp(CurlRequestBuilder& builder, Request const& request) {
  if (request.template HasOption<UserIp>()) {
    std::string value = request.template GetOption<UserIp>().value();
    if (value.empty()) {
      value = builder.LastClientIpAddress();
    }
    if (!value.empty()) {
      builder.AddQueryParameter(UserIp::name(), value);
    }
  }
}

template <typename Request>
Status CurlClient::SetupBuilder(CurlRequestBuilder& builder,
                                Request const& request, char const* method) {
  auto status = SetupBuilderCommon(builder, method);
  if (!status.ok()) {
    return status;
  }
  request.AddOptionsToHttpRequest(builder);
  SetupBuilderUserIp(builder, request);
  return Status();
}

template <typename RequestType>
StatusOr<std::unique_ptr<ResumableUploadSession>>
CurlClient::CreateResumableSessionGeneric(RequestType const& request) {
  auto session_id =
      request.template GetOption<UseResumableUploadSession>().value_or("");
  if (!session_id.empty()) {
    return RestoreResumableSession(session_id);
  }

  CurlRequestBuilder builder(
      upload_endpoint_ + "/b/" + request.bucket_name() + "/o", upload_factory_);
  auto status = SetupBuilderCommon(builder, "POST");
  if (!status.ok()) {
    return status;
  }

  // In most cases we use `SetupBuilder()` to setup all these options in the
  // request. But in this case we cannot because that might also set
  // `Content-Type` to the wrong value. Instead we have to explicitly list all
  // the options here. Somebody could write a clever meta-function to say
  // "set all the options except `ContentType`, but I think that is going to be
  // very hard to understand.
  builder.AddOption(request.template GetOption<EncryptionKey>());
  builder.AddOption(request.template GetOption<IfGenerationMatch>());
  builder.AddOption(request.template GetOption<IfGenerationNotMatch>());
  builder.AddOption(request.template GetOption<IfMetagenerationMatch>());
  builder.AddOption(request.template GetOption<IfMetagenerationNotMatch>());
  builder.AddOption(request.template GetOption<KmsKeyName>());
  builder.AddOption(request.template GetOption<PredefinedAcl>());
  builder.AddOption(request.template GetOption<Projection>());
  builder.AddOption(request.template GetOption<UserProject>());
  builder.AddOption(request.template GetOption<CustomHeader>());
  builder.AddOption(request.template GetOption<Fields>());
  builder.AddOption(request.template GetOption<IfMatchEtag>());
  builder.AddOption(request.template GetOption<IfNoneMatchEtag>());
  builder.AddOption(request.template GetOption<QuotaUser>());
  builder.AddOption(request.template GetOption<UploadContentLength>());
  SetupBuilderUserIp(builder, request);

  builder.AddQueryParameter("uploadType", "resumable");
  builder.AddHeader("Content-Type: application/json; charset=UTF-8");
  nlohmann::json resource;
  if (request.template HasOption<WithObjectMetadata>()) {
    resource = ObjectMetadataJsonForInsert(
        request.template GetOption<WithObjectMetadata>().value());
  }
  if (request.template HasOption<ContentEncoding>()) {
    resource["contentEncoding"] =
        request.template GetOption<ContentEncoding>().value();
  }
  if (request.template HasOption<ContentType>()) {
    resource["contentType"] = request.template GetOption<ContentType>().value();
  }
  if (request.template HasOption<Crc32cChecksumValue>()) {
    resource["crc32c"] =
        request.template GetOption<Crc32cChecksumValue>().value();
  }
  if (request.template HasOption<MD5HashValue>()) {
    resource["md5Hash"] = request.template GetOption<MD5HashValue>().value();
  }

  if (resource.empty()) {
    builder.AddQueryParameter("name", request.object_name());
  } else {
    resource["name"] = request.object_name();
  }

  std::string request_payload;
  if (!resource.empty()) {
    request_payload = resource.dump();
  }
  builder.AddHeader("Content-Length: " +
                    std::to_string(request_payload.size()));
  auto http_response = builder.BuildRequest().MakeRequest(request_payload);
  if (!http_response.ok()) {
    return std::move(http_response).status();
  }
  if (http_response->status_code >= HttpStatusCode::kMinNotSuccess) {
    return AsStatus(*http_response);
  }
  auto response =
      ResumableUploadResponse::FromHttpResponse(*std::move(http_response));
  if (!response.ok()) {
    return std::move(response).status();
  }
  if (response->upload_session_url.empty()) {
    std::ostringstream os;
    os << __func__ << " - invalid server response, parsed to " << *response;
    return Status(StatusCode::kInternal, std::move(os).str());
  }
  return std::unique_ptr<ResumableUploadSession>(
      absl::make_unique<CurlResumableUploadSession>(
          shared_from_this(), std::move(response->upload_session_url),
          std::move(request.template GetOption<CustomHeader>())));
}

CurlClient::CurlClient(google::cloud::Options options)
    : opts_(std::move(options)),
      backwards_compatibility_options_(
          MakeBackwardsCompatibleClientOptions(opts_)),
      x_goog_api_client_header_("x-goog-api-client: " + x_goog_api_client()),
      storage_endpoint_(JsonEndpoint(opts_)),
      upload_endpoint_(JsonUploadEndpoint(opts_)),
      xml_endpoint_(XmlEndpoint(opts_)),
      iam_endpoint_(IamEndpoint(opts_)),
      xml_enabled_(XmlEnabled()),
      generator_(google::cloud::internal::MakeDefaultPRNG()),
      storage_factory_(CreateHandleFactory(opts_)),
      upload_factory_(CreateHandleFactory(opts_)),
      xml_upload_factory_(CreateHandleFactory(opts_)),
      xml_download_factory_(CreateHandleFactory(opts_)) {
  CurlInitializeOnce(opts_);
}

StatusOr<ResumableUploadResponse> CurlClient::UploadChunk(
    UploadChunkRequest const& request) {
  CurlRequestBuilder builder(request.upload_session_url(), upload_factory_);
  auto status = SetupBuilder(builder, request, "PUT");
  if (!status.ok()) {
    return status;
  }
  builder.AddHeader(request.RangeHeader());
  builder.AddHeader("Content-Type: application/octet-stream");
  builder.AddHeader("Content-Length: " +
                    std::to_string(request.payload_size()));
  // We need to explicitly disable chunked transfer encoding. libcurl uses is by
  // default (at least in this case), and that wastes bandwidth as the content
  // length is known.
  builder.AddHeader("Transfer-Encoding:");
  auto response = builder.BuildRequest().MakeUploadRequest(request.payload());
  if (!response.ok()) {
    return std::move(response).status();
  }
  if (response->status_code < HttpStatusCode::kMinNotSuccess ||
      response->status_code == HttpStatusCode::kResumeIncomplete) {
    return ResumableUploadResponse::FromHttpResponse(*std::move(response));
  }
  return AsStatus(*response);
}

StatusOr<ResumableUploadResponse> CurlClient::QueryResumableUpload(
    QueryResumableUploadRequest const& request) {
  CurlRequestBuilder builder(request.upload_session_url(), upload_factory_);
  auto status = SetupBuilder(builder, request, "PUT");
  if (!status.ok()) {
    return status;
  }
  builder.AddHeader("Content-Range: bytes */*");
  builder.AddHeader("Content-Type: application/octet-stream");
  builder.AddHeader("Content-Length: 0");
  auto response = builder.BuildRequest().MakeRequest(std::string{});
  if (!response.ok()) {
    return std::move(response).status();
  }
  if (response->status_code < HttpStatusCode::kMinNotSuccess ||
      response->status_code == HttpStatusCode::kResumeIncomplete) {
    return ResumableUploadResponse::FromHttpResponse(*std::move(response));
  }
  return AsStatus(*response);
}

StatusOr<ListBucketsResponse> CurlClient::ListBuckets(
    ListBucketsRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b", storage_factory_);
  auto status = SetupBuilder(builder, request, "GET");
  if (!status.ok()) {
    return status;
  }
  builder.AddQueryParameter("project", request.project_id());
  return ParseFromHttpResponse<ListBucketsResponse>(
      builder.BuildRequest().MakeRequest(std::string{}));
}

StatusOr<BucketMetadata> CurlClient::CreateBucket(
    CreateBucketRequest const& request) {
  // Assume the bucket name is validated by the caller.
  CurlRequestBuilder builder(storage_endpoint_ + "/b", storage_factory_);
  auto status = SetupBuilder(builder, request, "POST");
  if (!status.ok()) {
    return status;
  }
  builder.AddQueryParameter("project", request.project_id());
  builder.AddHeader("Content-Type: application/json");
  return CheckedFromString<BucketMetadataParser>(
      builder.BuildRequest().MakeRequest(request.json_payload()));
}

StatusOr<BucketMetadata> CurlClient::GetBucketMetadata(
    GetBucketMetadataRequest const& request) {
  // Assume the bucket name is validated by the caller.
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name(),
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "GET");
  if (!status.ok()) {
    return status;
  }
  return CheckedFromString<BucketMetadataParser>(
      builder.BuildRequest().MakeRequest(std::string{}));
}

StatusOr<EmptyResponse> CurlClient::DeleteBucket(
    DeleteBucketRequest const& request) {
  // Assume the bucket name is validated by the caller.
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name(),
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "DELETE");
  if (!status.ok()) {
    return status;
  }
  return ReturnEmptyResponse(builder.BuildRequest().MakeRequest(std::string{}));
}

StatusOr<BucketMetadata> CurlClient::UpdateBucket(
    UpdateBucketRequest const& request) {
  // Assume the bucket name is validated by the caller.
  CurlRequestBuilder builder(
      storage_endpoint_ + "/b/" + request.metadata().name(), storage_factory_);
  auto status = SetupBuilder(builder, request, "PUT");
  if (!status.ok()) {
    return status;
  }
  builder.AddHeader("Content-Type: application/json");
  return CheckedFromString<BucketMetadataParser>(
      builder.BuildRequest().MakeRequest(request.json_payload()));
}

StatusOr<BucketMetadata> CurlClient::PatchBucket(
    PatchBucketRequest const& request) {
  // Assume the bucket name is validated by the caller.
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket(),
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "PATCH");
  if (!status.ok()) {
    return status;
  }
  builder.AddHeader("Content-Type: application/json");
  return CheckedFromString<BucketMetadataParser>(
      builder.BuildRequest().MakeRequest(request.payload()));
}

StatusOr<IamPolicy> CurlClient::GetBucketIamPolicy(
    GetBucketIamPolicyRequest const& request) {
  CurlRequestBuilder builder(
      storage_endpoint_ + "/b/" + request.bucket_name() + "/iam",
      storage_factory_);
  auto status = SetupBuilder(builder, request, "GET");
  if (!status.ok()) {
    return status;
  }
  auto response = builder.BuildRequest().MakeRequest(std::string{});
  if (!response.ok()) {
    return std::move(response).status();
  }
  if (response->status_code >= HttpStatusCode::kMinNotSuccess) {
    return AsStatus(*response);
  }
  return ParseIamPolicyFromString(response->payload);
}

StatusOr<NativeIamPolicy> CurlClient::GetNativeBucketIamPolicy(
    GetBucketIamPolicyRequest const& request) {
  CurlRequestBuilder builder(
      storage_endpoint_ + "/b/" + request.bucket_name() + "/iam",
      storage_factory_);
  auto status = SetupBuilder(builder, request, "GET");
  if (!status.ok()) {
    return status;
  }
  auto response = builder.BuildRequest().MakeRequest(std::string{});
  if (!response.ok()) {
    return std::move(response).status();
  }
  if (response->status_code >= HttpStatusCode::kMinNotSuccess) {
    return AsStatus(*response);
  }
  return NativeIamPolicy::CreateFromJson(response->payload);
}

StatusOr<IamPolicy> CurlClient::SetBucketIamPolicy(
    SetBucketIamPolicyRequest const& request) {
  CurlRequestBuilder builder(
      storage_endpoint_ + "/b/" + request.bucket_name() + "/iam",
      storage_factory_);
  auto status = SetupBuilder(builder, request, "PUT");
  if (!status.ok()) {
    return status;
  }
  builder.AddHeader("Content-Type: application/json");
  auto response = builder.BuildRequest().MakeRequest(request.json_payload());
  if (!response.ok()) {
    return std::move(response).status();
  }
  if (response->status_code >= HttpStatusCode::kMinNotSuccess) {
    return AsStatus(*response);
  }
  return ParseIamPolicyFromString(response->payload);
}

StatusOr<NativeIamPolicy> CurlClient::SetNativeBucketIamPolicy(
    SetNativeBucketIamPolicyRequest const& request) {
  CurlRequestBuilder builder(
      storage_endpoint_ + "/b/" + request.bucket_name() + "/iam",
      storage_factory_);
  auto status = SetupBuilder(builder, request, "PUT");
  if (!status.ok()) {
    return status;
  }
  builder.AddHeader("Content-Type: application/json");
  auto response = builder.BuildRequest().MakeRequest(request.json_payload());
  if (!response.ok()) {
    return std::move(response).status();
  }
  if (response->status_code >= HttpStatusCode::kMinNotSuccess) {
    return AsStatus(*response);
  }
  return NativeIamPolicy::CreateFromJson(response->payload);
}

StatusOr<TestBucketIamPermissionsResponse> CurlClient::TestBucketIamPermissions(
    TestBucketIamPermissionsRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                                 "/iam/testPermissions",
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "GET");
  if (!status.ok()) {
    return status;
  }
  for (auto const& perm : request.permissions()) {
    builder.AddQueryParameter("permissions", perm);
  }
  auto response = builder.BuildRequest().MakeRequest(std::string{});
  if (!response.ok()) {
    return std::move(response).status();
  }
  if (response->status_code >= HttpStatusCode::kMinNotSuccess) {
    return AsStatus(*response);
  }
  return TestBucketIamPermissionsResponse::FromHttpResponse(response->payload);
}

StatusOr<BucketMetadata> CurlClient::LockBucketRetentionPolicy(
    LockBucketRetentionPolicyRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                                 "/lockRetentionPolicy",
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "POST");
  if (!status.ok()) {
    return status;
  }
  builder.AddHeader("content-type: application/json");
  builder.AddHeader("content-length: 0");
  builder.AddOption(IfMetagenerationMatch(request.metageneration()));
  return CheckedFromString<BucketMetadataParser>(
      builder.BuildRequest().MakeRequest(std::string{}));
}

StatusOr<ObjectMetadata> CurlClient::InsertObjectMedia(
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
  if (!request.GetOption<DisableMD5Hash>().value() ||
      !request.GetOption<DisableCrc32cChecksum>().value_or(false)) {
    return InsertObjectMediaMultipart(request);
  }

  // Otherwise do a simple upload.
  return InsertObjectMediaSimple(request);
}

StatusOr<ObjectMetadata> CurlClient::CopyObject(
    CopyObjectRequest const& request) {
  CurlRequestBuilder builder(
      storage_endpoint_ + "/b/" + request.source_bucket() + "/o/" +
          UrlEscapeString(request.source_object()) + "/copyTo/b/" +
          request.destination_bucket() + "/o/" +
          UrlEscapeString(request.destination_object()),
      storage_factory_);
  auto status = SetupBuilder(builder, request, "POST");
  if (!status.ok()) {
    return status;
  }
  builder.AddHeader("Content-Type: application/json");
  std::string json_payload("{}");
  if (request.HasOption<WithObjectMetadata>()) {
    json_payload = ObjectMetadataJsonForCopy(
                       request.GetOption<WithObjectMetadata>().value())
                       .dump();
  }
  return CheckedFromString<ObjectMetadataParser>(
      builder.BuildRequest().MakeRequest(json_payload));
}

StatusOr<ObjectMetadata> CurlClient::GetObjectMetadata(
    GetObjectMetadataRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                                 "/o/" + UrlEscapeString(request.object_name()),
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "GET");
  if (!status.ok()) {
    return status;
  }
  return CheckedFromString<ObjectMetadataParser>(
      builder.BuildRequest().MakeRequest(std::string{}));
}

StatusOr<std::unique_ptr<ObjectReadSource>> CurlClient::ReadObject(
    ReadObjectRangeRequest const& request) {
  if (xml_enabled_ && !request.HasOption<IfMetagenerationNotMatch>() &&
      !request.HasOption<IfGenerationNotMatch>() &&
      !request.HasOption<IfMetagenerationMatch>() &&
      !request.HasOption<IfGenerationMatch>() &&
      !request.HasOption<QuotaUser>() && !request.HasOption<UserIp>()) {
    return ReadObjectXml(request);
  }
  // Assume the bucket name is validated by the caller.
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                                 "/o/" + UrlEscapeString(request.object_name()),
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "GET");
  if (!status.ok()) {
    return status;
  }
  builder.AddQueryParameter("alt", "media");
  if (request.RequiresRangeHeader()) {
    builder.AddHeader(request.RangeHeader());
  }
  if (request.RequiresNoCache()) {
    builder.AddHeader("Cache-Control: no-transform");
  }

  return std::unique_ptr<ObjectReadSource>(
      new CurlDownloadRequest(builder.BuildDownloadRequest(std::string{})));
}

StatusOr<ListObjectsResponse> CurlClient::ListObjects(
    ListObjectsRequest const& request) {
  // Assume the bucket name is validated by the caller.
  CurlRequestBuilder builder(
      storage_endpoint_ + "/b/" + request.bucket_name() + "/o",
      storage_factory_);
  auto status = SetupBuilder(builder, request, "GET");
  if (!status.ok()) {
    return status;
  }
  builder.AddQueryParameter("pageToken", request.page_token());
  return ParseFromHttpResponse<ListObjectsResponse>(
      builder.BuildRequest().MakeRequest(std::string{}));
}

StatusOr<EmptyResponse> CurlClient::DeleteObject(
    DeleteObjectRequest const& request) {
  // Assume the bucket name is validated by the caller.
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                                 "/o/" + UrlEscapeString(request.object_name()),
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "DELETE");
  if (!status.ok()) {
    return status;
  }
  return ReturnEmptyResponse(builder.BuildRequest().MakeRequest(std::string{}));
}

StatusOr<ObjectMetadata> CurlClient::UpdateObject(
    UpdateObjectRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                                 "/o/" + UrlEscapeString(request.object_name()),
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "PUT");
  if (!status.ok()) {
    return status;
  }
  builder.AddHeader("Content-Type: application/json");
  return CheckedFromString<ObjectMetadataParser>(
      builder.BuildRequest().MakeRequest(request.json_payload()));
}

StatusOr<ObjectMetadata> CurlClient::PatchObject(
    PatchObjectRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                                 "/o/" + UrlEscapeString(request.object_name()),
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "PATCH");
  if (!status.ok()) {
    return status;
  }
  builder.AddHeader("Content-Type: application/json");
  return CheckedFromString<ObjectMetadataParser>(
      builder.BuildRequest().MakeRequest(request.payload()));
}

StatusOr<ObjectMetadata> CurlClient::ComposeObject(
    ComposeObjectRequest const& request) {
  CurlRequestBuilder builder(
      storage_endpoint_ + "/b/" + request.bucket_name() + "/o/" +
          UrlEscapeString(request.object_name()) + "/compose",
      storage_factory_);
  auto status = SetupBuilder(builder, request, "POST");
  if (!status.ok()) {
    return status;
  }
  builder.AddHeader("Content-Type: application/json");
  return CheckedFromString<ObjectMetadataParser>(
      builder.BuildRequest().MakeRequest(request.JsonPayload()));
}

StatusOr<RewriteObjectResponse> CurlClient::RewriteObject(
    RewriteObjectRequest const& request) {
  CurlRequestBuilder builder(
      storage_endpoint_ + "/b/" + request.source_bucket() + "/o/" +
          UrlEscapeString(request.source_object()) + "/rewriteTo/b/" +
          request.destination_bucket() + "/o/" +
          UrlEscapeString(request.destination_object()),
      storage_factory_);
  auto status = SetupBuilder(builder, request, "POST");
  if (!status.ok()) {
    return status;
  }
  if (!request.rewrite_token().empty()) {
    builder.AddQueryParameter("rewriteToken", request.rewrite_token());
  }
  builder.AddHeader("Content-Type: application/json");
  std::string json_payload("{}");
  if (request.HasOption<WithObjectMetadata>()) {
    json_payload = ObjectMetadataJsonForRewrite(
                       request.GetOption<WithObjectMetadata>().value())
                       .dump();
  }
  auto response = builder.BuildRequest().MakeRequest(json_payload);
  if (!response.ok()) {
    return std::move(response).status();
  }
  if (response->status_code >= HttpStatusCode::kMinNotSuccess) {
    return AsStatus(*response);
  }
  // This one does not use the common "ParseFromHttpResponse" function because
  // it takes different arguments.
  return RewriteObjectResponse::FromHttpResponse(response->payload);
}

StatusOr<std::unique_ptr<ResumableUploadSession>>
CurlClient::CreateResumableSession(ResumableUploadRequest const& request) {
  return CreateResumableSessionGeneric(request);
}

StatusOr<std::unique_ptr<ResumableUploadSession>>
CurlClient::RestoreResumableSession(std::string const& session_id) {
  auto session = absl::make_unique<CurlResumableUploadSession>(
      shared_from_this(), session_id);
  auto response = session->ResetSession();
  if (response.status().ok()) {
    return std::unique_ptr<ResumableUploadSession>(std::move(session));
  }
  return std::move(response).status();
}

StatusOr<EmptyResponse> CurlClient::DeleteResumableUpload(
    DeleteResumableUploadRequest const& request) {
  CurlRequestBuilder builder(request.upload_session_url(), upload_factory_);
  auto status = SetupBuilderCommon(builder, "DELETE");
  if (!status.ok()) {
    return status;
  }
  auto response = builder.BuildRequest().MakeRequest(std::string{});
  if (!response.ok()) {
    return std::move(response).status();
  }
  if (response->status_code >= HttpStatusCode::kMinNotSuccess &&
      response->status_code != 499) {
    return AsStatus(*response);
  }
  return EmptyResponse{};
}

StatusOr<ListBucketAclResponse> CurlClient::ListBucketAcl(
    ListBucketAclRequest const& request) {
  CurlRequestBuilder builder(
      storage_endpoint_ + "/b/" + request.bucket_name() + "/acl",
      storage_factory_);
  auto status = SetupBuilder(builder, request, "GET");
  if (!status.ok()) {
    return status;
  }
  auto response = builder.BuildRequest().MakeRequest(std::string{});
  if (!response.ok()) {
    return std::move(response).status();
  }
  if (response->status_code >= HttpStatusCode::kMinNotSuccess) {
    return AsStatus(*response);
  }
  return internal::ListBucketAclResponse::FromHttpResponse(response->payload);
}

StatusOr<BucketAccessControl> CurlClient::GetBucketAcl(
    GetBucketAclRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                                 "/acl/" + UrlEscapeString(request.entity()),
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "GET");
  if (!status.ok()) {
    return status;
  }
  return CheckedFromString<internal::BucketAccessControlParser>(
      builder.BuildRequest().MakeRequest(std::string{}));
}

StatusOr<BucketAccessControl> CurlClient::CreateBucketAcl(
    CreateBucketAclRequest const& request) {
  CurlRequestBuilder builder(
      storage_endpoint_ + "/b/" + request.bucket_name() + "/acl",
      storage_factory_);
  auto status = SetupBuilder(builder, request, "POST");
  if (!status.ok()) {
    return status;
  }
  builder.AddHeader("Content-Type: application/json");
  nlohmann::json object;
  object["entity"] = request.entity();
  object["role"] = request.role();
  return CheckedFromString<internal::BucketAccessControlParser>(
      builder.BuildRequest().MakeRequest(object.dump()));
}

StatusOr<EmptyResponse> CurlClient::DeleteBucketAcl(
    DeleteBucketAclRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                                 "/acl/" + UrlEscapeString(request.entity()),
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "DELETE");
  if (!status.ok()) {
    return status;
  }
  return ReturnEmptyResponse(builder.BuildRequest().MakeRequest(std::string{}));
}

StatusOr<BucketAccessControl> CurlClient::UpdateBucketAcl(
    UpdateBucketAclRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                                 "/acl/" + UrlEscapeString(request.entity()),
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "PUT");
  if (!status.ok()) {
    return status;
  }
  builder.AddHeader("Content-Type: application/json");
  nlohmann::json patch;
  patch["entity"] = request.entity();
  patch["role"] = request.role();
  return CheckedFromString<internal::BucketAccessControlParser>(
      builder.BuildRequest().MakeRequest(patch.dump()));
}

StatusOr<BucketAccessControl> CurlClient::PatchBucketAcl(
    PatchBucketAclRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                                 "/acl/" + UrlEscapeString(request.entity()),
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "PATCH");
  if (!status.ok()) {
    return status;
  }
  builder.AddHeader("Content-Type: application/json");
  return CheckedFromString<internal::BucketAccessControlParser>(
      builder.BuildRequest().MakeRequest(request.payload()));
}

StatusOr<ListObjectAclResponse> CurlClient::ListObjectAcl(
    ListObjectAclRequest const& request) {
  // Assume the bucket name is validated by the caller.
  CurlRequestBuilder builder(
      storage_endpoint_ + "/b/" + request.bucket_name() + "/o/" +
          UrlEscapeString(request.object_name()) + "/acl",
      storage_factory_);
  auto status = SetupBuilder(builder, request, "GET");
  if (!status.ok()) {
    return status;
  }
  return ParseFromHttpResponse<ListObjectAclResponse>(
      builder.BuildRequest().MakeRequest(std::string{}));
}

StatusOr<ObjectAccessControl> CurlClient::CreateObjectAcl(
    CreateObjectAclRequest const& request) {
  CurlRequestBuilder builder(
      storage_endpoint_ + "/b/" + request.bucket_name() + "/o/" +
          UrlEscapeString(request.object_name()) + "/acl",
      storage_factory_);
  auto status = SetupBuilder(builder, request, "POST");
  if (!status.ok()) {
    return status;
  }
  builder.AddHeader("Content-Type: application/json");
  nlohmann::json object;
  object["entity"] = request.entity();
  object["role"] = request.role();
  return CheckedFromString<ObjectAccessControlParser>(
      builder.BuildRequest().MakeRequest(object.dump()));
}

StatusOr<EmptyResponse> CurlClient::DeleteObjectAcl(
    DeleteObjectAclRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                                 "/o/" +
                                 UrlEscapeString(request.object_name()) +
                                 "/acl/" + UrlEscapeString(request.entity()),
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "DELETE");
  if (!status.ok()) {
    return status;
  }
  return ReturnEmptyResponse(builder.BuildRequest().MakeRequest(std::string{}));
}

StatusOr<ObjectAccessControl> CurlClient::GetObjectAcl(
    GetObjectAclRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                                 "/o/" +
                                 UrlEscapeString(request.object_name()) +
                                 "/acl/" + UrlEscapeString(request.entity()),
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "GET");
  if (!status.ok()) {
    return status;
  }
  return CheckedFromString<ObjectAccessControlParser>(
      builder.BuildRequest().MakeRequest(std::string{}));
}

StatusOr<ObjectAccessControl> CurlClient::UpdateObjectAcl(
    UpdateObjectAclRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                                 "/o/" +
                                 UrlEscapeString(request.object_name()) +
                                 "/acl/" + UrlEscapeString(request.entity()),
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "PUT");
  if (!status.ok()) {
    return status;
  }
  builder.AddHeader("Content-Type: application/json");
  nlohmann::json object;
  object["entity"] = request.entity();
  object["role"] = request.role();
  return CheckedFromString<ObjectAccessControlParser>(
      builder.BuildRequest().MakeRequest(object.dump()));
}

StatusOr<ObjectAccessControl> CurlClient::PatchObjectAcl(
    PatchObjectAclRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                                 "/o/" +
                                 UrlEscapeString(request.object_name()) +
                                 "/acl/" + UrlEscapeString(request.entity()),
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "PATCH");
  if (!status.ok()) {
    return status;
  }
  builder.AddHeader("Content-Type: application/json");
  return CheckedFromString<ObjectAccessControlParser>(
      builder.BuildRequest().MakeRequest(request.payload()));
}

StatusOr<ListDefaultObjectAclResponse> CurlClient::ListDefaultObjectAcl(
    ListDefaultObjectAclRequest const& request) {
  // Assume the bucket name is validated by the caller.
  CurlRequestBuilder builder(
      storage_endpoint_ + "/b/" + request.bucket_name() + "/defaultObjectAcl",
      storage_factory_);
  auto status = SetupBuilder(builder, request, "GET");
  if (!status.ok()) {
    return status;
  }
  return ParseFromHttpResponse<ListDefaultObjectAclResponse>(
      builder.BuildRequest().MakeRequest(std::string{}));
}

StatusOr<ObjectAccessControl> CurlClient::CreateDefaultObjectAcl(
    CreateDefaultObjectAclRequest const& request) {
  CurlRequestBuilder builder(
      storage_endpoint_ + "/b/" + request.bucket_name() + "/defaultObjectAcl",
      storage_factory_);
  auto status = SetupBuilder(builder, request, "POST");
  if (!status.ok()) {
    return status;
  }
  nlohmann::json object;
  object["entity"] = request.entity();
  object["role"] = request.role();
  builder.AddHeader("Content-Type: application/json");
  return CheckedFromString<ObjectAccessControlParser>(
      builder.BuildRequest().MakeRequest(object.dump()));
}

StatusOr<EmptyResponse> CurlClient::DeleteDefaultObjectAcl(
    DeleteDefaultObjectAclRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                                 "/defaultObjectAcl/" +
                                 UrlEscapeString(request.entity()),
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "DELETE");
  if (!status.ok()) {
    return status;
  }
  return ReturnEmptyResponse(builder.BuildRequest().MakeRequest(std::string{}));
}

StatusOr<ObjectAccessControl> CurlClient::GetDefaultObjectAcl(
    GetDefaultObjectAclRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                                 "/defaultObjectAcl/" +
                                 UrlEscapeString(request.entity()),
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "GET");
  if (!status.ok()) {
    return status;
  }
  return CheckedFromString<ObjectAccessControlParser>(
      builder.BuildRequest().MakeRequest(std::string{}));
}

StatusOr<ObjectAccessControl> CurlClient::UpdateDefaultObjectAcl(
    UpdateDefaultObjectAclRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                                 "/defaultObjectAcl/" +
                                 UrlEscapeString(request.entity()),
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "PUT");
  if (!status.ok()) {
    return status;
  }
  builder.AddHeader("Content-Type: application/json");
  nlohmann::json object;
  object["entity"] = request.entity();
  object["role"] = request.role();
  return CheckedFromString<ObjectAccessControlParser>(
      builder.BuildRequest().MakeRequest(object.dump()));
}

StatusOr<ObjectAccessControl> CurlClient::PatchDefaultObjectAcl(
    PatchDefaultObjectAclRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                                 "/defaultObjectAcl/" +
                                 UrlEscapeString(request.entity()),
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "PATCH");
  if (!status.ok()) {
    return status;
  }
  builder.AddHeader("Content-Type: application/json");
  return CheckedFromString<ObjectAccessControlParser>(
      builder.BuildRequest().MakeRequest(request.payload()));
}

StatusOr<ServiceAccount> CurlClient::GetServiceAccount(
    GetProjectServiceAccountRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/projects/" +
                                 request.project_id() + "/serviceAccount",
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "GET");
  if (!status.ok()) {
    return status;
  }
  return CheckedFromString<ServiceAccountParser>(
      builder.BuildRequest().MakeRequest(std::string{}));
}

StatusOr<ListHmacKeysResponse> CurlClient::ListHmacKeys(
    ListHmacKeysRequest const& request) {
  CurlRequestBuilder builder(
      storage_endpoint_ + "/projects/" + request.project_id() + "/hmacKeys",
      storage_factory_);
  auto status = SetupBuilder(builder, request, "GET");
  if (!status.ok()) {
    return status;
  }
  return ParseFromHttpResponse<ListHmacKeysResponse>(
      builder.BuildRequest().MakeRequest(std::string{}));
}

StatusOr<CreateHmacKeyResponse> CurlClient::CreateHmacKey(
    CreateHmacKeyRequest const& request) {
  CurlRequestBuilder builder(
      storage_endpoint_ + "/projects/" + request.project_id() + "/hmacKeys",
      storage_factory_);
  auto status = SetupBuilder(builder, request, "POST");
  if (!status.ok()) {
    return status;
  }
  builder.AddQueryParameter("serviceAccountEmail", request.service_account());
  builder.AddHeader("content-length: 0");
  return ParseFromHttpResponse<CreateHmacKeyResponse>(
      builder.BuildRequest().MakeRequest(std::string{}));
}

StatusOr<EmptyResponse> CurlClient::DeleteHmacKey(
    DeleteHmacKeyRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/projects/" +
                                 request.project_id() + "/hmacKeys/" +
                                 request.access_id(),
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "DELETE");
  if (!status.ok()) {
    return status;
  }
  return ReturnEmptyResponse(builder.BuildRequest().MakeRequest(std::string{}));
}

StatusOr<HmacKeyMetadata> CurlClient::GetHmacKey(
    GetHmacKeyRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/projects/" +
                                 request.project_id() + "/hmacKeys/" +
                                 request.access_id(),
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "GET");
  if (!status.ok()) {
    return status;
  }
  return CheckedFromString<HmacKeyMetadataParser>(
      builder.BuildRequest().MakeRequest(std::string{}));
}

StatusOr<HmacKeyMetadata> CurlClient::UpdateHmacKey(
    UpdateHmacKeyRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/projects/" +
                                 request.project_id() + "/hmacKeys/" +
                                 request.access_id(),
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "PUT");
  if (!status.ok()) {
    return status;
  }
  nlohmann::json payload;
  if (!request.resource().state().empty()) {
    payload["state"] = request.resource().state();
  }
  if (!request.resource().etag().empty()) {
    payload["etag"] = request.resource().etag();
  }
  builder.AddHeader("Content-Type: application/json");
  return CheckedFromString<HmacKeyMetadataParser>(
      builder.BuildRequest().MakeRequest(payload.dump()));
}

StatusOr<SignBlobResponse> CurlClient::SignBlob(
    SignBlobRequest const& request) {
  CurlRequestBuilder builder(iam_endpoint_ + "/projects/-/serviceAccounts/" +
                                 request.service_account() + ":signBlob",
                             storage_factory_);
  auto status = SetupBuilderCommon(builder, "POST", "iamcredentials");
  if (!status.ok()) {
    return status;
  }
  nlohmann::json payload;
  payload["payload"] = request.base64_encoded_blob();
  if (!request.delegates().empty()) {
    payload["delegates"] = request.delegates();
  }
  builder.AddHeader("Content-Type: application/json");
  return ParseFromHttpResponse<SignBlobResponse>(
      builder.BuildRequest().MakeRequest(payload.dump()));
}

StatusOr<ListNotificationsResponse> CurlClient::ListNotifications(
    ListNotificationsRequest const& request) {
  // Assume the bucket name is validated by the caller.
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                                 "/notificationConfigs",
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "GET");
  if (!status.ok()) {
    return status;
  }
  return ParseFromHttpResponse<ListNotificationsResponse>(
      builder.BuildRequest().MakeRequest(std::string{}));
}

StatusOr<NotificationMetadata> CurlClient::CreateNotification(
    CreateNotificationRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                                 "/notificationConfigs",
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "POST");
  if (!status.ok()) {
    return status;
  }
  builder.AddHeader("Content-Type: application/json");
  return CheckedFromString<NotificationMetadataParser>(
      builder.BuildRequest().MakeRequest(request.json_payload()));
}

StatusOr<NotificationMetadata> CurlClient::GetNotification(
    GetNotificationRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                                 "/notificationConfigs/" +
                                 request.notification_id(),
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "GET");
  if (!status.ok()) {
    return status;
  }
  return CheckedFromString<NotificationMetadataParser>(
      builder.BuildRequest().MakeRequest(std::string{}));
}

StatusOr<EmptyResponse> CurlClient::DeleteNotification(
    DeleteNotificationRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                                 "/notificationConfigs/" +
                                 request.notification_id(),
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "DELETE");
  if (!status.ok()) {
    return status;
  }
  return ReturnEmptyResponse(builder.BuildRequest().MakeRequest(std::string{}));
}

StatusOr<ObjectMetadata> CurlClient::InsertObjectMediaXml(
    InsertObjectMediaRequest const& request) {
  CurlRequestBuilder builder(xml_endpoint_ + "/" + request.bucket_name() + "/" +
                                 UrlEscapeString(request.object_name()),
                             xml_upload_factory_);
  auto status = SetupBuilderCommon(builder, "PUT");
  if (!status.ok()) {
    return status;
  }

  //
  // Apply the options from InsertObjectMediaRequest that are set, translating
  // to the XML format for them.
  //
  builder.AddOption(request.GetOption<ContentEncoding>());
  // Set the content type of a sensible value, the application can override this
  // in the options for the request.
  if (!request.HasOption<ContentType>()) {
    builder.AddHeader("content-type: application/octet-stream");
  } else {
    builder.AddOption(request.GetOption<ContentType>());
  }
  builder.AddOption(request.GetOption<EncryptionKey>());
  if (request.HasOption<IfGenerationMatch>()) {
    builder.AddHeader(
        "x-goog-if-generation-match: " +
        std::to_string(request.GetOption<IfGenerationMatch>().value()));
  }
  // IfGenerationNotMatch cannot be set, checked by the caller.
  if (request.HasOption<IfMetagenerationMatch>()) {
    builder.AddHeader(
        "x-goog-if-metageneration-match: " +
        std::to_string(request.GetOption<IfMetagenerationMatch>().value()));
  }
  // IfMetagenerationNotMatch cannot be set, checked by the caller.
  if (request.HasOption<KmsKeyName>()) {
    builder.AddHeader("x-goog-encryption-kms-key-name: " +
                      request.GetOption<KmsKeyName>().value());
  }
  if (request.HasOption<MD5HashValue>()) {
    builder.AddHeader("x-goog-hash: md5=" +
                      request.GetOption<MD5HashValue>().value());
  } else if (!request.GetOption<DisableMD5Hash>().value()) {
    builder.AddHeader("x-goog-hash: md5=" + ComputeMD5Hash(request.contents()));
  }
  if (request.HasOption<Crc32cChecksumValue>()) {
    builder.AddHeader("x-goog-hash: crc32c=" +
                      request.GetOption<Crc32cChecksumValue>().value());
  } else if (!request.GetOption<DisableCrc32cChecksum>().value_or(false)) {
    builder.AddHeader("x-goog-hash: crc32c=" +
                      ComputeCrc32cChecksum(request.contents()));
  }
  if (request.HasOption<PredefinedAcl>()) {
    builder.AddHeader("x-goog-acl: " +
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

  builder.AddHeader("Content-Length: " +
                    std::to_string(request.contents().size()));
  auto response = builder.BuildRequest().MakeRequest(request.contents());
  if (!response.ok()) {
    return std::move(response).status();
  }
  if (response->status_code >= HttpStatusCode::kMinNotSuccess) {
    return AsStatus(*response);
  }
  return internal::ObjectMetadataParser::FromJson(nlohmann::json{
      {"name", request.object_name()},
      {"bucket", request.bucket_name()},
  });
}

StatusOr<std::unique_ptr<ObjectReadSource>> CurlClient::ReadObjectXml(
    ReadObjectRangeRequest const& request) {
  CurlRequestBuilder builder(xml_endpoint_ + "/" + request.bucket_name() + "/" +
                                 UrlEscapeString(request.object_name()),
                             xml_download_factory_);
  auto status = SetupBuilderCommon(builder, "GET");
  if (!status.ok()) {
    return status;
  }

  //
  // Apply the options from ReadObjectMediaRequest that are set, translating
  // to the XML format for them.
  //
  builder.AddOption(request.GetOption<EncryptionKey>());
  builder.AddOption(request.GetOption<Generation>());
  // None of the IfGeneration*Match nor IfMetageneration*Match can be set. This
  // is checked by the caller (in this class).
  builder.AddOption(request.GetOption<UserProject>());

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
    builder.AddHeader(request.RangeHeader());
  }
  if (request.RequiresNoCache()) {
    builder.AddHeader("Cache-Control: no-transform");
  }

  return std::unique_ptr<ObjectReadSource>(
      new CurlDownloadRequest(builder.BuildDownloadRequest(std::string{})));
}

StatusOr<ObjectMetadata> CurlClient::InsertObjectMediaMultipart(
    InsertObjectMediaRequest const& request) {
  // To perform a multipart upload we need to separate the parts as described
  // in:
  //   https://cloud.google.com/storage/docs/uploading-objects#rest-upload-objects
  // This function is structured as follows:
  // 1. Create a request object, as we often do.
  CurlRequestBuilder builder(
      upload_endpoint_ + "/b/" + request.bucket_name() + "/o", upload_factory_);
  auto status = SetupBuilder(builder, request, "POST");
  if (!status.ok()) {
    return status;
  }

  // 2. Pick a separator that does not conflict with the request contents.
  auto boundary = PickBoundary(request.contents());
  builder.AddHeader("content-type: multipart/related; boundary=" + boundary);
  builder.AddQueryParameter("uploadType", "multipart");
  builder.AddQueryParameter("name", request.object_name());

  // 3. Perform a streaming upload because computing the size upfront is more
  //    complicated than it is worth.
  std::ostringstream writer;

  nlohmann::json metadata = nlohmann::json::object();
  if (request.HasOption<WithObjectMetadata>()) {
    metadata = ObjectMetadataJsonForInsert(
        request.GetOption<WithObjectMetadata>().value());
  }
  if (request.HasOption<MD5HashValue>()) {
    metadata["md5Hash"] = request.GetOption<MD5HashValue>().value();
  } else if (!request.GetOption<DisableMD5Hash>().value()) {
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
  builder.AddHeader("Content-Length: " + std::to_string(contents.size()));
  return CheckedFromString<ObjectMetadataParser>(
      builder.BuildRequest().MakeRequest(contents));
}

std::string CurlClient::PickBoundary(std::string const& text_to_avoid) {
  // We need to find a string that is *not* found in `text_to_avoid`, we pick
  // a string at random, and see if it is in `text_to_avoid`, if it is, we grow
  // the string with random characters and start from where we last found a
  // the candidate.  Eventually we will find something, though it might be
  // larger than `text_to_avoid`.  And we only make (approximately) one pass
  // over `text_to_avoid`.
  auto generate_candidate = [this](int n) {
    static std::string const kChars =
        "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::unique_lock<std::mutex> lk(mu_);
    return google::cloud::internal::Sample(generator_, n, kChars);
  };
  constexpr int kCandidateInitialSize = 16;
  constexpr int kCandidateGrowthSize = 4;
  return GenerateMessageBoundary(text_to_avoid, std::move(generate_candidate),
                                 kCandidateInitialSize, kCandidateGrowthSize);
}

StatusOr<ObjectMetadata> CurlClient::InsertObjectMediaSimple(
    InsertObjectMediaRequest const& request) {
  CurlRequestBuilder builder(
      upload_endpoint_ + "/b/" + request.bucket_name() + "/o", upload_factory_);
  auto status = SetupBuilder(builder, request, "POST");
  if (!status.ok()) {
    return status;
  }
  // Set the content type of a sensible value, the application can override this
  // in the options for the request.
  if (!request.HasOption<ContentType>()) {
    builder.AddHeader("content-type: application/octet-stream");
  }
  builder.AddQueryParameter("uploadType", "media");
  builder.AddQueryParameter("name", request.object_name());
  builder.AddHeader("Content-Length: " +
                    std::to_string(request.contents().size()));
  return CheckedFromString<ObjectMetadataParser>(
      builder.BuildRequest().MakeRequest(request.contents()));
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
