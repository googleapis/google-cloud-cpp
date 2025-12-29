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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_CLIENT_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_CLIENT_OPTIONS_H

#include "google/cloud/storage/oauth2/credentials.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/common_options.h"
#include "google/cloud/options.h"
#include <chrono>
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
std::string RestEndpoint(Options const&);
std::string IamEndpoint(Options const&);

Options ApplyPolicy(Options opts, RetryPolicy const& p);
Options ApplyPolicy(Options opts, BackoffPolicy const& p);
Options ApplyPolicy(Options opts, IdempotencyPolicy const& p);

inline Options ApplyPolicies(Options opts) { return opts; }

template <typename P, typename... Policies>
Options ApplyPolicies(Options opts, P&& head, Policies&&... tail) {
  opts = ApplyPolicy(std::move(opts), std::forward<P>(head));
  return ApplyPolicies(std::move(opts), std::forward<Policies>(tail)...);
}

Options DefaultOptions(std::shared_ptr<oauth2::Credentials> credentials,
                       Options opts);
Options DefaultOptionsWithCredentials(Options opts);

}  // namespace internal

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_CLIENT_OPTIONS_H
