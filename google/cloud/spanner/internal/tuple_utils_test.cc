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
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

TEST(TupleUtils, NumElements) {
  EXPECT_EQ(internal::NumElements<std::vector<int>>::value, 2);
  EXPECT_EQ(internal::NumElements<std::tuple<>>::value, 0);
  EXPECT_EQ(internal::NumElements<std::tuple<int>>::value, 1);
  EXPECT_EQ((internal::NumElements<std::tuple<int, int>>::value), 2);
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
  internal::ForEach(tup, Stringify{}, v);
  EXPECT_THAT(v, testing::ElementsAre("1", "42"));
}

TEST(TupleUtils, ForEachMutate) {
  auto add_one = [](int& x) { x += 1; };
  auto tup = std::make_tuple(1, 2, 3);
  internal::ForEach(tup, add_one);
  EXPECT_EQ(tup, std::make_tuple(2, 3, 4));
}

namespace ns {
// A type that looks like a tuple (i.e., a heterogeneous container), but is not
// a tuple. This will verify that `ForEach` works with tuple-like types. In a
// separate namespace to make sure that `ForEach` works with types in another
// namespace.
template <typename... Ts>
struct NotATuple {
  std::tuple<Ts...> data;
};

// Required ADL extension point to make `NotATuple` iterable like a tuple.
template <std::size_t I, typename... Ts>
auto GetElement(NotATuple<Ts...>& nat) -> decltype(std::get<I>(nat.data)) {
  return std::get<I>(nat.data);
}
}  // namespace ns

TEST(TupleUtils, ForEachStruct) {
  auto not_a_tuple = ns::NotATuple<bool, int>{std::make_tuple(true, 42)};
  std::vector<std::string> v;
  internal::ForEach(not_a_tuple, Stringify{}, v);
  EXPECT_THAT(v, testing::ElementsAre("1", "42"));
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
