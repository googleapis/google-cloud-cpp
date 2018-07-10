// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_BINARY_DATA_AS_DEBUG_STRING_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_BINARY_DATA_AS_DEBUG_STRING_H_

#include "google/cloud/storage/version.h"
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/**
 * Format a block of data for debug printing.
 *
 * Takes a block of data, possible with non-printable characters and creates
 * a string with two columns.  The first column is 24 characters wide and has
 * the non-printable characters replaced by periods.  The second column is 48
 * characters wide and contains the hexdump of the data.  The columns are
 * separated by a single space.
 */
std::string BinaryDataAsDebugString(char const* data, std::size_t size,
                                    std::size_t max_output_bytes = 0);
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_BINARY_DATA_AS_DEBUG_STRING_H_
