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
#include "google/cloud/internal/absl_str_cat_quiet.h"

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

StatusOr<std::vector<std::uint8_t>> Credentials::SignBlob(
    absl::optional<std::string> const&, std::string const&) const {
  return Status(StatusCode::kUnimplemented,
                "The current credentials cannot sign blobs locally");
}

StatusOr<std::pair<std::string, std::string>> AuthorizationHeader(
    Credentials& credentials, std::chrono::system_clock::time_point tp) {
  auto token = credentials.GetToken(tp);
  if (!token) return std::move(token).status();
  if (token->token.empty()) return std::make_pair(std::string{}, std::string{});
  return std::make_pair(std::string{"Authorization"},
                        absl::StrCat("Bearer ", token->token));
}

StatusOr<std::string> AuthorizationHeaderJoined(
    Credentials& credentials, std::chrono::system_clock::time_point tp) {
  auto token = credentials.GetToken(tp);
  if (!token) return std::move(token).status();
  if (token->token.empty()) return std::string{};
  return absl::StrCat("Authorization: Bearer ", token->token);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
