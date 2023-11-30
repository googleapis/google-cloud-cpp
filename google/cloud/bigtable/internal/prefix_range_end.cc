// Copyright 2017 Google LLC
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

#include "google/cloud/bigtable/internal/prefix_range_end.h"
#include <algorithm>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

std::string PrefixRangeEnd(std::string const& key) {
  auto pos = key.find_last_not_of('\xFF');
  if (pos == std::string::npos) {
    // If key is all \xFF then any sequence higher than key starts with the
    // same number of \xFF.  So the end of the range is +infinity, which is
    // represented by the empty string.
    return std::string();
  }
  // Generally just take the last byte and increment by 1, but if there are
  // trailing \xFF byte we need to turn those into zeroes and increment the last
  // byte that is not a \xFF.
  std::string result = key;
  std::fill(std::begin(result) + pos + 1, std::end(result), '\0');
  result[pos]++;
  return result;
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
