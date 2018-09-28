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

#include "google/cloud/internal/throw_delegate.h"
#include "google/cloud/storage/oauth2/anonymous_credentials.h"
#include "google/cloud/storage/oauth2/authorized_user_credentials.h"
#include "google/cloud/storage/oauth2/service_account_credentials.h"
#include "google/cloud/storage/version.h"
#include <memory>
#include <sstream>
#include <utility>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace oauth2 {

/**
 * Produces a credential type based on a the runtime environment. If the
 * GOOGLE_APPLICATION_CREDENTIALS environment variable is set, the JSON file it
 * points to will be loaded and used to create a credential of the specified
 * type. Otherwise, if running on a Google-hosted environment (Compute Engine,
 * etc.), credentials for the the environment's default service account will be
 * used.
 */
std::shared_ptr<Credentials> GoogleDefaultCredentials();

// Below are functions to manually create specific credential types.

/// Creates an anonymous credential.
std::shared_ptr<AnonymousCredentials> CreateAnonymousCredentials();

/**
 * Creates an authorized user credential. Note that it is strongly preferred to
 * instead use service account credentials with Cloud Storage client libraries.
 */
std::shared_ptr<AuthorizedUserCredentials<>>
    CreateAuthorizedUserCredentialsFromJsonFilePath(std::string);

/**
 * Creates an authorized user credential. Note that it is strongly preferred to
 * instead use service account credentials with Cloud Storage client libraries.
 */
std::shared_ptr<AuthorizedUserCredentials<>>
    CreateAuthorizedUserCredentialsFromJsonContents(std::string);

/// Creates a service account credential.
std::shared_ptr<ServiceAccountCredentials<>>
    CreateServiceAccountCredentialsFromJsonFilePath(std::string);

/// Creates a service account credential.
std::shared_ptr<ServiceAccountCredentials<>>
    CreateServiceAccountCredentialsFromJsonContents(std::string);

// TODO(#579): Should we support loading service account credentials from a P12
// file too? Other libraries do, but the JSON format is strongly preferred.

namespace internal {

/**
 * Returns the path to the file containing Application Default Credentials, as
 * set in the GOOGLE_APPLICATION_CREDENTIALS environment variable. Returns an
 * empty string if no such path exists.
 */
std::string GoogleAdcFilePathOrEmpty();

}  // namespace internal

}  // namespace oauth2
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OAUTH2_GOOGLE_CREDENTIALS_H_
