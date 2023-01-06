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

#include "google/cloud/internal/access_token.h"
#include "absl/time/time.h"
#include <ostream>
#include <tuple>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

std::ostream& operator<<(std::ostream& os, AccessToken const& rhs) {
  // Tokens are truncated because they contain security secrets.
  return os << "token=<" << rhs.token.substr(0, 32) << ">, expiration="
            << absl::FormatTime(absl::FromChrono(rhs.expiration));
}

bool operator==(AccessToken const& lhs, AccessToken const& rhs) {
  return std::tie(lhs.token, lhs.expiration) ==
         std::tie(rhs.token, rhs.expiration);
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
