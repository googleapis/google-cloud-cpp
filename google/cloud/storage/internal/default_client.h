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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_DEFAULT_CLIENT_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_DEFAULT_CLIENT_H_

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/curl_request.h"

namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/**
 * Implement storage::Client using a class meeting the CurlRequest interface.
 *
 * @tparam HttpRequest a dependency injection point to replace CurlRequest with
 *   a mock.
 *
 * TODO(#717) - document the CurlRequest interface as a concept.
 */
template <typename HttpRequest = CurlRequest>
class DefaultClient : public Client {
 public:
  explicit DefaultClient(std::shared_ptr<Credentials> credentials)
      : DefaultClient(ClientOptions(std::move(credentials))) {}

  explicit DefaultClient(ClientOptions options) : options_(std::move(options)) {
    storage_endpoint_ = options_.endpoint() + "/storage/" + options_.version();
    upload_endpoint_ =
        options_.endpoint() + "/upload/storage/" + options_.version();
  }

  std::pair<Status, BucketMetadata> GetBucketMetadata(
      GetBucketMetadataRequest const& request) override {
    // Assume the bucket name is validated by the caller.
    HttpRequest http_request(storage_endpoint_ + "/b/" + request.bucket_name());
    request.AddParametersToHttpRequest(http_request);
    http_request.AddHeader(options_.credentials()->AuthorizationHeader());
    http_request.PrepareRequest(std::string{});
    auto payload = http_request.MakeRequest();
    if (200 != payload.status_code) {
      return std::make_pair(
          Status{payload.status_code, std::move(payload.payload)},
          BucketMetadata{});
    }
    return std::make_pair(Status(),
                          BucketMetadata::ParseFromJson(payload.payload));
  }

  std::pair<Status, ObjectMetadata> InsertObjectMedia(
      InsertObjectMediaRequest const& request) override {
    // Assume the bucket name is validated by the caller.
    HttpRequest http_request(upload_endpoint_ + "/b/" + request.bucket_name() +
                             "/o");
    http_request.AddQueryParameter("uploadType", "media");
    http_request.AddQueryParameter("name", request.object_name());
    request.AddParametersToHttpRequest(http_request);
    http_request.AddHeader(options_.credentials()->AuthorizationHeader());
    http_request.AddHeader("Content-Type: application/octet-stream");
    http_request.AddHeader("Content-Length: " +
                           std::to_string(request.contents().size()));
    http_request.PrepareRequest(std::move(request.contents()));
    auto payload = http_request.MakeRequest();
    if (200 != payload.status_code) {
      return std::make_pair(
          Status{payload.status_code, std::move(payload.payload)},
          ObjectMetadata{});
    }
    return std::make_pair(Status(),
                          ObjectMetadata::ParseFromJson(payload.payload));
  }

  std::pair<Status, std::string> ReadObjectRangeMedia(
      internal::ReadObjectRangeRequest const& request) override {
    // Assume the bucket name is validated by the caller.
    HttpRequest http_request(storage_endpoint_ + "/b/" + request.bucket_name() +
                             "/o/" + request.object_name());
    http_request.AddQueryParameter("alt", "media");
    request.AddParametersToHttpRequest(http_request);
    http_request.AddHeader(options_.credentials()->AuthorizationHeader());
    // For the moment, we are using range reads to read the objects (see #727)
    // disable decompression because range reads do not work in that case:
    //   https://cloud.google.com/storage/docs/transcoding#range
    // and
    //   https://cloud.google.com/storage/docs/transcoding#decompressive_transcoding
    http_request.AddHeader("Cache-Control: no-transform");
    http_request.AddHeader("Range: bytes=" + std::to_string(request.begin()) +
                           '-' + std::to_string(request.end()));
    http_request.PrepareRequest(std::string{});
    auto payload = http_request.MakeRequest();
    if (200 != payload.status_code) {
      return std::make_pair(
          Status{payload.status_code, std::move(payload.payload)},
          std::string{});
    }
    return std::make_pair(Status(), std::move(payload.payload));
  }

 private:
  ClientOptions options_;
  std::string storage_endpoint_;
  std::string upload_endpoint_;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_DEFAULT_CLIENT_H_
