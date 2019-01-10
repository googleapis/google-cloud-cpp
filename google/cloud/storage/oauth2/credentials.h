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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_CREDENTIALS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_CREDENTIALS_H_

#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include "google/cloud/storage/version.h"
#include <chrono>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace oauth2 {
/**
 * Interface for OAuth 2.0 credentials used to access Google Cloud services.
 *
 * Instantiating a specific kind of Credentials should usually be done via the
 * convenience methods declared in google_credentials.h.
 *
 * @see https://cloud.google.com/docs/authentication/ for an overview of
 * authenticating to Google Cloud Platform APIs.
 */
class Credentials {
 public:
  virtual ~Credentials() = default;

  /**
   * Returns a pair consisting of:
   * - The status containing failure details if we were unable to obtain a
   *   value for the Authorization header. For credentials that need to be
   *   periodically refreshed, this might include failure details from a refresh
   *   HTTP request. Otherwise an OK status is returned.
   * - The value for the Authorization header in HTTP requests, or an empty
   *   string if we were unable to obtain one.
   */
  virtual StatusOr<std::string> AuthorizationHeader() = 0;
};

}  // namespace oauth2
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_CREDENTIALS_H_
