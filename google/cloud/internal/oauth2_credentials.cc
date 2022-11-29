// Copyright 2019 Google LLC
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

#include "google/cloud/internal/oauth2_credentials.h"
#include "google/cloud/internal/make_status.h"

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

StatusOr<std::vector<std::uint8_t>> Credentials::SignBlob(
    absl::optional<std::string> const&, std::string const&) const {
  return Status(StatusCode::kUnimplemented,
                "The current credentials cannot sign blobs locally");
}

StatusOr<internal::AccessToken> Credentials::GetToken(
    std::chrono::system_clock::time_point /*tp*/) {
  return internal::UnimplementedError(
      "WIP(#10316) - use decorator for credentials", GCP_ERROR_INFO());
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
