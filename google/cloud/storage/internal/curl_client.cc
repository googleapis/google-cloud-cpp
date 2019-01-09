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
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/make_unique.h"
#include "google/cloud/storage/internal/curl_request_builder.h"
#include "google/cloud/storage/internal/curl_resumable_streambuf.h"
#include "google/cloud/storage/internal/curl_resumable_upload_session.h"
#include "google/cloud/storage/internal/curl_streambuf.h"
#include "google/cloud/storage/internal/generate_message_boundary.h"
#include "google/cloud/storage/object_stream.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

extern "C" void CurlShareLockCallback(CURL*, curl_lock_data, curl_lock_access,
                                      void* userptr) {
  auto* client = reinterpret_cast<CurlClient*>(userptr);
  client->LockShared();
}

extern "C" void CurlShareUnlockCallback(CURL*, curl_lock_data, void* userptr) {
  auto* client = reinterpret_cast<CurlClient*>(userptr);
  client->UnlockShared();
}

std::shared_ptr<CurlHandleFactory> CreateHandleFactory(
    ClientOptions const& options) {
  if (options.connection_pool_size() == 0U) {
    return std::make_shared<DefaultCurlHandleFactory>();
  }
  return std::make_shared<PooledCurlHandleFactory>(
      options.connection_pool_size());
}

std::unique_ptr<HashValidator> CreateHashValidator(bool disable_md5,
                                                   bool disable_crc32c) {
  if (disable_md5 and disable_crc32c) {
    return google::cloud::internal::make_unique<NullHashValidator>();
  }
  if (disable_md5) {
    return google::cloud::internal::make_unique<Crc32cHashValidator>();
  }
  if (disable_crc32c) {
    return google::cloud::internal::make_unique<MD5HashValidator>();
  }
  return google::cloud::internal::make_unique<CompositeValidator>(
      google::cloud::internal::make_unique<Crc32cHashValidator>(),
      google::cloud::internal::make_unique<MD5HashValidator>());
}

/// Create a HashValidator for a download request.
std::unique_ptr<HashValidator> CreateHashValidator(
    ReadObjectRangeRequest const& request) {
  if (request.HasOption<ReadRange>()) {
    return google::cloud::internal::make_unique<NullHashValidator>();
  }
  return CreateHashValidator(request.HasOption<DisableMD5Hash>(),
                             request.HasOption<DisableCrc32cChecksum>());
}

/// Create a HashValidator for an upload request.
std::unique_ptr<HashValidator> CreateHashValidator(
    InsertObjectStreamingRequest const& request) {
  return CreateHashValidator(request.HasOption<DisableMD5Hash>(),
                             request.HasOption<DisableCrc32cChecksum>());
}

/// Create a HashValidator for an insert request.
std::unique_ptr<HashValidator> CreateHashValidator(
    InsertObjectMediaRequest const&) {
  return google::cloud::internal::make_unique<NullHashValidator>();
}

std::string XmlMapPredefinedAcl(std::string const& acl) {
  static std::map<std::string, std::string> mapping{
      {"authenticatedRead", "authenticated-read"},
      {"bucketOwnerFullControl", "bucket-owner-full-control"},
      {"bucketOwnerRead", "bucket-owner-read"},
      {"private", "private"},
      {"projectPrivate", "project-private"},
      {"publicRead", "public-read"},
  };
  auto loc = mapping.find(acl);
  if (loc == mapping.end()) {
    return acl;
  }
  return loc->second;
}

std::string UrlEscapeString(std::string const& value) {
  CurlHandle handle;
  return std::string(handle.MakeEscapedString(value).get());
}

template <typename ReturnType>
StatusOr<ReturnType> ParseFromString(StatusOr<HttpResponse> response) {
  if (not response.ok()) {
    return std::move(response).status();
  }
  if (response->status_code >= 300) {
    return AsStatus(*response);
  }
  return ReturnType::ParseFromString(response->payload);
}

StatusOr<EmptyResponse> ReturnEmptyResponse(StatusOr<HttpResponse> response) {
  if (not response.ok()) {
    return std::move(response).status();
  }
  if (response->status_code >= 300) {
    return AsStatus(*response);
  }
  return EmptyResponse{};
}

template <typename ReturnType>
StatusOr<ReturnType> ParseFromHttpResponse(StatusOr<HttpResponse> response) {
  if (not response.ok()) {
    return std::move(response).status();
  }
  if (response->status_code >= 300) {
    return AsStatus(*response);
  }
  return ReturnType::FromHttpResponse(std::move(*response));
}

}  // namespace

Status CurlClient::SetupBuilderCommon(CurlRequestBuilder& builder,
                                      char const* method) {
  auto auth_header = AuthorizationHeader(options_.credentials());
  if (not auth_header.ok()) {
    return std::move(auth_header).status();
  }
  builder.SetMethod(method)
      .SetDebugLogging(options_.enable_http_tracing())
      .SetCurlShare(share_.get())
      .AddUserAgentPrefix(options_.user_agent_prefix())
      .AddHeader(auth_header.value());
  return Status();
}

