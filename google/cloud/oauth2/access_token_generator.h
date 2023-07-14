// Copyright 2023 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_OAUTH2_ACCESS_TOKEN_GENERATOR_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_OAUTH2_ACCESS_TOKEN_GENERATOR_H

#include "google/cloud/access_token.h"
#include "google/cloud/credentials.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <chrono>
#include <iosfwd>
#include <string>

namespace google {
namespace cloud {
namespace oauth2 {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Defines the interface for access token generators.
 *
 * Instances of this class can generate OAuth2 access tokens. These are used to
 * authenticate with Google Cloud Platform (and other Google Services), and
 * may be useful for applications that directly invoke REST-based services.
 *
 * @par Performance
 * @parblock
 * Creating a new access token is relatively expensive. It may require remote
 * calls via HTTP, or at the very least some (local) cryptographic operations.
 *
 * Most implementations of this class will cache an access token until it is
 * about to expire. Application developers are advised to keep
 * `AccessTokenGenerator` instances for as long as possible. They should also
 * avoid caching the access token themselves, as caching is already provided by
 * the implementation.
 * @endparblock
 *
 * @par Thread Safety
 * It is safe to call an instance of this class from two separate threads.
 *
 * @par Error Handling
 * This class uses `StatusOr<T>` to report errors. When an operation fails to
 * perform its work the returned `StatusOr<T>` contains the error details. If
 * the `ok()` member function in the `StatusOr<T>` returns `true` then it
 * contains the expected result. Please consult the #google::cloud::StatusOr
 * documentation for more details.
 */
class AccessTokenGenerator {
 public:
  /// Destructor.
  virtual ~AccessTokenGenerator() = default;

  /**
   * Returns an OAuth2 access token.
   *
   * This function caches the access token to avoid the cost of recomputing the
   * token on each call.
   */
  virtual StatusOr<AccessToken> GetToken() = 0;
};

/// Returns a token generator based on @p credentials.
std::shared_ptr<AccessTokenGenerator> MakeAccessTokenGenerator(
    Credentials const& credentials);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_OAUTH2_ACCESS_TOKEN_GENERATOR_H
