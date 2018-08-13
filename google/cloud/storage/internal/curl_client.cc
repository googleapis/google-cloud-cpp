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
#include "google/cloud/storage/internal/curl_request_builder.h"
#include "google/cloud/storage/internal/curl_streambuf.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
std::pair<Status, ListBucketsResponse> CurlClient::ListBuckets(
    ListBucketsRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b");
  builder.SetDebugLogging(options_.enable_http_tracing());
  builder.AddHeader(options_.credentials()->AuthorizationHeader());
  builder.AddQueryParameter("project", request.project_id());
  request.AddOptionsToHttpRequest(builder);
  auto payload = builder.BuildRequest(std::string{}).MakeRequest();
  if (payload.status_code >= 300) {
    return std::make_pair(
        Status{payload.status_code, std::move(payload.payload)},
        ListBucketsResponse{});
  }
  return std::make_pair(
      Status(), ListBucketsResponse::FromHttpResponse(std::move(payload)));
}

std::pair<Status, BucketMetadata> CurlClient::CreateBucket(
    CreateBucketRequest const& request) {
  // Assume the bucket name is validated by the caller.
  CurlRequestBuilder builder(storage_endpoint_ + "/b");
  builder.AddQueryParameter("project", request.project_id());
  builder.SetDebugLogging(options_.enable_http_tracing());
  builder.AddHeader(options_.credentials()->AuthorizationHeader());
  request.AddOptionsToHttpRequest(builder);
  builder.AddHeader("Content-Type: application/json");
  auto payload = builder.BuildRequest(request.json_payload()).MakeRequest();
  if (payload.status_code >= 300) {
    return std::make_pair(
        Status{payload.status_code, std::move(payload.payload)},
        BucketMetadata{});
  }
  return std::make_pair(Status(),
                        BucketMetadata::ParseFromString(payload.payload));
}

std::pair<Status, BucketMetadata> CurlClient::GetBucketMetadata(
    GetBucketMetadataRequest const& request) {
  // Assume the bucket name is validated by the caller.
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name());
  builder.SetDebugLogging(options_.enable_http_tracing());
  builder.AddHeader(options_.credentials()->AuthorizationHeader());
  request.AddOptionsToHttpRequest(builder);
  auto payload = builder.BuildRequest(std::string{}).MakeRequest();
  if (200 != payload.status_code) {
    return std::make_pair(
        Status{payload.status_code, std::move(payload.payload)},
        BucketMetadata{});
  }
  return std::make_pair(Status(),
                        BucketMetadata::ParseFromString(payload.payload));
}

std::pair<Status, EmptyResponse> CurlClient::DeleteBucket(
    DeleteBucketRequest const& request) {
  // Assume the bucket name is validated by the caller.
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name());
  builder.SetDebugLogging(options_.enable_http_tracing());
  builder.AddHeader(options_.credentials()->AuthorizationHeader());
  request.AddOptionsToHttpRequest(builder);
  builder.SetMethod("DELETE");
  auto payload = builder.BuildRequest(std::string{}).MakeRequest();
  if (payload.status_code >= 300) {
    return std::make_pair(
        Status{payload.status_code, std::move(payload.payload)},
        internal::EmptyResponse{});
  }
  return std::make_pair(Status(), internal::EmptyResponse{});
}

std::pair<Status, BucketMetadata> CurlClient::UpdateBucket(
    UpdateBucketRequest const& request) {
  // Assume the bucket name is validated by the caller.
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" +
                             request.metadata().name());
  builder.SetDebugLogging(options_.enable_http_tracing());
  builder.AddHeader(options_.credentials()->AuthorizationHeader());
  request.AddOptionsToHttpRequest(builder);
  builder.AddHeader("Content-Type: application/json");
  builder.SetMethod("PUT");
  auto payload = builder.BuildRequest(request.json_payload()).MakeRequest();
  if (payload.status_code >= 300) {
    return std::make_pair(
        Status{payload.status_code, std::move(payload.payload)},
        BucketMetadata{});
  }
  return std::make_pair(Status(),
                        BucketMetadata::ParseFromString(payload.payload));
}

