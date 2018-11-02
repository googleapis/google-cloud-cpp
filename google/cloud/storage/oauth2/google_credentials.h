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
 * Produces a credential type based on a the runtime environment.
 *
 * If the GOOGLE_APPLICATION_CREDENTIALS environment variable is set, the JSON
 * file it points to will be loaded and used to create a credential of the
 * specified type. Otherwise, if running on a Google-hosted environment (Compute
 * Engine, etc.), credentials for the the environment's default service account
 * will be used.
 */
std::shared_ptr<Credentials> GoogleDefaultCredentials();

//@{
/**
 * @name Functions to manually create specific credential types.
 */

/// Creates an "anonymous" credential.
std::shared_ptr<Credentials> CreateAnonymousCredentials();

/**
 * Creates an "authorized user" credential from a JSON file at the given path.
 *
 * @note It is strongly preferred to instead use service account credentials
 * with Cloud Storage client libraries.
 */
std::shared_ptr<Credentials> CreateAuthorizedUserCredentialsFromJsonFilePath(
    std::string const&);

/**
 * Creates an "authorized user" credential from a JSON string.
 *
 * @note It is strongly preferred to instead use service account credentials
 * with Cloud Storage client libraries.
 */
std::shared_ptr<Credentials> CreateAuthorizedUserCredentialsFromJsonContents(
    std::string const&);

/// Creates a "service account" credential from a JSON file at the given path.
std::shared_ptr<Credentials> CreateServiceAccountCredentialsFromJsonFilePath(
    std::string const&);

/// Creates a "service account" credential from a JSON string.
std::shared_ptr<Credentials> CreateServiceAccountCredentialsFromJsonContents(
    std::string const&);

/// Creates a Google Compute Engine credential for the default service account.
std::shared_ptr<Credentials> CreateComputeEngineCredentials();

/// Creates a Google Compute Engine credential for the given service account.
std::shared_ptr<Credentials> CreateComputeEngineCredentials(std::string const&);

// TODO(#1193): Should we support loading service account credentials from a P12
// file too? Other libraries do, but the JSON format is strongly preferred.

//@}

}  // namespace oauth2
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_GOOGLE_CREDENTIALS_H_
