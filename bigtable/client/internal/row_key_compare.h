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

#ifndef GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_INTERNAL_ROW_KEY_COMPARE_H_
#define GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_INTERNAL_ROW_KEY_COMPARE_H_

#include "bigtable/client/version.h"

#include <absl/strings/string_view.h>

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {
/**
 * Return -1, 0, or 1 if `lhs < rhs`, `lhs == rhs`, or `lhs > rhs`
 * respectively.
 *
 * We need to compare row keys as byte vectors, but std::string is (or can)
 * be based on `signed char` where `\xFF` is less than `\x00`.
 */
int RowKeyCompare(absl::string_view lhs, absl::string_view rhs);

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_INTERNAL_ROW_KEY_COMPARE_H_
