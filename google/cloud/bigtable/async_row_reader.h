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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_ASYNC_ROW_READER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_ASYNC_ROW_READER_H

#include "google/cloud/bigtable/version.h"
#include <cstdint>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * @deprecated Please use `bigtable::RowReader::NO_ROWS_LIMIT` instead.
 */
template <typename Unused1, typename Unused2>
class AsyncRowReader {
 public:
  /// Special value to be used as rows_limit indicating no limit.
  GOOGLE_CLOUD_CPP_DEPRECATED(
      "`AsyncRowReader<>::NO_ROWS_LIMIT` is deprecated. It is scheduled for "
      "deletion on 2023-05-01. Please use `RowReader::NO_ROWS_LIMIT` instead.")
  // NOLINTNEXTLINE(readability-identifier-naming)
  static std::int64_t constexpr NO_ROWS_LIMIT = 0;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_ASYNC_ROW_READER_H