template <typename Request>
Status CurlClient::SetupBuilder(CurlRequestBuilder& builder,
                                Request const& request, char const* method) {
  auto status = SetupBuilderCommon(builder, method);
  if (not status.ok()) {
    return status;
  }
  request.AddOptionsToHttpRequest(builder);
  if (request.template HasOption<UserIp>()) {
    std::string value = request.template GetOption<UserIp>().value();
    if (value.empty()) {
      value = builder.LastClientIpAddress();
    }
    if (not value.empty()) {
      builder.AddQueryParameter(UserIp::name(), value);
    }
  }
  return Status();
}

template <typename RequestType>
StatusOr<std::unique_ptr<ResumableUploadSession>>
CurlClient::CreateResumableSessionGeneric(RequestType const& request) {
  if (request.template HasOption<UseResumableUploadSession>()) {
    auto session_id =
        request.template GetOption<UseResumableUploadSession>().value();
    if (not session_id.empty()) {
      return RestoreResumableSession(session_id);
    }
  }

  CurlRequestBuilder builder(
      upload_endpoint_ + "/b/" + request.bucket_name() + "/o", upload_factory_);
  auto status = SetupBuilder(builder, request, "POST");
  if (not status.ok()) {
    return status;
  }
  builder.AddQueryParameter("uploadType", "resumable");
  builder.AddQueryParameter("name", request.object_name());
  builder.AddHeader("Content-Type: application/json; charset=UTF-8");
  std::string request_payload;
  if (request.template HasOption<WithObjectMetadata>()) {
    request_payload = request.template GetOption<WithObjectMetadata>()
                          .value()
                          .JsonPayloadForUpdate();
  }
  builder.AddHeader("Content-Length: " +
                    std::to_string(request_payload.size()));
  auto http_response = builder.BuildRequest().MakeRequest(request_payload);
  if (not http_response.ok()) {
    return std::move(http_response).status();
  }
  if (http_response->status_code >= 300) {
    return AsStatus(*http_response);
  }
  auto response =
      ResumableUploadResponse::FromHttpResponse(*std::move(http_response));
  if (not response.ok()) {
    return std::move(response).status();
  }
  if (response->upload_session_url.empty()) {
    std::ostringstream os;
    os << __func__ << " - invalid server response, parsed to " << *response;
    return Status(StatusCode::kInternal, std::move(os).str());
  }
  return std::unique_ptr<ResumableUploadSession>(
      google::cloud::internal::make_unique<CurlResumableUploadSession>(
          shared_from_this(), std::move(response->upload_session_url)));
}

CurlClient::CurlClient(ClientOptions options)
    : options_(std::move(options)),
      share_(curl_share_init(), &curl_share_cleanup),
      generator_(google::cloud::internal::MakeDefaultPRNG()),
      storage_factory_(CreateHandleFactory(options_)),
      upload_factory_(CreateHandleFactory(options_)),
      xml_upload_factory_(CreateHandleFactory(options_)),
      xml_download_factory_(CreateHandleFactory(options_)) {
  storage_endpoint_ = options_.endpoint() + "/storage/" + options_.version();
  upload_endpoint_ =
      options_.endpoint() + "/upload/storage/" + options_.version();

  auto endpoint =
      google::cloud::internal::GetEnv("CLOUD_STORAGE_TESTBENCH_ENDPOINT");
  if (endpoint.has_value()) {
    xml_upload_endpoint_ = options_.endpoint() + "/xmlapi";
    xml_download_endpoint_ = options_.endpoint() + "/xmlapi";
  } else {
    xml_upload_endpoint_ = "https://storage-upload.googleapis.com";
    xml_download_endpoint_ = "https://storage-download.googleapis.com";
  }

  curl_share_setopt(share_.get(), CURLSHOPT_LOCKFUNC, CurlShareLockCallback);
  curl_share_setopt(share_.get(), CURLSHOPT_UNLOCKFUNC,
                    CurlShareUnlockCallback);
  curl_share_setopt(share_.get(), CURLSHOPT_USERDATA, this);
  curl_share_setopt(share_.get(), CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);
  curl_share_setopt(share_.get(), CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
  curl_share_setopt(share_.get(), CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);

  CurlInitializeOnce(options.enable_ssl_locking_callbacks());
}

StatusOr<ResumableUploadResponse> CurlClient::UploadChunk(
    UploadChunkRequest const& request) {
  CurlRequestBuilder builder(request.upload_session_url(), upload_factory_);
  auto status = SetupBuilder(builder, request, "PUT");
  if (not status.ok()) {
    return status;
  }
  builder.AddHeader(request.RangeHeader());
  builder.AddHeader("Content-Type: application/octet-stream");
  builder.AddHeader("Content-Length: " +
                    std::to_string(request.payload().size()));
  auto response = builder.BuildRequest().MakeRequest(request.payload());
  if (not response.ok()) {
    return std::move(response).status();
  }
  if (response->status_code < 300 or response->status_code == 308) {
    return ResumableUploadResponse::FromHttpResponse(*std::move(response));
  }
  return AsStatus(*response);
}

StatusOr<ResumableUploadResponse> CurlClient::QueryResumableUpload(
    QueryResumableUploadRequest const& request) {
  CurlRequestBuilder builder(request.upload_session_url(), upload_factory_);
  auto status = SetupBuilder(builder, request, "PUT");
  if (not status.ok()) {
    return status;
  }
  builder.AddHeader("Content-Range: bytes */*");
  builder.AddHeader("Content-Type: application/octet-stream");
  builder.AddHeader("Content-Length: 0");
  auto response = builder.BuildRequest().MakeRequest(std::string{});
  if (not response.ok()) {
    return std::move(response).status();
  }
  if (response->status_code < 300 or response->status_code == 308) {
    return ResumableUploadResponse::FromHttpResponse(*std::move(response));
  }
  return AsStatus(*response);
}

StatusOr<ListBucketsResponse> CurlClient::ListBuckets(
    ListBucketsRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b", storage_factory_);
  auto status = SetupBuilder(builder, request, "GET");
  if (not status.ok()) {
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
  if (not status.ok()) {
    return status;
  }
  builder.AddQueryParameter("project", request.project_id());
  builder.AddHeader("Content-Type: application/json");
  return ParseFromString<BucketMetadata>(
      builder.BuildRequest().MakeRequest(request.json_payload()));
}

StatusOr<BucketMetadata> CurlClient::GetBucketMetadata(
    GetBucketMetadataRequest const& request) {
  // Assume the bucket name is validated by the caller.
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name(),
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "GET");
  if (not status.ok()) {
    return status;
  }
  return ParseFromString<BucketMetadata>(
      builder.BuildRequest().MakeRequest(std::string{}));
}

StatusOr<EmptyResponse> CurlClient::DeleteBucket(
    DeleteBucketRequest const& request) {
  // Assume the bucket name is validated by the caller.
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name(),
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "DELETE");
  if (not status.ok()) {
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
  if (not status.ok()) {
    return status;
  }
  builder.AddHeader("Content-Type: application/json");
  return ParseFromString<BucketMetadata>(
      builder.BuildRequest().MakeRequest(request.json_payload()));
}

StatusOr<BucketMetadata> CurlClient::PatchBucket(
    PatchBucketRequest const& request) {
  // Assume the bucket name is validated by the caller.
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket(),
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "PATCH");
  if (not status.ok()) {
    return status;
  }
  builder.AddHeader("Content-Type: application/json");
  return ParseFromString<BucketMetadata>(
      builder.BuildRequest().MakeRequest(request.payload()));
}

