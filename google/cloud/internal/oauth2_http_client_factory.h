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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_HTTP_CLIENT_FACTORY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_HTTP_CLIENT_FACTORY_H

#include "google/cloud/internal/rest_client.h"
#include "google/cloud/options.h"
#include "google/cloud/version.h"
#include <functional>
#include <memory>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Create a HTTP client.
 *
 * Many of the classes derived from `oauth2_internal::Credentials` need to
 * perform HTTP operations to complete their work.  Most of the time the results
 * of these HTTP operations are cached for multiple minutes, often for as long
 * as an hour.  Keeping a RestClient does not provide any benefits, as the
 * underlying connections will be closed by the time a new HTTP request is made.
 *
 * Furthermore, some of the `oauth2_internal::Credential` classes need to
 * perform requests to many different endpoints.
 *
 * For these reasons, the classes are better off consuming a factory to create
 * new `RestClient` objects as needed.
 */
using HttpClientFactory =
    std::function<std::unique_ptr<rest_internal::RestClient>(Options const&)>;

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_HTTP_CLIENT_FACTORY_H
