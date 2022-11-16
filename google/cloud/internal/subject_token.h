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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_SUBJECT_TOKEN_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_SUBJECT_TOKEN_H

#include "google/cloud/version.h"
#include <chrono>
#include <iosfwd>
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * A slightly more type-safe representation for subject tokens.
 *
 * External accounts credentials use [OAuth 2.0 Token Exchange][RFC 8693] to
 * convert a "subject token" into an "access token". The latter is used (as one
 * would expect) to access GCP services. Tokens are just strings. It is too easy
 * to confuse their roles. A struct to wrap them provides enough type
 * annotations to avoid most mistakes.
 *
 * [RFC 8663]: https://www.rfc-editor.org/rfc/rfc8693.html
 */
struct SubjectToken {
  std::string token;
};

std::ostream& operator<<(std::ostream& os, SubjectToken const& rhs);

bool operator==(SubjectToken const& lhs, SubjectToken const& rhs);

inline bool operator!=(SubjectToken const& lhs, SubjectToken const& rhs) {
  return !(lhs == rhs);
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_SUBJECT_TOKEN_H
