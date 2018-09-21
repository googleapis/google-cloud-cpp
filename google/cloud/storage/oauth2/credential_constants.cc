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

#include "google/cloud/storage/oauth2/credential_constants.h"
#include "google/cloud/storage/version.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace oauth2 {

char const kGoogleOAuthRefreshEndpoint[] =
    "https://oauth2.googleapis.com/token";
char const kGoogleOAuthScopeCloudPlatform[] =
    "https://www.googleapis.com/auth/cloud-platform";
char const kGoogleOAuthScopeCloudStorageReadOnly[] =
    "https://www.googleapis.com/auth/devstorage.read_only";

namespace internal {

#ifdef _WIN32
char const kGoogleAdcHomeVar[] = "APPDATA";
char const kGoogleAdcWellKnownPathSuffix[] =
    "/gcloud/application_default_credentials.json";
#else
char const kGoogleAdcHomeVar[] = "HOME";
char const kGoogleAdcWellKnownPathSuffix[] =
    "/.config/gcloud/application_default_credentials.json";
#endif

char const kGoogleOAuthJwtGrantType[] =
    "urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer";

}  // namespace internal

}  // namespace oauth2
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
