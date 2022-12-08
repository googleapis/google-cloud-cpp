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

#include "google/cloud/internal/oauth2_decorate_credentials.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/oauth2_cached_credentials.h"
#include "google/cloud/internal/oauth2_logging_credentials.h"

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::shared_ptr<oauth2_internal::Credentials> Decorate(
    std::shared_ptr<oauth2_internal::Credentials> impl, Options const& opts) {
  impl = WithLogging(std::move(impl), opts, "refresh");
  impl = WithCaching(std::move(impl));
  return WithLogging(std::move(impl), opts, "cached");
}

std::shared_ptr<oauth2_internal::Credentials> WithLogging(
    std::shared_ptr<oauth2_internal::Credentials> impl, Options const& opts,
    std::string stage) {
  if (opts.get<TracingComponentsOption>().count("auth") == 0) return impl;
  return std::make_shared<oauth2_internal::LoggingCredentials>(
      std::move(stage), TracingOptions(), std::move(impl));
}

std::shared_ptr<oauth2_internal::Credentials> WithCaching(
    std::shared_ptr<oauth2_internal::Credentials> impl) {
  return std::make_shared<oauth2_internal::CachedCredentials>(std::move(impl));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
