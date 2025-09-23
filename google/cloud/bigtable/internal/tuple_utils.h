// Copyright 2025 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_TUPLE_UTILS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_TUPLE_UTILS_H

#include "google/cloud/bigtable/version.h"
#include <tuple>
#include <type_traits>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// The implementation for `IsTuple<T>` (below).
template <typename T>
struct IsTupleImpl : std::false_type {};
template <typename... Ts>
struct IsTupleImpl<std::tuple<Ts...>> : std::true_type {};

// Decays the given type `T` and determines whether it is a `std::tuple<...>`.
//
// Example:
//
//     using Type = std::tuple<...>;
//     static_assert(IsTuple<Type>::value, "");
//
template <typename T>
using IsTuple = IsTupleImpl<std::decay_t<T>>;

// Decays the tuple `T` and returns its size as in the ::value member.
template <typename T>
using TupleSize = std::tuple_size<std::decay_t<T>>;

// Base case of `ForEach` that is called at the end of iterating a tuple.
// See the docs for the next overload to see how to use `ForEach`.
template <std::size_t I = 0, typename T, typename F, typename... Args>
std::enable_if_t<I == TupleSize<T>::value, void> ForEach(T&&, F&&, Args&&...) {}

// This function template iterates the elements of a tuple, calling the given
// functor with each of the tuple's elements as well as any additional
// (optional) caller-provided arguments. The given functor should be able to
// accept each type in the container. All arguments are perfect-forwarded to
// the functor, so the functor may choose to accept the tuple arguments by
// value, const-ref, or even non-const reference, in which case the elements
// inside the tuple may be modified.
//
// Example:
//
//     struct Stringify {
//       template <typename T>
//       void operator()(T& t, std::vector<std::string>& out) const {
//         out.push_back(std::to_string(t));
//       }
//     };
//     auto tup = std::make_tuple(true, 42);
//     std::vector<std::string> v;
//     internal::ForEach(tup, Stringify{}, v);
//     EXPECT_THAT(v, testing::ElementsAre("1", "42"));
//
template <std::size_t I = 0, typename T, typename F, typename... Args>
std::enable_if_t<(I < TupleSize<T>::value), void> ForEach(T&& t, F&& f,
                                                          Args&&... args) {
  auto&& e = std::get<I>(std::forward<T>(t));
  std::forward<F>(f)(std::forward<decltype(e)>(e), std::forward<Args>(args)...);
  ForEach<I + 1>(std::forward<T>(t), std::forward<F>(f),
                 std::forward<Args>(args)...);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_TUPLE_UTILS_H
