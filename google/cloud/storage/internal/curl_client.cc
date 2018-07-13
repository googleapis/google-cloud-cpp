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

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
std::pair<Status, BucketMetadata> CurlClient::GetBucketMetadata(
    GetBucketMetadataRequest const& request) {
  // Assume the bucket name is validated by the caller.
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name());
  builder.SetDebugLogging(options_.enable_http_tracing());
  builder.AddHeader(options_.credentials()->AuthorizationHeader());
  request.AddParametersToHttpRequest(builder);
  auto payload = builder.BuildRequest(std::string{}).MakeRequest();
  if (200 != payload.status_code) {
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
  request.AddParametersToHttpRequest(builder);
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
  request.AddParametersToHttpRequest(builder);
  auto payload = builder.BuildRequest(std::string{}).MakeRequest();
  if (payload.status_code >= 300) {
    return std::make_pair(
        Status{payload.status_code, std::move(payload.payload)},
        ObjectMetadata{});
  }
  return std::make_pair(Status(),
                        ObjectMetadata::ParseFromString(payload.payload));
}

std::pair<Status, ReadObjectRangeResponse> CurlClient::ReadObjectRangeMedia(
    ReadObjectRangeRequest const& request) {
  // Assume the bucket name is validated by the caller.
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                             "/o/" + request.object_name());
  builder.SetDebugLogging(options_.enable_http_tracing());
  builder.AddHeader(options_.credentials()->AuthorizationHeader());
  builder.AddQueryParameter("alt", "media");
  // For the moment, we are using range reads to read the objects (see #727)
  // disable decompression because range reads do not work in that case:
  //   https://cloud.google.com/storage/docs/transcoding#range
  // and
  //   https://cloud.google.com/storage/docs/transcoding#decompressive_transcoding
  builder.AddHeader("Cache-Control: no-transform");
  builder.AddHeader("Range: bytes=" + std::to_string(request.begin()) + '-' +
                    std::to_string(request.end()));
  auto payload = builder.BuildRequest(std::string{}).MakeRequest();
  if (payload.status_code >= 300) {
    return std::make_pair(
        Status{payload.status_code, std::move(payload.payload)},
        internal::ReadObjectRangeResponse{});
  }
  return std::make_pair(
      Status(),
      internal::ReadObjectRangeResponse::FromHttpResponse(std::move(payload)));
}

std::pair<Status, ListObjectsResponse> CurlClient::ListObjects(
    ListObjectsRequest const& request) {
  // Assume the bucket name is validated by the caller.
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                             "/o");
  builder.SetDebugLogging(options_.enable_http_tracing());
  builder.AddHeader(options_.credentials()->AuthorizationHeader());
  request.AddParametersToHttpRequest(builder);
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
  request.AddParametersToHttpRequest(builder);
  builder.SetMethod("DELETE");
  auto payload = builder.BuildRequest(std::string{}).MakeRequest();
  if (payload.status_code >= 300) {
    return std::make_pair(
        Status{payload.status_code, std::move(payload.payload)},
        internal::EmptyResponse{});
  }
  return std::make_pair(Status(), internal::EmptyResponse{});
}

std::pair<Status, ListObjectAclResponse> CurlClient::ListObjectAcl(
    ListObjectAclRequest const& request) {
  // Assume the bucket name is validated by the caller.
  CurlRequestBuilder builder(storage_endpoint_ + "/b/" + request.bucket_name() +
                             "/o/" + request.object_name() + "/acl");
  builder.SetDebugLogging(options_.enable_http_tracing());
  builder.AddHeader(options_.credentials()->AuthorizationHeader());
  request.AddParametersToHttpRequest(builder);
  auto payload = builder.BuildRequest(std::string{}).MakeRequest();
  if (payload.status_code >= 300) {
    return std::make_pair(
        Status(),
        internal::ListObjectAclResponse::FromHttpResponse(std::move(payload)));
  }
  return std::make_pair(Status{payload.status_code, std::move(payload.payload)},
                        internal::ListObjectAclResponse{});
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