std::pair<Status, ObjectMetadata> CurlClient::InsertObjectMedia(
    InsertObjectMediaRequest const& request) {
  // Assume the bucket name is validated by the caller.
  CurlRequestBuilder builder(upload_endpoint_ + "/b/" + request.bucket_name() +
                             "/o");
  builder.SetDebugLogging(options_.enable_http_tracing());
  builder.AddHeader(options_.credentials()->AuthorizationHeader());
  request.AddOptionsToHttpRequest(builder);
  builder.AddQueryParameter("uploadType", "media");
  builder.AddQueryParameter("name", request.object_name());
  builder.AddHeader("Content-Type: application/octet-stream");
  builder.AddHeader("Content-Length: " +
                    std::to_string(request.contents().size()));
  auto payload = builder.BuildRequest(request.contents()).MakeRequest();
  if (200 != payload.status_code) {
    return std::make_pair(
        Status{payload.status_code, std::move(payload.payload)},
        ObjectMetadata{});
  }
  return std::make_pair(Status(),
                        ObjectMetadata::ParseFromString(payload.payload));
}

std::pair<Status, ObjectMetadata> CurlClient::GetObjectMetadata(
    GetObjectMetadataRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                             "/o/" + request.object_name());
  builder.SetDebugLogging(options_.enable_http_tracing());
  builder.AddHeader(options_.credentials()->AuthorizationHeader());
  request.AddOptionsToHttpRequest(builder);
  auto payload = builder.BuildRequest(std::string{}).MakeRequest();
  if (payload.status_code >= 300) {
    return std::make_pair(
        Status{payload.status_code, std::move(payload.payload)},
        ObjectMetadata{});
  }
  return std::make_pair(Status(),
                        ObjectMetadata::ParseFromString(payload.payload));
}

std::pair<Status, std::unique_ptr<ObjectReadStreambuf>> CurlClient::ReadObject(
    ReadObjectRangeRequest const& request) {
  // Assume the bucket name is validated by the caller.
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                             "/o/" + request.object_name());
  builder.SetDebugLogging(options_.enable_http_tracing());
  builder.AddHeader(options_.credentials()->AuthorizationHeader());
  builder.AddQueryParameter("alt", "media");
  // TODO(#937) - use client options to configure buffer size.
  std::unique_ptr<CurlReadStreambuf> buf(new CurlReadStreambuf(
      builder.BuildDownloadRequest(std::string{}), 128 * 1024));
  return std::make_pair(Status(),
                        std::unique_ptr<ObjectReadStreambuf>(std::move(buf)));
}

std::pair<Status, std::unique_ptr<ObjectWriteStreambuf>>
CurlClient::WriteObject(InsertObjectStreamingRequest const& request) {
  auto url = upload_endpoint_ + "/b/" + request.bucket_name() + "/o";
  CurlRequestBuilder builder(url);
  builder.SetDebugLogging(options_.enable_http_tracing());
  builder.AddHeader(options_.credentials()->AuthorizationHeader());
  request.AddOptionsToHttpRequest(builder);
  builder.AddQueryParameter("uploadType", "media");
  builder.AddQueryParameter("name", request.object_name());
  builder.AddHeader("Content-Type: application/octet-stream");
  // TODO(#937) - use client options to configure buffer size.
  std::unique_ptr<internal::CurlStreambuf> buf(
      new internal::CurlStreambuf(builder.BuildUpload(), 128 * 1024));
  return std::make_pair(
      Status(),
      std::unique_ptr<internal::ObjectWriteStreambuf>(std::move(buf)));
}

std::pair<Status, ListObjectsResponse> CurlClient::ListObjects(
    ListObjectsRequest const& request) {
  // Assume the bucket name is validated by the caller.
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                             "/o");
  builder.SetDebugLogging(options_.enable_http_tracing());
  builder.AddHeader(options_.credentials()->AuthorizationHeader());
  request.AddOptionsToHttpRequest(builder);
  builder.AddQueryParameter("pageToken", request.page_token());
  auto payload = builder.BuildRequest(std::string{}).MakeRequest();
  if (200 != payload.status_code) {
    return std::make_pair(
        Status{payload.status_code, std::move(payload.payload)},
        internal::ListObjectsResponse{});
  }
  return std::make_pair(
      Status(),
      internal::ListObjectsResponse::FromHttpResponse(std::move(payload)));
}

