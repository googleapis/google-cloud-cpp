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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_TUPLE_FILTER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_TUPLE_FILTER_H

#include "google/cloud/storage/version.h"
#include "google/cloud/internal/disjunction.h"
#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/internal/tuple.h"
#include "google/cloud/internal/utility.h"
#include <tuple>
#include <type_traits>
#include <utility>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

/// Prepend a type to tuple's type list - unmatched case.
template <typename TL, typename T>
struct TupleTypePrepend {};

/// Prepend a type to tuple's type list - std::tuple specialization
template <typename T, typename... Args>
struct TupleTypePrepend<std::tuple<Args...>, T> {
  using Result = std::tuple<T, Args...>;
};

/**
 * A helper class to filter a single element from a tuple.
 *
 * Depending on whether to filter the element or not, appropriate tuple type is
 * generated and a "filtering" member function, which either returns an empty
 * tuple or a tuple containing the argument.
 *
 * @tparam T of the filtered element
 * @tparam Filter whether to filter the element.
 */
template <typename T, bool Filter>
struct TupleFilterItem {};

/**
 * Implementation of TupleFilterItem - true branch.
 */
template <typename T>
struct TupleFilterItem<T, true> {
  using Result = std::tuple<T>;
  template <typename U>
  Result operator()(U&& t) const {
    return Result(std::forward<U>(t));
  }
};

/**
 * Implementation of TupleFilterItem - false branch.
 */
template <typename T>
struct TupleFilterItem<T, false> {
  using Result = std::tuple<>;
  Result operator()(T const&) const { return Result(); }
};

/**
 * A helper to compute the return type of `StaticTupleFilter`.
 *
 * @tparam TPred a type predicate telling if an element stays or is filtered out
 * @tparam Tuple the type of the tuple to filter elements from
 */
template <template <class> class TPred, typename Tuple>
struct FilteredTupleReturnType {};

/**
 * Implementation of FilteredTupleReturnType - recursive case.
 *
 * @tparam TPred a type predicate telling if an element stays or is filtered out
 * @tparam Head the type of the first tuple's element
 * @tparam Tail the remaining types tuple's elements
 */
template <template <class> class TPred, typename Head, typename... Tail>
struct FilteredTupleReturnType<TPred, std::tuple<Head, Tail...>> {
  using Result = typename std::conditional<
      TPred<Head>::value,
      typename TupleTypePrepend<
          typename FilteredTupleReturnType<TPred, std::tuple<Tail...>>::Result,
          Head>::Result,
      typename FilteredTupleReturnType<TPred,
                                       std::tuple<Tail...>>::Result>::type;
};

/**
 * Implementation of FilteredTupleReturnType - recursion end / empty tuple.
 *
 * @tparam TPred a type predicate telling if an element stays or is filtered out
 */
template <template <class> class TPred>
struct FilteredTupleReturnType<TPred, std::tuple<>> {
  using Result = std::tuple<>;
};

/// A helper functor passed to `apply` to filter tuples.
template <template <class> class TPred, typename Tuple>
struct StaticTupleFilterImpl {
  template <typename... Args>
  typename FilteredTupleReturnType<TPred, Tuple>::Result operator()(
      Args&&... args) {
    return std::tuple_cat(TupleFilterItem<Args, TPred<Args>::value>()(
        std::forward<Args>(args))...);
  }
};

/**
 * Filter elements from a tuple based on their type.
 *
 * A new tuple is returned with only the elements whose type satisfied the
 * provided type predicate.
 *
 * @tparam TPred a type predicate telling if an element stays or is filtered out
 * @tparam Args the type of the tuple's elements
 *
 * @param tuple the tuple to filter elements from
 */
template <template <class> class TPred, class Tuple>
typename FilteredTupleReturnType<TPred,
                                 typename std::decay<Tuple>::type>::Result
StaticTupleFilter(Tuple&& t) {
  return std::tuple_cat(google::cloud::internal::apply(
      StaticTupleFilterImpl<TPred, typename std::decay<Tuple>::type>(),
      std::forward<Tuple>(t)));
}

/**
 * A factory of template predicates checking for presence on a type list
 *
 * @tparam Types the list of types which for which the predicate returns true.
 */
template <typename... Types>
struct Among {
  template <typename T>
  using TPred = google::cloud::internal::disjunction<
      std::is_same<typename std::decay<T>::type, Types>...>;
};

/**
 * A factory of template predicates checking for lack of presence on a type list
 *
 * @tparam Types the list of types which for which the predicate returns false.
 */
template <typename... Types>
struct NotAmong {
  template <typename T>
  using TPred = std::integral_constant<
      bool, !google::cloud::internal::disjunction<
                std::is_same<typename std::decay<T>::type, Types>...>::value>;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_TUPLE_FILTER_H
