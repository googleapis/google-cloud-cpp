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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_REST_OBJECT_READ_SOURCE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_REST_OBJECT_READ_SOURCE_H

#include "google/cloud/storage/internal/object_read_source.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/internal/http_payload.h"
#include "google/cloud/internal/rest_response.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include <map>
#include <memory>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

class RestObjectReadSource : public ObjectReadSource {
 public:
  explicit RestObjectReadSource(
      std::unique_ptr<google::cloud::rest_internal::RestResponse> response);

  ~RestObjectReadSource() override = default;

  bool IsOpen() const override { return payload_ && payload_->HasUnreadData(); }

  StatusOr<HttpResponse> Close() override;

  StatusOr<ReadSourceResult> Read(char* buf, std::size_t n) override;

 private:
  google::cloud::rest_internal::HttpStatusCode status_code_;
  std::multimap<std::string, std::string> headers_;
  std::unique_ptr<google::cloud::rest_internal::HttpPayload> payload_;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_REST_OBJECT_READ_SOURCE_H
