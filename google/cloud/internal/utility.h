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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_UTILITY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_UTILITY_H

#include "google/cloud/version.h"
#include <type_traits>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

// This header re-implements some of C++14's <utility> header.

// Re-implementation of C++14 `std::integer_sequence`.
template <class T, T... I>
struct integer_sequence {  // NOLINT(readability-identifier-naming)
  using value_type = T;
  static std::size_t constexpr size() noexcept { return sizeof...(I); }
};

// Re-implementation of C++14 `std::index_sequence`.
template <std::size_t... Ints>
using index_sequence = integer_sequence<std::size_t, Ints...>;

/**
 * Implementation of `make_index_sequence`.
 *
 * `MakeIndexSequenceImpl` references itself accumulating the result
 * indices in `I`. The recursion stops when `N` reaches 0. By that time, `I`
 * should contain the result.
 *
 * `MakeIndexSequenceImpl<T, N, void, I...>::result` will return an
 * `integer_sequence<T, 0, 1, 2, ..., (N - 1), I...>`.
 *
 * @tparam T the type of the index in `index_sequence`
 * @tparam N the counter bounding type recursion; `std::integral_constant` has
 *     to be used because specializing a template with a value of type which is
 *     dependent on a different template parameter is not allowed.
 * @tparam Enable ignored, formal parameter to allow for disabling some
 *     specializations
 * @tparam `I` indices accumulated so far in the recursion.
 */
template <typename T, typename N, typename Enable, T... I>
struct MakeIndexSequenceImpl {};

/**
 * Implementation of `make_index_sequence`.
 *
 * Specialization for N > 0.
 */
template <typename T, T N, T... I>
struct MakeIndexSequenceImpl<T, std::integral_constant<T, N>,
                             typename std::enable_if<(N > 0), void>::type,
                             I...> {
  using result =
      typename MakeIndexSequenceImpl<T, std::integral_constant<T, N - 1>, void,
                                     N - 1, I...>::result;
};

/**
 * Implementation of `make_index_sequence`.
 *
 * Specialization for N == 0.
 */
template <typename T, T... I>
struct MakeIndexSequenceImpl<T, std::integral_constant<T, 0>, void, I...> {
  using result = integer_sequence<T, I...>;
};

template <class T, T N>
using make_integer_sequence =
    typename MakeIndexSequenceImpl<T, std::integral_constant<T, N>,
                                   void>::result;

template <size_t N>
using make_index_sequence = make_integer_sequence<size_t, N>;

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_UTILITY_H