StatusOr<IamPolicy> CurlClient::GetBucketIamPolicy(
    GetBucketIamPolicyRequest const& request) {
  CurlRequestBuilder builder(
      storage_endpoint_ + "/b/" + request.bucket_name() + "/iam",
      storage_factory_);
  auto status = SetupBuilder(builder, request, "GET");
  if (not status.ok()) {
    return status;
  }
  auto response = builder.BuildRequest().MakeRequest(std::string{});
  if (not response.ok()) {
    return std::move(response).status();
  }
  if (response->status_code >= 300) {
    return AsStatus(*response);
  }
  return ParseIamPolicyFromString(response->payload);
}

StatusOr<IamPolicy> CurlClient::SetBucketIamPolicy(
    SetBucketIamPolicyRequest const& request) {
  CurlRequestBuilder builder(
      storage_endpoint_ + "/b/" + request.bucket_name() + "/iam",
      storage_factory_);
  auto status = SetupBuilder(builder, request, "PUT");
  if (not status.ok()) {
    return status;
  }
  builder.AddHeader("Content-Type: application/json");
  auto response = builder.BuildRequest().MakeRequest(request.json_payload());
  if (not response.ok()) {
    return std::move(response).status();
  }
  if (response->status_code >= 300) {
    return AsStatus(*response);
  }
  return ParseIamPolicyFromString(response->payload);
}

StatusOr<TestBucketIamPermissionsResponse> CurlClient::TestBucketIamPermissions(
    TestBucketIamPermissionsRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                                 "/iam/testPermissions",
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "GET");
  if (not status.ok()) {
    return status;
  }
  for (auto const& perm : request.permissions()) {
    builder.AddQueryParameter("permissions", perm);
  }
  auto response = builder.BuildRequest().MakeRequest(std::string{});
  if (not response.ok()) {
    return std::move(response).status();
  }
  if (response->status_code >= 300) {
    return AsStatus(*response);
  }
  return TestBucketIamPermissionsResponse::FromHttpResponse(*response);
}

StatusOr<EmptyResponse> CurlClient::LockBucketRetentionPolicy(
    LockBucketRetentionPolicyRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                                 "/lockRetentionPolicy",
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "POST");
  if (not status.ok()) {
    return status;
  }
  builder.AddHeader("content-type: application/json");
  builder.AddHeader("content-length: 0");
  builder.AddOption(IfMetagenerationMatch(request.metageneration()));
  return ReturnEmptyResponse(builder.BuildRequest().MakeRequest(std::string{}));
}

