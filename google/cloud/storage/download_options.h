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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_DOWNLOAD_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_DOWNLOAD_OPTIONS_H

#include "google/cloud/storage/internal/complex_option.h"
#include "google/cloud/storage/version.h"
#include <cstdint>
#include <iostream>
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
struct ReadRangeData {
  std::int64_t begin;
  std::int64_t end;
};

/**
 * Request only a portion of the GCS object in a ReadObject operation.
 *
 * Note that the range is right-open, as it is customary in C++. That is, it
 * excludes the `end` byte.
 */
struct ReadRange : public internal::ComplexOption<ReadRange, ReadRangeData> {
  ReadRange() = default;
  explicit ReadRange(std::int64_t begin, std::int64_t end)
      : ComplexOption(ReadRangeData{begin, end}) {}
  static char const* name() { return "read-range"; }
};

inline std::ostream& operator<<(std::ostream& os, ReadRangeData const& rhs) {
  return os << "ReadRangeData={begin=" << rhs.begin << ", end=" << rhs.end
            << "}";
}

/**
 * Download all the data from the GCS object starting at the given offset.
 */
struct ReadFromOffset
    : public internal::ComplexOption<ReadFromOffset, std::int64_t> {
  using ComplexOption::ComplexOption;
  // GCC <= 7.0 does not use the inherited default constructor, redeclare it
  // explicitly
  ReadFromOffset() = default;
  static char const* name() { return "read-offset"; }
};

/**
 * Read last N bytes from the GCS object.
 */
struct ReadLast : public internal::ComplexOption<ReadLast, std::int64_t> {
  using ComplexOption::ComplexOption;
  // GCC <= 7.0 does not use the inherited default constructor, redeclare it
  // explicitly
  ReadLast() = default;
  static char const* name() { return "read-last"; }
};

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_DOWNLOAD_OPTIONS_H
