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

#include "google/cloud/pubsub/options.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/service_endpoint.h"

namespace google {
namespace cloud {
namespace pubsub {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

Options IAMPolicyOptions(Options opts) {
  auto const ep =
      internal::UniverseDomainEndpoint("pubsub.googleapis.com", opts);
  return internal::MergeOptions(
      std::move(opts),
      Options{}.set<EndpointOption>(ep).set<AuthorityOption>(ep));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
