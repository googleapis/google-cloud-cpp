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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_TYPE_LIST_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_TYPE_LIST_H

#include "google/cloud/version.h"
#include <utility>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

/**
 * A list of types.
 *
 * This can be used instead of `std::tuple` when only the types are needed
 * without any values.
 */
template <typename... T>
struct TypeList {};

/**
 * A metafunction to concatenate `TypeList`s.
 *
 * @note Prefer to use the `TypeListCatT<...>` alias, which removes the need
 *     for `typename` and `::Type`.
 *
 * @par Example:
 *
 * @code
 * struct A {};
 * struct B {};
 * using Foo = TypeList<A, B>;
 *
 * struct C {};
 * using Bar = TypeList<C>;
 *
 * using Both = TypeListCatT<Foo, Bar>;
 * static_assert(std::is_same<TypeList<A, B, C>, Both>::value, "");
 * @endcode
 */
template <typename... T>
struct TypeListCat {};

/**
 * Convenience alias. Prefer this over directly using `TypeListCat`.
 */
template <typename... T>
using TypeListCatT = typename TypeListCat<T...>::Type;

// Full specialization for concatenating nothing.
template <>
struct TypeListCat<> {
  using Type = TypeList<>;
};

// Partial specialization for exactly one `TypeList<...>`.
template <typename T>
struct TypeListCat<T> {
  using Type = T;
};

// Partial specialization for two `TypeList<...>`s.
template <typename... Ts, typename... Us>
struct TypeListCat<TypeList<Ts...>, TypeList<Us...>> {
  using Type = TypeList<Ts..., Us...>;
};

// Partial specialization for more than 2.
template <typename T1, typename T2, typename... T>
struct TypeListCat<T1, T2, T...> {
  using Type = TypeListCatT<TypeListCatT<T1, T2>, T...>;
};

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_TYPE_LIST_H
