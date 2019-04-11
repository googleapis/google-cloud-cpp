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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_POLICY_DOCUMENT_REQUEST_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_POLICY_DOCUMENT_REQUEST_H_

#include "google/cloud/storage/policy_document.h"
#include "google/cloud/storage/signed_url_options.h"
#include "google/cloud/storage/version.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

/**
 * Creates a policy document.
 */
class PolicyDocumentRequest {
 public:
  PolicyDocumentRequest() = default;
  PolicyDocumentRequest(PolicyDocument document) : document_(document) {}

  PolicyDocument const& policy_document() const { return document_; }

  /**
   * Creates the string to be signed.
   *
   * @note unlike signed URL's, policy documents are base64-encoded before
   * being signed.
   */
  std::string StringToSign() const;

  SigningAccount const& signing_account() const { return signing_account_; }
  SigningAccountDelegates const& signing_account_delegates() const {
    return signing_account_delegates_;
  }

  void SetOption(SigningAccount const& o) { signing_account_ = o; }

  void SetOption(SigningAccountDelegates const& o) {
    signing_account_delegates_ = o;
  }

  template <typename H, typename... T>
  PolicyDocumentRequest& set_multiple_options(H&& h, T&&... tail) {
    SetOption(std::forward<H>(h));
    return set_multiple_options(std::forward<T>(tail)...);
  }

  PolicyDocumentRequest& set_multiple_options() { return *this; }

 private:
  PolicyDocument document_;
  SigningAccount signing_account_;
  SigningAccountDelegates signing_account_delegates_;
};

std::ostream& operator<<(std::ostream& os, PolicyDocumentRequest const& r);

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_POLICY_DOCUMENT_REQUEST_H_
