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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_SHA256_HMAC_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_SHA256_HMAC_H

#include "google/cloud/internal/sha256_type.h"
#include "google/cloud/version.h"
#include "absl/types/span.h"
#include <cstdint>
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

///@{
/**
 * @defgroup Overloads for HMAC(HMAC(...))
 *
 * Compute the SHA256 HMAC (as raw bytes) from @p data using @p key as the key.
 */
Sha256Type Sha256Hmac(std::string const& key, absl::Span<char const> data);
Sha256Type Sha256Hmac(std::string const& key,
                      absl::Span<std::uint8_t const> data);
//@}

///@{
/**
 * @defgroup Overloads for HMAC(HMAC(...))
 *
 * HMAC is often used in chains, as in `HMAC(HMAC(HMAC(key, v1), v2), v3)`.
 * these overloads simplify writing such cases.
 */
Sha256Type Sha256Hmac(Sha256Type const& key, absl::Span<char const> data);
Sha256Type Sha256Hmac(Sha256Type const& key,
                      absl::Span<std::uint8_t const> data);
///@}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_SHA256_HMAC_H
