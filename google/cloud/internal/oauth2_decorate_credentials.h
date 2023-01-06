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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_DECORATE_CREDENTIALS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_DECORATE_CREDENTIALS_H

#include "google/cloud/internal/oauth2_credentials.h"
#include "google/cloud/options.h"
#include "google/cloud/version.h"
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// Add a full stack of logging (if requested in @p opts) and caching decorators
/// to the credentials.
std::shared_ptr<oauth2_internal::Credentials> Decorate(
    std::shared_ptr<oauth2_internal::Credentials> impl, Options const& opts);

/// Add only a logging decorator to the credentials if requested in @p opts
std::shared_ptr<oauth2_internal::Credentials> WithLogging(
    std::shared_ptr<oauth2_internal::Credentials> impl, Options const& opts,
    std::string stage);

/// Add only a caching decorator to the credentials.
std::shared_ptr<oauth2_internal::Credentials> WithCaching(
    std::shared_ptr<oauth2_internal::Credentials> impl);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OAUTH2_DECORATE_CREDENTIALS_H