std::pair<Status, EmptyResponse> CurlClient::DeleteObject(
    DeleteObjectRequest const& request) {
  // Assume the bucket name is validated by the caller.
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                             "/o/" + request.object_name());
  builder.SetDebugLogging(options_.enable_http_tracing());
  builder.AddHeader(options_.credentials()->AuthorizationHeader());
  request.AddOptionsToHttpRequest(builder);
  builder.SetMethod("DELETE");
  auto payload = builder.BuildRequest(std::string{}).MakeRequest();
  if (payload.status_code >= 300) {
    return std::make_pair(
        Status{payload.status_code, std::move(payload.payload)},
        internal::EmptyResponse{});
  }
  return std::make_pair(Status(), internal::EmptyResponse{});
}

std::pair<Status, ListBucketAclResponse> CurlClient::ListBucketAcl(
    ListBucketAclRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                             "/acl");
  builder.SetDebugLogging(options_.enable_http_tracing());
  builder.AddHeader(options_.credentials()->AuthorizationHeader());
  request.AddOptionsToHttpRequest(builder);
  auto payload = builder.BuildRequest(std::string{}).MakeRequest();
  if (payload.status_code >= 300) {
    return std::make_pair(
        Status{payload.status_code, std::move(payload.payload)},
        internal::ListBucketAclResponse{});
  }
  return std::make_pair(
      Status(),
      internal::ListBucketAclResponse::FromHttpResponse(std::move(payload)));
}

std::pair<Status, BucketAccessControl> CurlClient::GetBucketAcl(
    GetBucketAclRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                             "/acl/" + request.entity());
  builder.SetDebugLogging(options_.enable_http_tracing());
  builder.AddHeader(options_.credentials()->AuthorizationHeader());
  request.AddOptionsToHttpRequest(builder);
  auto payload = builder.BuildRequest(std::string{}).MakeRequest();
  if (payload.status_code >= 300) {
    return std::make_pair(
        Status{payload.status_code, std::move(payload.payload)},
        BucketAccessControl{});
  }
  return std::make_pair(Status(),
                        BucketAccessControl::ParseFromString(payload.payload));
}

std::pair<Status, BucketAccessControl> CurlClient::CreateBucketAcl(
    CreateBucketAclRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                             "/acl");
  builder.SetDebugLogging(options_.enable_http_tracing());
  builder.AddHeader(options_.credentials()->AuthorizationHeader());
  request.AddOptionsToHttpRequest(builder);
  nl::json object;
  object["entity"] = request.entity();
  object["role"] = request.role();
  builder.AddHeader("Content-Type: application/json");
  auto payload = builder.BuildRequest(object.dump()).MakeRequest();
  if (payload.status_code >= 300) {
    return std::make_pair(
        Status{payload.status_code, std::move(payload.payload)},
        BucketAccessControl{});
  }
  return std::make_pair(Status(),
                        BucketAccessControl::ParseFromString(payload.payload));
}

std::pair<Status, EmptyResponse> CurlClient::DeleteBucketAcl(
    DeleteBucketAclRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                             "/acl/" + request.entity());
  builder.SetDebugLogging(options_.enable_http_tracing());
  builder.AddHeader(options_.credentials()->AuthorizationHeader());
  request.AddOptionsToHttpRequest(builder);
  builder.SetMethod("DELETE");
  auto payload = builder.BuildRequest(std::string{}).MakeRequest();
  if (payload.status_code >= 300) {
    return std::make_pair(
        Status{payload.status_code, std::move(payload.payload)},
        internal::EmptyResponse{});
  }
  return std::make_pair(Status(), internal::EmptyResponse{});
}

