// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_POLICY_DOCUMENT_REQUESTS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_POLICY_DOCUMENT_REQUESTS_H_

#include "google/cloud/status_or.h"
#include "google/cloud/storage/bucket_metadata.h"
#include "google/cloud/storage/internal/generic_request.h"
#include "google/cloud/storage/internal/http_response.h"
#include "google/cloud/storage/internal/nljson.h"
#include "google/cloud/storage/policy_document.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
struct PolicyDocumentParser {
  static StatusOr<PolicyDocument> FromJson(internal::nl::json const& json);
  static StatusOr<PolicyDocument> FromString(std::string const& text);
};

class PolicyDocumentRequest {
 public:
  PolicyDocumentRequest() = default;
  explicit PolicyDocumentRequest(std::string bucket_name,
                                 PolicyDocument document)
      : bucket_name_(bucket_name), document_(document) {}

  std::string const& bucket_name() const { return bucket_name_; }

  PolicyDocument const& policy_document() const { return document_; }

  std::chrono::seconds expiration_time_as_seconds() const {
    return std::chrono::duration_cast<std::chrono::seconds>(
        document_.expiration_time.time_since_epoch());
  }

  std::string StringToSign() const;

 private:
  std::string bucket_name_;
  PolicyDocument document_;
};
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_POLICY_DOCUMENT_REQUESTS_H_
