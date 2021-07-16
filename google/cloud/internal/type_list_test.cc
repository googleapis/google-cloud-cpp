// Copyright 2021 Google LLC
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

#include "google/cloud/internal/type_list.h"
#include <gmock/gmock.h>
#include <type_traits>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

TEST(TypeList, Basic) {
  using A = TypeList<int>;
  using B = TypeList<char, double>;
  static_assert(!std::is_same<A, B>::value, "");
}

TEST(TypeList, Cat) {
  using A = TypeList<int>;
  using B = TypeList<char, double>;
  using C = TypeList<int, char, double>;
  using E = TypeList<>;  // Empty

  // Empty type lists work
  static_assert(std::is_same<E, TypeListCatT<>>::value, "");
  static_assert(std::is_same<E, TypeListCatT<E>>::value, "");
  static_assert(std::is_same<E, TypeListCatT<E, E>>::value, "");

  // A, B, C are not empty
  static_assert(!std::is_same<A, E>::value, "");
  static_assert(!std::is_same<B, E>::value, "");
  static_assert(!std::is_same<C, E>::value, "");

  // A = A + E
  static_assert(std::is_same<A, TypeListCatT<A, E>>::value, "");
  static_assert(std::is_same<A, TypeListCatT<E, A>>::value, "");

  // C = A + B
  static_assert(std::is_same<C, TypeListCatT<A, B>>::value, "");
  static_assert(!std::is_same<C, TypeListCatT<B, A>>::value, "");

  // A + A + A
  using AAA = TypeList<int, int, int>;
  static_assert(std::is_same<AAA, TypeListCatT<A, A, A>>::value, "");

  using ABC = TypeList<int, char, double, int, char, double>;
  static_assert(std::is_same<ABC, TypeListCatT<A, B, C>>::value, "");
  static_assert(std::is_same<ABC, TypeListCatT<A, B, C, E>>::value, "");

  // Empty doesn't mess up concatenations
  static_assert(std::is_same<ABC, TypeListCatT<A, B, E, C>>::value, "");
  static_assert(std::is_same<ABC, TypeListCatT<A, E, B, C>>::value, "");
  static_assert(std::is_same<ABC, TypeListCatT<E, A, B, C>>::value, "");
}

TEST(TypeList, Big) {
  // Verifies that we can concatenate lots of types, which breaks with
  // `std::tuple` on older versions of Clang
  using BigList = TypeList<int, int, int, int, int, int, int, int, int, int>;
  using BiggerList = TypeListCatT<BigList, BigList, BigList, BigList, BigList>;
  static_assert(!std::is_same<BigList, BiggerList>::value, "");
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
