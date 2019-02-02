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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_GOOGLE_CREDENTIALS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_GOOGLE_CREDENTIALS_H_

#include "google/cloud/storage/oauth2/credentials.h"
#include <memory>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace oauth2 {

/**
 * Produces a Credentials type based on the runtime environment.
 *
 * If the GOOGLE_APPLICATION_CREDENTIALS environment variable is set, the JSON
 * file it points to will be loaded and used to create a credential of the
 * specified type. Otherwise, if running on a Google-hosted environment (e.g.
 * Compute Engine), credentials for the the environment's default service
 * account will be used.
 *
 * @see https://cloud.google.com/docs/authentication/production for details
 * about Application Default %Credentials.
 */
StatusOr<std::shared_ptr<Credentials>> GoogleDefaultCredentials();

//@{
/**
 * @name Functions to manually create specific credential types.
 */

/// Creates an AnonymousCredentials.
std::shared_ptr<Credentials> CreateAnonymousCredentials();

/**
 * Creates an AuthorizedUserCredentials from a JSON file at the specified path.
 *
 * @note It is strongly preferred to instead use service account credentials
 * with Cloud Storage client libraries.
 */
StatusOr<std::shared_ptr<Credentials>>
CreateAuthorizedUserCredentialsFromJsonFilePath(std::string const&);

/**
 * Creates an AuthorizedUserCredentials from a JSON string.
 *
 * @note It is strongly preferred to instead use service account credentials
 * with Cloud Storage client libraries.
 */
StatusOr<std::shared_ptr<Credentials>>
CreateAuthorizedUserCredentialsFromJsonContents(std::string const&);

/// Creates a ServiceAccountCredentials rom a JSON file at the specified path.
StatusOr<std::shared_ptr<Credentials>>
CreateServiceAccountCredentialsFromJsonFilePath(std::string const&);

/// Creates a ServiceAccountCredentials from a JSON string.
StatusOr<std::shared_ptr<Credentials>>
CreateServiceAccountCredentialsFromJsonContents(std::string const&);

/// Creates a ComputeEngineCredentials for the VM's default service account.
std::shared_ptr<Credentials> CreateComputeEngineCredentials();

/// Creates a ComputeEngineCredentials for the VM's specified service account.
std::shared_ptr<Credentials> CreateComputeEngineCredentials(std::string const&);

//@}

}  // namespace oauth2
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_GOOGLE_CREDENTIALS_H_
