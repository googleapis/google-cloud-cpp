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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_SIGN_BLOB_REQUESTS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_SIGN_BLOB_REQUESTS_H

#include "google/cloud/storage/internal/generic_request.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/status_or.h"
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/**
 * Represents a request to call the `projects.serviceAccounts.signBlob` API.
 *
 * The Cloud IAM API allows applications to sign blobs using a service account.
 * Assuming the account used to access Google Cloud Platform has enough
 * privileges, this account might be different than the service account used to
 * sign the blob. And in cases where the service account keys are not known, for
 * example when using Google Compute Engine metadata server to access GCP, the
 * blob can be signed without having to download the account keys.
 *
 * In Google Cloud Storage this is useful to create signed URLs and signed
 * policy documents if signing service account keys are not available, as is
 * often the case in GCE or when running as an authorized user.
 *
 * @see
 * https://cloud.google.com/iam/credentials/reference/rest/v1/projects.serviceAccounts/signBlob
 *     for details about the `signBlob` API.
 */
class SignBlobRequest
    : public internal::GenericRequestBase<SignBlobRequest, CustomHeader> {
 public:
  explicit SignBlobRequest(std::string service_account,
                           std::string base64_encoded_blob,
                           std::vector<std::string> delegates)
      : service_account_(std::move(service_account)),
        base64_encoded_blob_(std::move(base64_encoded_blob)),
        delegates_(std::move(delegates)) {}

  std::string const& service_account() const { return service_account_; }
  std::string const& base64_encoded_blob() const {
    return base64_encoded_blob_;
  }
  std::vector<std::string> delegates() const { return delegates_; }

 private:
  std::string service_account_;
  std::string base64_encoded_blob_;
  std::vector<std::string> delegates_;
};

std::ostream& operator<<(std::ostream& os, SignBlobRequest const& r);

/// The response from a `HmacKeys: insert` API.
struct SignBlobResponse {
  static StatusOr<SignBlobResponse> FromHttpResponse(
      std::string const& payload);

  std::string key_id;
  std::string signed_blob;
};

std::ostream& operator<<(std::ostream& os, SignBlobResponse const& r);

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_SIGN_BLOB_REQUESTS_H