std::pair<Status, BucketAccessControl> CurlClient::UpdateBucketAcl(
    UpdateBucketAclRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                             "/acl/" + request.entity());
  builder.SetDebugLogging(options_.enable_http_tracing());
  builder.AddHeader(options_.credentials()->AuthorizationHeader());
  request.AddOptionsToHttpRequest(builder);
  builder.SetMethod("PUT");
  nl::json patch;
  patch["entity"] = request.entity();
  patch["role"] = request.role();
  builder.AddHeader("Content-Type: application/json");
  auto payload = builder.BuildRequest(patch.dump()).MakeRequest();
  if (payload.status_code >= 300) {
    return std::make_pair(
        Status{payload.status_code, std::move(payload.payload)},
        BucketAccessControl{});
  }
  return std::make_pair(Status(),
                        BucketAccessControl::ParseFromString(payload.payload));
}

std::pair<Status, BucketAccessControl> CurlClient::PatchBucketAcl(
    PatchBucketAclRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                             "/acl/" + request.entity());
  builder.SetDebugLogging(options_.enable_http_tracing());
  builder.AddHeader(options_.credentials()->AuthorizationHeader());
  request.AddOptionsToHttpRequest(builder);
  builder.SetMethod("PATCH");
  builder.AddHeader("Content-Type: application/json");
  auto payload = builder.BuildRequest(request.payload()).MakeRequest();
  if (payload.status_code >= 300) {
    return std::make_pair(
        Status{payload.status_code, std::move(payload.payload)},
        BucketAccessControl{});
  }
  return std::make_pair(Status(),
                        BucketAccessControl::ParseFromString(payload.payload));
}

std::pair<Status, ListObjectAclResponse> CurlClient::ListObjectAcl(
    ListObjectAclRequest const& request) {
  // Assume the bucket name is validated by the caller.
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                             "/o/" + request.object_name() + "/acl");
  builder.SetDebugLogging(options_.enable_http_tracing());
  builder.AddHeader(options_.credentials()->AuthorizationHeader());
  request.AddOptionsToHttpRequest(builder);
  auto payload = builder.BuildRequest(std::string{}).MakeRequest();
  if (payload.status_code >= 300) {
    return std::make_pair(
        Status{payload.status_code, std::move(payload.payload)},
        internal::ListObjectAclResponse{});
  }
  return std::make_pair(
      Status(),
      internal::ListObjectAclResponse::FromHttpResponse(std::move(payload)));
}

std::pair<Status, ObjectAccessControl> CurlClient::CreateObjectAcl(
    CreateObjectAclRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                             "/o/" + request.object_name() + "/acl");
  builder.SetDebugLogging(options_.enable_http_tracing());
  builder.AddHeader(options_.credentials()->AuthorizationHeader());
  request.AddOptionsToHttpRequest(builder);
  nl::json object;
  object["entity"] = request.entity();
  object["role"] = request.role();
  builder.AddHeader("Content-Type: application/json");
  auto payload = builder.BuildRequest(object.dump()).MakeRequest();
  if (payload.status_code >= 300) {
    return std::make_pair(
        Status{payload.status_code, std::move(payload.payload)},
        ObjectAccessControl{});
  }
  return std::make_pair(Status(),
                        ObjectAccessControl::ParseFromString(payload.payload));
}

std::pair<Status, EmptyResponse> CurlClient::DeleteObjectAcl(
    DeleteObjectAclRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                             "/o/" + request.object_name() + "/acl/" +
                             request.entity());
  builder.SetDebugLogging(options_.enable_http_tracing());
  builder.AddHeader(options_.credentials()->AuthorizationHeader());
  request.AddOptionsToHttpRequest(builder);
  builder.SetMethod("DELETE");
  auto payload = builder.BuildRequest(std::string{}).MakeRequest();
  if (payload.status_code >= 300) {
    return std::make_pair(
        Status{payload.status_code, std::move(payload.payload)},
        internal::EmptyResponse{});
  }
  return std::make_pair(Status(), internal::EmptyResponse{});
}

