// Copyright 2022 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_EXTERNAL_ACCOUNT_TOKEN_SOURCE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_EXTERNAL_ACCOUNT_TOKEN_SOURCE_H

#include "google/cloud/internal/subject_token.h"
#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <functional>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Return subject tokens for external account credentials.
 *
 * External accounts credentials use [OAuth 2.0 Token Exchange][RFC 8693] to
 * convert a "subject token" into an "access token". The latter is used
 * (as one would expect) to access GCP services.
 *
 * External accounts may obtain the subject tokens from several different
 * sources. The configuration for these sources can get quite involved, and the
 * sequence of HTTP requests to get a token can be fairly involved too. These
 * sources include:
 * - A single HTTP request, maybe to a local metadata service or OIDC service.
 * - Multiple HTTP requests, where data obtained from previous requests is
 *   used in the next requests (AWS works like this).
 * - A simple file that contains the token, maybe as a field in some JSON
 *   object.
 * - A local program may generate the token, maybe returned as a field in a JSON
 *   object.
 *
 * This type encapsulates all this complexity behind a simple API. The functor
 * is created as part of parsing the external account configuration. To use it
 * one needs to just make a call.
 *
 * [RFC 8663]: https://www.rfc-editor.org/rfc/rfc8693.html
 */
using ExternalAccountTokenSource =
    std::function<StatusOr<internal::SubjectToken>(Options)>;

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_EXTERNAL_ACCOUNT_TOKEN_SOURCE_H
