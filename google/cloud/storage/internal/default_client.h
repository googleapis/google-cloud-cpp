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

template <typename HttpRequestor = CurlRequest>
class DefaultClient : public Client {
 public:
  explicit DefaultClient(std::shared_ptr<Credentials> credentials)
      : DefaultClient(ClientOptions(std::move(credentials))) {}

  explicit DefaultClient(ClientOptions options)
      : options_(std::move(options)) {
    storage_endpoint_ = options_.endpoint() + "/storage/" + options_.version();
  }

  std::pair<Status, BucketMetadata> GetBucketMetadata(
      GetBucketMetadataRequest const& request) override {
    // Assume the bucket name is validated by the caller.
    HttpRequestor requestor(storage_endpoint_ + "/b/" + request.bucket_name());
    requestor.AddWellKnownParameters(request.well_known_parameters());
    requestor.AddHeader("Authorization" +
                        options_.credentials()->AuthorizationHeader());
    requestor.PrepareRequest(std::string{});
    auto payload = requestor.MakeRequest();
    if (200 != payload.status_code) {
      return std::make_pair(
          Status{payload.status_code, std::move(payload.payload)},
          BucketMetadata{});
    }
    return std::make_pair(Status(),
                          BucketMetadata::ParseFromJson(payload.payload));
  }

 private:
  ClientOptions options_;
  std::string storage_endpoint_;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_DEFAULT_CLIENT_H_