std::pair<Status, ObjectAccessControl> CurlClient::GetObjectAcl(
    GetObjectAclRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                             "/o/" + request.object_name() + "/acl/" +
                             request.entity());
  builder.SetDebugLogging(options_.enable_http_tracing());
  builder.AddHeader(options_.credentials()->AuthorizationHeader());
  request.AddOptionsToHttpRequest(builder);
  auto payload = builder.BuildRequest(std::string{}).MakeRequest();
  if (payload.status_code >= 300) {
    return std::make_pair(
        Status{payload.status_code, std::move(payload.payload)},
        ObjectAccessControl{});
  }
  return std::make_pair(Status(),
                        ObjectAccessControl::ParseFromString(payload.payload));
}

std::pair<Status, ObjectAccessControl> CurlClient::UpdateObjectAcl(
    UpdateObjectAclRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                             "/o/" + request.object_name() + "/acl/" +
                             request.entity());
  builder.SetDebugLogging(options_.enable_http_tracing());
  builder.AddHeader(options_.credentials()->AuthorizationHeader());
  request.AddOptionsToHttpRequest(builder);
  builder.SetMethod("PUT");
  nl::json object;
  object["entity"] = request.entity();
  object["role"] = request.role();
  builder.AddHeader("Content-Type: application/json");
  auto payload = builder.BuildRequest(object.dump()).MakeRequest();
  if (payload.status_code >= 300) {
    return std::make_pair(
        Status{payload.status_code, std::move(payload.payload)},
        ObjectAccessControl{});
  }
  return std::make_pair(Status(),
                        ObjectAccessControl::ParseFromString(payload.payload));
}

std::pair<Status, ObjectAccessControl> CurlClient::PatchObjectAcl(
    PatchObjectAclRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                             "/o/" + request.object_name() + "/acl/" +
                             request.entity());
  builder.SetDebugLogging(options_.enable_http_tracing());
  builder.AddHeader(options_.credentials()->AuthorizationHeader());
  request.AddOptionsToHttpRequest(builder);
  builder.SetMethod("PATCH");
  builder.AddHeader("Content-Type: application/json");
  auto payload = builder.BuildRequest(request.payload()).MakeRequest();
  if (payload.status_code >= 300) {
    return std::make_pair(
        Status{payload.status_code, std::move(payload.payload)},
        ObjectAccessControl{});
  }
  return std::make_pair(Status(),
                        ObjectAccessControl::ParseFromString(payload.payload));
}

std::pair<Status, ListDefaultObjectAclResponse>
CurlClient::ListDefaultObjectAcl(ListDefaultObjectAclRequest const& request) {
  // Assume the bucket name is validated by the caller.
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                             "/defaultObjectAcl");
  builder.SetDebugLogging(options_.enable_http_tracing());
  builder.AddHeader(options_.credentials()->AuthorizationHeader());
  request.AddOptionsToHttpRequest(builder);
  auto payload = builder.BuildRequest(std::string{}).MakeRequest();
  if (payload.status_code >= 300) {
    return std::make_pair(
        Status{payload.status_code, std::move(payload.payload)},
        internal::ListDefaultObjectAclResponse{});
  }
  return std::make_pair(
      Status(), internal::ListDefaultObjectAclResponse::FromHttpResponse(
                    std::move(payload)));
}

std::pair<Status, ObjectAccessControl> CurlClient::CreateDefaultObjectAcl(
    CreateDefaultObjectAclRequest const& request) {
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                             "/defaultObjectAcl");
  builder.SetDebugLogging(options_.enable_http_tracing());
  builder.AddHeader(options_.credentials()->AuthorizationHeader());
  request.AddOptionsToHttpRequest(builder);
  nl::json object;
  object["entity"] = request.entity();
  object["role"] = request.role();
  builder.AddHeader("Content-Type: application/json");
  auto payload = builder.BuildRequest(object.dump()).MakeRequest();
  if (payload.status_code >= 300) {
    return std::make_pair(
        Status{payload.status_code, std::move(payload.payload)},
        ObjectAccessControl{});
  }
  return std::make_pair(Status(),
                        ObjectAccessControl::ParseFromString(payload.payload));
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
