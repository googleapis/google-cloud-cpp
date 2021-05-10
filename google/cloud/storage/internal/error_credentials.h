// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ERROR_CREDENTIALS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ERROR_CREDENTIALS_H

#include "google/cloud/storage/oauth2/credentials.h"
#include "google/cloud/storage/version.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

/**
 * Report errors loading credentials when the RPC is called.
 *
 * With the "unified authentication client" approach the application just
 * declares its *intent*, e.g., "load the default credentials", the actual work
 * is delayed and depends on how the client library is implemented. We also want
 * the behavior with gRPC and REST to be as similar as possible.
 *
 * For some credential types (e.g. service account impersonation) there may be
 * problems with the credentials that are not manifest until after several RPCs
 * succeed.
 *
 * For gRPC, creating the credentials always succeeds, but using them may fail.
 *
 * With REST we typically validate the credentials when loaded, and then again
 * when we try to use them.
 *
 * This last approach was problematic, because it made some credentials fail
 * early. This class allows us to treat all credentials, including REST
 * credentials that failed to load as "evaluated at RPC time".
 */
class ErrorCredentials : public oauth2::Credentials {
 public:
  explicit ErrorCredentials(Status status) : status_(std::move(status)) {}

  StatusOr<std::string> AuthorizationHeader() override;

 private:
  Status status_;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ERROR_CREDENTIALS_H
