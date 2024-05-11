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

#include "google/cloud/internal/service_endpoint.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/universe_domain_options.h"
#include "absl/strings/strip.h"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

std::string UniverseDomainEndpoint(std::string gdu_endpoint,
                                   Options const& options) {
  if (!options.has<UniverseDomainOption>()) return gdu_endpoint;
  // Support both "foo.googleapis.com" and "foo.googleapis.com."
  return absl::StrCat(absl::StripSuffix(absl::StripSuffix(gdu_endpoint, "."),
                                        ".googleapis.com"),
                      ".", options.get<UniverseDomainOption>());
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
