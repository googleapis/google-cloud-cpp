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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_UTILITY_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_UTILITY_H_

#include "google/cloud/version.h"
#include <type_traits>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

// This header reimplements some of C++14's <utlity> header.

// Reimplementation of C++14 `std::integer_sequence`.
template <class T, T... I>
struct integer_sequence {
  typedef T value_type;
  static constexpr size_t size() noexcept { return sizeof...(I); }
};

// Reimplementation of C++14 `std::index_sequence`.
template <std::size_t... Ints>
using index_sequence = integer_sequence<std::size_t, Ints...>;

namespace detail {

/**
 * Implementation of `make_index_sequence`.
 *
 * `make_index_sequence_impl` references itself accumulating the result
 * indices in `I`. The recursion stops when `N` reaches 0. By that time, `I`
 * should contain the result.
 *
 * `make_index_sequence_impl<T, N, void, I...>::result` will return an
 * `integer_sequence<T, 0, 1, 2, ..., (N - 1), I...>`.
 *
 * @tparam T the type of the index in `index_sequence`
 * @tparam N the counter bounding type recursion; `std::integral_constant` has
 *     to be used because spacializing a template with a value of type which is
 *     dependent on a different template parameter is not allowed.
 * @tparam Enable ignored, formal parameter to allow for disabling some
 *     specializations
 * @tparam `I` indices accumulated so far in the recursion.
 */
template <typename T, typename N, typename Enable, T... I>
struct make_index_sequence_impl {};

/**
 * Implementation od `make_index_sequence`.
 *
 * Specialization for N > 0.
 */
template <typename T, T N, T... I>
struct make_index_sequence_impl<T, std::integral_constant<T, N>,
                                typename std::enable_if<(N > 0), void>::type,
                                I...> {
  using result =
      typename make_index_sequence_impl<T, std::integral_constant<T, N - 1>,
                                        void, N - 1, I...>::result;
};

/**
 * Implementation od `make_index_sequence`.
 *
 * Specialization for N == 0.
 */
template <typename T, T... I>
struct make_index_sequence_impl<T, std::integral_constant<T, 0>, void, I...> {
  using result = integer_sequence<T, I...>;
};

}  // namespace detail

template <class T, T N>
using make_integer_sequence =
    typename detail::make_index_sequence_impl<T, std::integral_constant<T, N>,
                                              void>::result;

template <size_t N>
using make_index_sequence = make_integer_sequence<size_t, N>;

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_UTILITY_H_
