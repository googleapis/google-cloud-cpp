// Copyright 2025 Google LLC
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

#include "google/cloud/common_options.h"
#include "google/cloud/options.h"
#include "google/cloud/version.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

Options MakeLocationalEndpointOptions(std::string endpoint) {
  absl::string_view authority = endpoint;
  if (!absl::ConsumePrefix(&authority, "https://")) {
    absl::ConsumePrefix(&authority, "http://");
  }
  std::vector<absl::string_view> splits = absl::StrSplit(authority, ':');
  return Options{}
      .set<EndpointOption>(std::move(endpoint))
      .set<AuthorityOption>(std::string(splits.front()));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
