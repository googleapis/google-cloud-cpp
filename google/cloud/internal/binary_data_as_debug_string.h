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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_BINARY_DATA_AS_DEBUG_STRING_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_BINARY_DATA_AS_DEBUG_STRING_H

#include "google/cloud/version.h"
#include <string>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Formats a block of data for debug printing.
 *
 * Converts non-printable characters to '.'. If requested, it truncates the
 * output and adds a '...<truncated>...' marker when it does so.
 */
std::string BinaryDataAsDebugString(char const* data, std::size_t size,
                                    std::size_t max_output_bytes = 0);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_BINARY_DATA_AS_DEBUG_STRING_H
