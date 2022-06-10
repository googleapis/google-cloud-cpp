// Copyright 2018 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_GOOGLE_CREDENTIALS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_GOOGLE_CREDENTIALS_H

#include "google/cloud/internal/oauth2_credentials.h"
#include "google/cloud/optional.h"
#include "google/cloud/options.h"
#include "google/cloud/version.h"
#include "absl/types/optional.h"
#include <memory>
#include <set>
#include <string>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

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
StatusOr<std::shared_ptr<Credentials>> GoogleDefaultCredentials(
    Options const& options = {});

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_GOOGLE_CREDENTIALS_H