StatusOr<ObjectMetadata> CurlClient::InsertObjectMedia(
    InsertObjectMediaRequest const& request) {
  // If the object metadata is specified, then we need to do a multipart upload.
  if (request.HasOption<WithObjectMetadata>()) {
    return InsertObjectMediaMultipart(request);
  }

  // Unless the request uses a feature that disables it, prefer to use XML.
  if (not request.HasOption<IfMetagenerationNotMatch>() and
      not request.HasOption<IfGenerationNotMatch>() and
      not request.HasOption<QuotaUser>() and not request.HasOption<UserIp>() and
      not request.HasOption<Projection>() and request.HasOption<Fields>() and
      request.GetOption<Fields>().value().empty()) {
    return InsertObjectMediaXml(request);
  }

  // If the application has set an explicit hash value we need to use multipart
  // uploads.
  if (not request.HasOption<DisableMD5Hash>() and
      not request.HasOption<DisableCrc32cChecksum>()) {
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
  if (not status.ok()) {
    return status;
  }
  builder.AddHeader("Content-Type: application/json");
  std::string json_payload("{}");
  if (request.HasOption<WithObjectMetadata>()) {
    json_payload =
        request.GetOption<WithObjectMetadata>().value().JsonPayloadForCopy();
  }
  return ParseFromString<ObjectMetadata>(
      builder.BuildRequest().MakeRequest(json_payload));
}

StatusOr<ObjectMetadata> CurlClient::GetObjectMetadata(
    GetObjectMetadataRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                                 "/o/" + UrlEscapeString(request.object_name()),
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "GET");
  if (not status.ok()) {
    return status;
  }
  return ParseFromString<ObjectMetadata>(
      builder.BuildRequest().MakeRequest(std::string{}));
}

StatusOr<std::unique_ptr<ObjectReadStreambuf>> CurlClient::ReadObject(
    ReadObjectRangeRequest const& request) {
  if (not request.HasOption<IfMetagenerationNotMatch>() and
      not request.HasOption<IfGenerationNotMatch>() and
      not request.HasOption<QuotaUser>() and not request.HasOption<UserIp>()) {
    return ReadObjectXml(request);
  }
  // Assume the bucket name is validated by the caller.
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                                 "/o/" + UrlEscapeString(request.object_name()),
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "GET");
  if (not status.ok()) {
    return status;
  }
  builder.AddQueryParameter("alt", "media");
  if (request.HasOption<ReadRange>()) {
    auto range = request.GetOption<ReadRange>().value();
    std::string header = "Range: bytes=" + std::to_string(range.begin) + "-" +
                         std::to_string(range.end - 1);
    builder.AddHeader(header);
    // When doing a range read we need to disable decompression because range
    // reads do not work in that case:
    //   https://cloud.google.com/storage/docs/transcoding#range
    // and
    //   https://cloud.google.com/storage/docs/transcoding#decompressive_transcoding
    builder.AddHeader("Cache-Control: no-transform");
  }

  std::unique_ptr<CurlReadStreambuf> buf(new CurlReadStreambuf(
      builder.BuildDownloadRequest(std::string{}),
      client_options().download_buffer_size(), CreateHashValidator(request)));
  return std::unique_ptr<ObjectReadStreambuf>(std::move(buf));
}

StatusOr<std::unique_ptr<ObjectWriteStreambuf>> CurlClient::WriteObject(
    InsertObjectStreamingRequest const& request) {
  if (not request.HasOption<IfMetagenerationNotMatch>() and
      not request.HasOption<IfGenerationNotMatch>() and
      not request.HasOption<QuotaUser>() and not request.HasOption<UserIp>() and
      not request.HasOption<Projection>() and request.HasOption<Fields>() and
      request.GetOption<Fields>().value().empty()) {
    return WriteObjectXml(request);
  }

  if (request.HasOption<WithObjectMetadata>() or
      request.HasOption<UseResumableUploadSession>()) {
    return WriteObjectResumable(request);
  }

  return WriteObjectSimple(request);
}

