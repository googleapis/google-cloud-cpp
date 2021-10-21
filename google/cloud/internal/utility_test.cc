// Copyright 2019 Google LLC
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

#include "google/cloud/internal/utility.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

TEST(MakeIndexSequence, Simple) {
  static_assert(std::is_same<integer_sequence<std::size_t>,
                             make_index_sequence<0>>::value,
                "");
  static_assert(std::is_same<integer_sequence<std::size_t, 0, 1, 2, 3, 4>,
                             make_index_sequence<5>>::value,
                "");
  static_assert(
      std::is_same<integer_sequence<int>, make_integer_sequence<int, 0>>::value,
      "");
  static_assert(std::is_same<integer_sequence<int, 0, 1, 2, 3, 4>,
                             make_integer_sequence<int, 5>>::value,
                "");
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
