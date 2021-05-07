// Copyright 2017 Google Inc.
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

#include "google/cloud/bigtable/internal/prefix_range_end.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

std::string PrefixRangeEnd(std::string const& key) {
  auto p = key.find_last_not_of('\xFF');
  // If key is all \xFF then any sequence higher than key starts with the same
  // number of \xFF.  So the end of the range is +infinity, which is represented
  // by the empty string.
  if (p == std::string::npos) return std::string{};
  auto const pos = static_cast<std::string::difference_type>(p);
  std::string result = key;
  // Generally just take the last byte and increment by 1, but if there are
  // trailing \xFF bytes we need to turn those into zeroes and increment the
  // last byte that is not a \xFF.
  std::fill(std::begin(result) + pos + 1, std::end(result), '\0');
  result[pos]++;
  return result;
}

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
