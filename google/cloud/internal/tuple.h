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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_TUPLE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_TUPLE_H

#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/internal/utility.h"
#include "google/cloud/version.h"
#include <tuple>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

// This header reimplements some of C++14's <tuple> header.

/**
 * Extract the return type of `std::apply` for given argument types.
 */
template <typename F, typename Tuple>
struct ApplyRes {};

/**
 * Extract the return type of `std::apply` for given argument types.
 */
template <typename F, typename... Args>
struct ApplyRes<F, std::tuple<Args...>> {
  using type = typename invoke_result<F, Args...>::type;
};

/**
 * Implementation of `apply`.
 *
 * Inspired by the exampe in section 20.5.1 of the standard.
 *
 * @tparam F the functor to apply the tuple to
 * @tparam Tuple the tuple of arguments to apply to `F`
 * @tparam I indices 0..(sizeof(Tuple)-1)
 */
template <class F, class Tuple, std::size_t... I>
typename ApplyRes<F, typename std::decay<Tuple>::type>::type ApplyImpl(
    F&& f, Tuple&& t, index_sequence<I...>) {
  return std::forward<F>(f)(std::get<I>(std::forward<Tuple>(t))...);
}

// Reimplementation of `std::apply` from C++14
template <class F, class Tuple>
typename ApplyRes<F, typename std::decay<Tuple>::type>::type apply(F&& f,
                                                                   Tuple&& t) {
  using Indices = make_index_sequence<
      std::tuple_size<typename std::decay<Tuple>::type>::value>;
  return ApplyImpl(std::forward<F>(f), std::forward<Tuple>(t), Indices());
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_TUPLE_H
