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

#include "google/cloud/spanner/internal/tuple_utils.h"
#include <gmock/gmock.h>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace spanner_internal {
inline namespace SPANNER_CLIENT_NS {

TEST(TupleUtils, IsTuple) {
  using T0 = std::tuple<>;
  static_assert(IsTuple<T0>::value, "");
  static_assert(IsTuple<T0 const>::value, "");
  static_assert(IsTuple<T0 const&>::value, "");

  using T1 = std::tuple<int>;
  static_assert(IsTuple<T1>::value, "");
  static_assert(IsTuple<T1 const>::value, "");
  static_assert(IsTuple<T1 const&>::value, "");

  using TN = std::tuple<int, bool, char>;
  static_assert(IsTuple<TN>::value, "");
  static_assert(IsTuple<TN const>::value, "");
  static_assert(IsTuple<TN const&>::value, "");

  static_assert(!IsTuple<int>::value, "");
  static_assert(!IsTuple<char>::value, "");
  static_assert(!IsTuple<std::vector<int>>::value, "");
}

// Helper functor used to test the `ForEach` function. Uses a templated
// `operator()`.
struct Stringify {
  template <typename T>
  void operator()(T const& t, std::vector<std::string>& out) const {
    out.push_back(std::to_string(t));
  }
};

TEST(TupleUtils, ForEachMultipleTypes) {
  auto tup = std::make_tuple(true, 42);
  std::vector<std::string> v;
  ForEach(tup, Stringify{}, v);
  EXPECT_THAT(v, testing::ElementsAre("1", "42"));
}

TEST(TupleUtils, ForEachMutate) {
  auto add_one = [](int& x) { x += 1; };
  auto tup = std::make_tuple(1, 2, 3);
  ForEach(tup, add_one);
  EXPECT_EQ(tup, std::make_tuple(2, 3, 4));
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