StatusOr<ListObjectsResponse> CurlClient::ListObjects(
    ListObjectsRequest const& request) {
  // Assume the bucket name is validated by the caller.
  CurlRequestBuilder builder(
      storage_endpoint_ + "/b/" + request.bucket_name() + "/o",
      storage_factory_);
  auto status = SetupBuilder(builder, request, "GET");
  if (not status.ok()) {
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
  if (not status.ok()) {
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
  if (not status.ok()) {
    return status;
  }
  builder.AddHeader("Content-Type: application/json");
  return ParseFromString<ObjectMetadata>(
      builder.BuildRequest().MakeRequest(request.json_payload()));
}

StatusOr<ObjectMetadata> CurlClient::PatchObject(
    PatchObjectRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                                 "/o/" + UrlEscapeString(request.object_name()),
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "PATCH");
  if (not status.ok()) {
    return status;
  }
  builder.AddHeader("Content-Type: application/json");
  return ParseFromString<ObjectMetadata>(
      builder.BuildRequest().MakeRequest(request.payload()));
}

StatusOr<ObjectMetadata> CurlClient::ComposeObject(
    ComposeObjectRequest const& request) {
  CurlRequestBuilder builder(
      storage_endpoint_ + "/b/" + request.bucket_name() + "/o/" +
          UrlEscapeString(request.object_name()) + "/compose",
      storage_factory_);
  auto status = SetupBuilder(builder, request, "POST");
  if (not status.ok()) {
    return status;
  }
  builder.AddHeader("Content-Type: application/json");
  return ParseFromString<ObjectMetadata>(
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
  if (not status.ok()) {
    return status;
  }
  if (not request.rewrite_token().empty()) {
    builder.AddQueryParameter("rewriteToken", request.rewrite_token());
  }
  builder.AddHeader("Content-Type: application/json");
  std::string json_payload("{}");
  if (request.HasOption<WithObjectMetadata>()) {
    json_payload =
        request.GetOption<WithObjectMetadata>().value().JsonPayloadForCopy();
  }
  auto response = builder.BuildRequest().MakeRequest(json_payload);
  if (not response.ok()) {
    return std::move(response).status();
  }
  if (response->status_code >= 300) {
    return AsStatus(*response);
  }
  // This one does not use the common "ParseFromHttpResponse" function because
  // it takes different arguments.
  return RewriteObjectResponse::FromHttpResponse(*response);
}

StatusOr<std::unique_ptr<ResumableUploadSession>>
CurlClient::CreateResumableSession(ResumableUploadRequest const& request) {
  return CreateResumableSessionGeneric(request);
}

StatusOr<std::unique_ptr<ResumableUploadSession>>
CurlClient::RestoreResumableSession(std::string const& session_id) {
  auto session =
      google::cloud::internal::make_unique<CurlResumableUploadSession>(
          shared_from_this(), session_id);
  auto response = session->ResetSession();
  if (response.status().ok()) {
    return std::unique_ptr<ResumableUploadSession>(std::move(session));
  }
  return std::move(response).status();
}

StatusOr<ListBucketAclResponse> CurlClient::ListBucketAcl(
    ListBucketAclRequest const& request) {
  CurlRequestBuilder builder(
      storage_endpoint_ + "/b/" + request.bucket_name() + "/acl",
      storage_factory_);
  auto status = SetupBuilder(builder, request, "GET");
  if (not status.ok()) {
    return status;
  }
  auto response = builder.BuildRequest().MakeRequest(std::string{});
  if (not response.ok()) {
    return std::move(response).status();
  }
  if (response->status_code >= 300) {
    return AsStatus(*response);
  }
  return internal::ListBucketAclResponse::FromHttpResponse(
      *std::move(response));
}

StatusOr<BucketAccessControl> CurlClient::GetBucketAcl(
    GetBucketAclRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                                 "/acl/" + UrlEscapeString(request.entity()),
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "GET");
  if (not status.ok()) {
    return status;
  }
  return ParseFromString<BucketAccessControl>(
      builder.BuildRequest().MakeRequest(std::string{}));
}

StatusOr<BucketAccessControl> CurlClient::CreateBucketAcl(
    CreateBucketAclRequest const& request) {
  CurlRequestBuilder builder(
      storage_endpoint_ + "/b/" + request.bucket_name() + "/acl",
      storage_factory_);
  auto status = SetupBuilder(builder, request, "POST");
  if (not status.ok()) {
    return status;
  }
  builder.AddHeader("Content-Type: application/json");
  nl::json object;
  object["entity"] = request.entity();
  object["role"] = request.role();
  return ParseFromString<BucketAccessControl>(
      builder.BuildRequest().MakeRequest(object.dump()));
}

StatusOr<EmptyResponse> CurlClient::DeleteBucketAcl(
    DeleteBucketAclRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                                 "/acl/" + UrlEscapeString(request.entity()),
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "DELETE");
  if (not status.ok()) {
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
  if (not status.ok()) {
    return status;
  }
  builder.AddHeader("Content-Type: application/json");
  nl::json patch;
  patch["entity"] = request.entity();
  patch["role"] = request.role();
  return ParseFromString<BucketAccessControl>(
      builder.BuildRequest().MakeRequest(patch.dump()));
}

StatusOr<BucketAccessControl> CurlClient::PatchBucketAcl(
    PatchBucketAclRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                                 "/acl/" + UrlEscapeString(request.entity()),
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "PATCH");
  if (not status.ok()) {
    return status;
  }
  builder.AddHeader("Content-Type: application/json");
  return ParseFromString<BucketAccessControl>(
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
  if (not status.ok()) {
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
  if (not status.ok()) {
    return status;
  }
  builder.AddHeader("Content-Type: application/json");
  nl::json object;
  object["entity"] = request.entity();
  object["role"] = request.role();
  return ParseFromString<ObjectAccessControl>(
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
  if (not status.ok()) {
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
  if (not status.ok()) {
    return status;
  }
  return ParseFromString<ObjectAccessControl>(
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
  if (not status.ok()) {
    return status;
  }
  builder.AddHeader("Content-Type: application/json");
  nl::json object;
  object["entity"] = request.entity();
  object["role"] = request.role();
  return ParseFromString<ObjectAccessControl>(
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
  if (not status.ok()) {
    return status;
  }
  builder.AddHeader("Content-Type: application/json");
  return ParseFromString<ObjectAccessControl>(
      builder.BuildRequest().MakeRequest(request.payload()));
}

StatusOr<ListDefaultObjectAclResponse> CurlClient::ListDefaultObjectAcl(
    ListDefaultObjectAclRequest const& request) {
  // Assume the bucket name is validated by the caller.
  CurlRequestBuilder builder(
      storage_endpoint_ + "/b/" + request.bucket_name() + "/defaultObjectAcl",
      storage_factory_);
  auto status = SetupBuilder(builder, request, "GET");
  if (not status.ok()) {
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
  if (not status.ok()) {
    return status;
  }
  nl::json object;
  object["entity"] = request.entity();
  object["role"] = request.role();
  builder.AddHeader("Content-Type: application/json");
  return ParseFromString<ObjectAccessControl>(
      builder.BuildRequest().MakeRequest(object.dump()));
}

StatusOr<EmptyResponse> CurlClient::DeleteDefaultObjectAcl(
    DeleteDefaultObjectAclRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                                 "/defaultObjectAcl/" +
                                 UrlEscapeString(request.entity()),
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "DELETE");
  if (not status.ok()) {
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
  if (not status.ok()) {
    return status;
  }
  return ParseFromString<ObjectAccessControl>(
      builder.BuildRequest().MakeRequest(std::string{}));
}

StatusOr<ObjectAccessControl> CurlClient::UpdateDefaultObjectAcl(
    UpdateDefaultObjectAclRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                                 "/defaultObjectAcl/" +
                                 UrlEscapeString(request.entity()),
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "PUT");
  if (not status.ok()) {
    return status;
  }
  builder.AddHeader("Content-Type: application/json");
  nl::json object;
  object["entity"] = request.entity();
  object["role"] = request.role();
  return ParseFromString<ObjectAccessControl>(
      builder.BuildRequest().MakeRequest(object.dump()));
}

StatusOr<ObjectAccessControl> CurlClient::PatchDefaultObjectAcl(
    PatchDefaultObjectAclRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                                 "/defaultObjectAcl/" +
                                 UrlEscapeString(request.entity()),
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "PATCH");
  if (not status.ok()) {
    return status;
  }
  builder.AddHeader("Content-Type: application/json");
  return ParseFromString<ObjectAccessControl>(
      builder.BuildRequest().MakeRequest(request.payload()));
}

StatusOr<ServiceAccount> CurlClient::GetServiceAccount(
    GetProjectServiceAccountRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/projects/" +
                                 request.project_id() + "/serviceAccount",
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "GET");
  if (not status.ok()) {
    return status;
  }
  return ParseFromString<ServiceAccount>(
      builder.BuildRequest().MakeRequest(std::string{}));
}

StatusOr<ListNotificationsResponse> CurlClient::ListNotifications(
    ListNotificationsRequest const& request) {
  // Assume the bucket name is validated by the caller.
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                                 "/notificationConfigs",
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "GET");
  if (not status.ok()) {
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
  if (not status.ok()) {
    return status;
  }
  builder.AddHeader("Content-Type: application/json");
  return ParseFromString<NotificationMetadata>(
      builder.BuildRequest().MakeRequest(request.json_payload()));
}

StatusOr<NotificationMetadata> CurlClient::GetNotification(
    GetNotificationRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                                 "/notificationConfigs/" +
                                 request.notification_id(),
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "GET");
  if (not status.ok()) {
    return status;
  }
  return ParseFromString<NotificationMetadata>(
      builder.BuildRequest().MakeRequest(std::string{}));
}

StatusOr<EmptyResponse> CurlClient::DeleteNotification(
    DeleteNotificationRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                                 "/notificationConfigs/" +
                                 request.notification_id(),
                             storage_factory_);
  auto status = SetupBuilder(builder, request, "DELETE");
  if (not status.ok()) {
    return status;
  }
  return ReturnEmptyResponse(builder.BuildRequest().MakeRequest(std::string{}));
}

void CurlClient::LockShared() { mu_.lock(); }

void CurlClient::UnlockShared() { mu_.unlock(); }

StatusOr<ObjectMetadata> CurlClient::InsertObjectMediaXml(
    InsertObjectMediaRequest const& request) {
  CurlRequestBuilder builder(xml_upload_endpoint_ + "/" +
                                 request.bucket_name() + "/" +
                                 UrlEscapeString(request.object_name()),
                             xml_upload_factory_);
  auto status = SetupBuilderCommon(builder, "PUT");
  if (not status.ok()) {
    return status;
  }
  builder.AddHeader("Host: storage.googleapis.com");

  //
  // Apply the options from InsertObjectMediaRequest that are set, translating
  // to the XML format for them.
  //
  builder.AddOption(request.GetOption<ContentEncoding>());
  // Set the content type of a sensible value, the application can override this
  // in the options for the request.
  if (not request.HasOption<ContentType>()) {
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
        "x-goog-if-meta-generation-match: " +
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
  } else if (not request.HasOption<DisableMD5Hash>()) {
    builder.AddHeader("x-goog-hash: md5=" + ComputeMD5Hash(request.contents()));
  }
  if (request.HasOption<Crc32cChecksumValue>()) {
    builder.AddHeader("x-goog-hash: crc32c=" +
                      request.GetOption<Crc32cChecksumValue>().value());
  } else if (not request.HasOption<DisableCrc32cChecksum>()) {
    builder.AddHeader("x-goog-hash: crc32c=" +
                      ComputeCrc32cChecksum(request.contents()));
  }
  if (request.HasOption<PredefinedAcl>()) {
    builder.AddHeader(
        "x-goog-acl: " +
        XmlMapPredefinedAcl(request.GetOption<PredefinedAcl>().value()));
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
  if (not response.ok()) {
    return std::move(response).status();
  }
  if (response->status_code >= 300) {
    return AsStatus(*response);
  }
  return ObjectMetadata::ParseFromJson(internal::nl::json{
      {"name", request.object_name()},
      {"bucket", request.bucket_name()},
  });
}

StatusOr<std::unique_ptr<ObjectReadStreambuf>> CurlClient::ReadObjectXml(
    ReadObjectRangeRequest const& request) {
  CurlRequestBuilder builder(xml_download_endpoint_ + "/" +
                                 request.bucket_name() + "/" +
                                 UrlEscapeString(request.object_name()),
                             xml_download_factory_);
  auto status = SetupBuilderCommon(builder, "GET");
  if (not status.ok()) {
    return status;
  }
  builder.AddHeader("Host: storage.googleapis.com");

  //
  // Apply the options from ReadObjectMediaRequest that are set, translating
  // to the XML format for them.
  //
  builder.AddOption(request.GetOption<EncryptionKey>());
  builder.AddOption(request.GetOption<Generation>());
  if (request.HasOption<IfGenerationMatch>()) {
    builder.AddHeader(
        "x-goog-if-generation-match: " +
        std::to_string(request.GetOption<IfGenerationMatch>().value()));
  }
  // IfGenerationNotMatch cannot be set, checked by the caller.
  if (request.HasOption<IfMetagenerationMatch>()) {
    builder.AddHeader(
        "x-goog-if-meta-generation-match: " +
        std::to_string(request.GetOption<IfMetagenerationMatch>().value()));
  }
  // IfMetagenerationNotMatch cannot be set, checked by the caller.
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

  if (request.HasOption<ReadRange>()) {
    auto range = request.GetOption<ReadRange>().value();
    std::string header = "Range: bytes=" + std::to_string(range.begin) + "-" +
                         std::to_string(range.end - 1);
    builder.AddHeader(header);
    // When doing a range read we need to disable decompression because range
    // reads do not work in that case:
    //   https://cloud.google.com/storage/docs/transcoding#range
    // and
    //   https://cloud.google.com/storage/docs/transcoding#decompressive_transcoding
    builder.AddHeader("Cache-Control: no-transform");
  }

  std::unique_ptr<CurlReadStreambuf> buf(new CurlReadStreambuf(
      builder.BuildDownloadRequest(std::string{}),
      client_options().download_buffer_size(), CreateHashValidator(request)));
  return std::unique_ptr<ObjectReadStreambuf>(std::move(buf));
}

StatusOr<std::unique_ptr<ObjectWriteStreambuf>> CurlClient::WriteObjectXml(
    InsertObjectStreamingRequest const& request) {
  CurlRequestBuilder builder(xml_upload_endpoint_ + "/" +
                                 request.bucket_name() + "/" +
                                 UrlEscapeString(request.object_name()),
                             xml_upload_factory_);
  auto status = SetupBuilderCommon(builder, "PUT");
  if (not status.ok()) {
    return status;
  }
  builder.AddHeader("Host: storage.googleapis.com");

  //
  // Apply the options from InsertObjectMediaRequest that are set, translating
  // to the XML format for them.
  //
  builder.AddOption(request.GetOption<ContentEncoding>());
  // Set the content type of a sensible value, the application can override this
  // in the options for the request.
  if (not request.HasOption<ContentType>()) {
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
        "x-goog-if-meta-generation-match: " +
        std::to_string(request.GetOption<IfMetagenerationMatch>().value()));
  }
  // IfMetagenerationNotMatch cannot be set, checked by the caller.
  if (request.HasOption<KmsKeyName>()) {
    builder.AddHeader("x-goog-encryption-kms-key-name: " +
                      request.GetOption<KmsKeyName>().value());
  }
  if (request.HasOption<PredefinedAcl>()) {
    builder.AddHeader(
        "x-goog-acl: " +
        XmlMapPredefinedAcl(request.GetOption<PredefinedAcl>().value()));
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

  std::unique_ptr<internal::CurlWriteStreambuf> buf(
      new internal::CurlWriteStreambuf(builder.BuildUpload(),
                                       client_options().upload_buffer_size(),
                                       CreateHashValidator(request)));
  return std::unique_ptr<internal::ObjectWriteStreambuf>(std::move(buf));
}

StatusOr<ObjectMetadata> CurlClient::InsertObjectMediaMultipart(
    InsertObjectMediaRequest const& request) {
  // To perform a multipart upload we need to separate the parts using:
  //   https://cloud.google.com/storage/docs/json_api/v1/how-tos/multipart-upload
  // This function is structured as follows:
  // 1. Create a request object, as we often do.
  CurlRequestBuilder builder(
      upload_endpoint_ + "/b/" + request.bucket_name() + "/o", upload_factory_);
  auto status = SetupBuilder(builder, request, "POST");
  if (not status.ok()) {
    return status;
  }

  // 2. Pick a separator that does not conflict with the request contents.
  auto boundary = PickBoundary(request.contents());
  builder.AddHeader("content-type: multipart/related; boundary=" + boundary);
  builder.AddQueryParameter("uploadType", "multipart");
  builder.AddQueryParameter("name", request.object_name());

  // 3. Perform a streaming upload because computing the size upfront is more
  //    complicated than it is worth.
  std::unique_ptr<internal::CurlWriteStreambuf> buf(
      new internal::CurlWriteStreambuf(builder.BuildUpload(),
                                       client_options().upload_buffer_size(),
                                       CreateHashValidator(request)));
  ObjectWriteStream writer(std::move(buf));

  nl::json metadata = nl::json::object();
  if (request.HasOption<WithObjectMetadata>()) {
    metadata = request.GetOption<WithObjectMetadata>().value().JsonForUpdate();
  }
  if (request.HasOption<MD5HashValue>()) {
    metadata["md5Hash"] = request.GetOption<MD5HashValue>().value();
  } else {
    metadata["md5Hash"] = ComputeMD5Hash(request.contents());
  }

  if (request.HasOption<Crc32cChecksumValue>()) {
    metadata["crc32c"] = request.GetOption<Crc32cChecksumValue>().value();
  } else {
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
  writer.Close();
  return std::move(writer).metadata();
}

std::string CurlClient::PickBoundary(std::string const& text_to_avoid) {
  // We need to find a string that is *not* found in `text_to_avoid`, we pick
  // a string at random, and see if it is in `text_to_avoid`, if it is, we grow
  // the string with random characters and start from where we last found a
  // the candidate.  Eventually we will find something, though it might be
  // larger than `text_to_avoid`.  And we only make (approximately) one pass
  // over `text_to_avoid`.
  auto generate_candidate = [this](int n) {
    static std::string const chars =
        "abcdefghijklmnopqrstuvwxyz012456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::unique_lock<std::mutex> lk(mu_);
    return google::cloud::internal::Sample(generator_, n, chars);
  };
  constexpr int INITIAL_CANDIDATE_SIZE = 16;
  constexpr int CANDIDATE_GROWTH_SIZE = 4;
  return GenerateMessageBoundary(text_to_avoid, std::move(generate_candidate),
                                 INITIAL_CANDIDATE_SIZE, CANDIDATE_GROWTH_SIZE);
}

StatusOr<ObjectMetadata> CurlClient::InsertObjectMediaSimple(
    InsertObjectMediaRequest const& request) {
  CurlRequestBuilder builder(
      upload_endpoint_ + "/b/" + request.bucket_name() + "/o", upload_factory_);
  auto status = SetupBuilder(builder, request, "POST");
  if (not status.ok()) {
    return status;
  }
  // Set the content type of a sensible value, the application can override this
  // in the options for the request.
  if (not request.HasOption<ContentType>()) {
    builder.AddHeader("content-type: application/octet-stream");
  }
  builder.AddQueryParameter("uploadType", "media");
  builder.AddQueryParameter("name", request.object_name());
  builder.AddHeader("Content-Length: " +
                    std::to_string(request.contents().size()));
  return ParseFromString<ObjectMetadata>(
      builder.BuildRequest().MakeRequest(request.contents()));
}

StatusOr<std::unique_ptr<ObjectWriteStreambuf>> CurlClient::WriteObjectSimple(
    InsertObjectStreamingRequest const& request) {
  auto url = upload_endpoint_ + "/b/" + request.bucket_name() + "/o";
  CurlRequestBuilder builder(url, upload_factory_);
  auto status = SetupBuilder(builder, request, "POST");
  if (not status.ok()) {
    return status;
  }

  // Set the content type of a sensible value, the application can override this
  // in the options for the request.
  if (not request.HasOption<ContentType>()) {
    builder.AddHeader("content-type: application/octet-stream");
  }
  builder.AddQueryParameter("uploadType", "media");
  builder.AddQueryParameter("name", request.object_name());
  std::unique_ptr<internal::CurlWriteStreambuf> buf(
      new internal::CurlWriteStreambuf(builder.BuildUpload(),
                                       client_options().upload_buffer_size(),
                                       CreateHashValidator(request)));
  return std::unique_ptr<internal::ObjectWriteStreambuf>(std::move(buf));
}

StatusOr<std::unique_ptr<ObjectWriteStreambuf>>
CurlClient::WriteObjectResumable(InsertObjectStreamingRequest const& request) {
  auto session = CreateResumableSessionGeneric(request);
  if (not session.ok()) {
    return std::move(session).status();
  }

  auto buf =
      google::cloud::internal::make_unique<internal::CurlResumableStreambuf>(
          std::move(session).value(), client_options().upload_buffer_size(),
          CreateHashValidator(request));
  return std::unique_ptr<internal::ObjectWriteStreambuf>(std::move(buf));
}

StatusOr<std::string> CurlClient::AuthorizationHeader(
    std::shared_ptr<google::cloud::storage::oauth2::Credentials> const&
        credentials) {
  return credentials->AuthorizationHeader();
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
