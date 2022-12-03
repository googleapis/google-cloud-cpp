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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_MOCKS_MOCK_STREAM_RANGE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_MOCKS_MOCK_STREAM_RANGE_H

#include "google/cloud/status.h"
#include "google/cloud/stream_range.h"
#include <vector>

namespace google {
namespace cloud {
/**
 * Contains helpers for testing the Google Cloud C++ Client Libraries.
 *
 * The symbols defined in this namespace are offered for public consumption.
 * They are not just for maintainers of the client library.
 */
namespace mocks {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Construct a `StreamRange<T>` for use in tests.
 *
 * @param values The successfully returned `T` values.
 * @param final_status The final Status of the range. Defaults to an OK Status.
 *
 * @code
 * auto sr = MakeStreamRange<T>({t1, t2});
 * for (StatusOr<int> const& v : sr) {
 *   // Yields t1 -> t2
 * }
 *
 * sr = MakeStreamRange<T>({t1, t2}, BadStatus());
 * for (StatusOr<int> const& v : sr) {
 *   // Yields t1 -> t2 -> BadStatus()
 * }
 * @endcode
 */
template <typename T>
StreamRange<T> MakeStreamRange(std::vector<T> values,
                               Status final_status = {}) {
  auto iter = values.begin();
  auto reader =
      [v = std::move(values), it = std::move(iter),
       s = std::move(final_status)]() mutable -> absl::variant<Status, T> {
    if (it == v.end()) return s;
    return *it++;
  };
  return internal::MakeStreamRange<T>(std::move(reader));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace mocks
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_MOCKS_MOCK_STREAM_RANGE_H
