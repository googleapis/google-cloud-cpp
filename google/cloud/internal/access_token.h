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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ACCESS_TOKEN_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ACCESS_TOKEN_H

#include "google/cloud/version.h"
#include <chrono>
#include <iosfwd>
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/// Represents an access token with a known expiration time.
struct AccessToken {
  std::string token;
  std::chrono::system_clock::time_point expiration;
};

std::ostream& operator<<(std::ostream& os, AccessToken const& rhs);

bool operator==(AccessToken const& lhs, AccessToken const& rhs);

inline bool operator!=(AccessToken const& lhs, AccessToken const& rhs) {
  return !(lhs == rhs);
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ACCESS_TOKEN_H
