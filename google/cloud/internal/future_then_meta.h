// Copyright 2018 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FUTURE_THEN_META_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FUTURE_THEN_META_H
/**
 * @file
 *
 * Define metafunctions used in the implementation for `future<T>::%then()`.
 */

#include "google/cloud/internal/future_fwd.h"
#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/version.h"
#include <memory>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

template <typename T>
class future_shared_state;

/// Compute the return type for a `future<T>::then()`
template <typename FunctorReturn>
struct unwrap_then {  // NOLINT(readability-identifier-naming)
  using type = FunctorReturn;
  using requires_unwrap_t = std::false_type;
};

/// Specialize the `unwrap_then<>` for functors that return `future<U>`
template <typename U>
struct unwrap_then<future<U>> {
  using type = U;
  using requires_unwrap_t = std::true_type;
};

/// Specialize the `unwrap_then<>` for functors that return `future<U>`
template <typename U>
struct unwrap_internal {  // NOLINT(readability-identifier-naming)
  using type = U;
  using requires_unwrap_t = std::false_type;
};

template <typename U>
struct unwrap_internal<std::shared_ptr<internal::future_shared_state<U>>> {
  using type = U;
  using requires_unwrap_t = std::true_type;
};

/**
 * A metafunction to implement `internal::continuation<Functor,T>`.
 *
 * This metafunction implements a number of useful results given a functor type
 * @p Functor, and the value type @p T of a `future_shared_state<T>`.
 *
 * @note The `Functor` in this metafunction is *not* the template parameter
 * passed to `.then()`.  It is a functor, created by `.then()`, that wraps the
 * original argument but operates directly on `future_shared_state<T>`. Without
 * this wrapper the implementation of the continuation classes would need to
 * know about the full definition of `future<T>`, and we could too easily create
 * a cycle where the definition of `future_shared_state<T>` needs the definition
 * of `future<T>` which needs the definition of `future_shared_state<T>.
 *
 * * First it determines if `Functor` meets the requirements, i.e., that it can
 *   be invoked with an object of type `future_shared_state<T>` as its only
 *   argument.
 * * Then it computes the type of the expression `functor(fut)`, where `functor`
 *   is of type `Functor` and `fut` is of type `future<T>`.
 * * It determines if the resulting type requires implicit unwrapping because it
 *   is a `future<U>`.
 * * It computes the type of the shared state needed to implement
 *   `future<T>::%then()`.
 *
 * @tparam Functor the functor to call. Note that this is a functor wrapped by
 *   `future<T>`. It must accept a `std::shared_ptr<future_shared_state<T>>` as
 *   its single input parameter.
 * @tparam T the type contained in the input future.
 */
template <
    typename Functor, typename T,
    typename std::enable_if<
        is_invocable<Functor, std::shared_ptr<future_shared_state<T>>>::value,
        int>::type = 0>
struct continuation_helper {  // NOLINT(readability-identifier-naming)
  /// The type returned by calling the functor with the given future type.
  using functor_result_t =
      invoke_result_t<Functor, std::shared_ptr<future_shared_state<T>>>;

  /// The type returned by `.then()`, which is a `future<U>`.
  using result_t = typename unwrap_then<functor_result_t>::type;

  /// `std::true_type` if `functor_result_t` is a `future<R>`, `std::false_type`
  /// otherwise.
  using requires_unwrap_t =
      typename unwrap_then<functor_result_t>::requires_unwrap_t;

  /// The type of the shared state created by `.then()`.
  using state_t = future_shared_state<result_t>;
};

/**
 * A metafunction to implement `internal::continuation<Functor,T>`.
 *
 * This metafunction implements a number of useful results given a functor type
 * @p Functor, and the value type @p T of a `future_shared_state<T>`.
 *
 * @note The `Functor` in this metafunction is *not* the template parameter
 * passed to `.then()`.  It is a functor, created by `.then()`, that wraps the
 * original argument but operates directly on `future_shared_state<T>`. Without
 * this wrapper the implementation of the continuation classes would need to
 * know about the full definition of `future<T>`, and we could too easily create
 * a cycle where the definition of `future_shared_state<T>` needs the definition
 * of `future<T>` which needs the definition of `future_shared_state<T>.
 *
 * * First it determines if `Functor` meets the requirements, i.e., that it can
 *   be invoked with an object of type `future_shared_state<T>` as its only
 *   argument.
 * * Then in computes the type of the expression `functor(fut)`, where `functor`
 *   is of type `Functor` and `fut` is of type `future<T>`.
 * * It determines if the resulting type requires implicit unwrapping because it
 *   is a `future<U>`.
 * * It computes the type of the shared state needed to implement
 *   `future<T>::%then()`.
 *
 * @tparam Functor the functor to call. Note that this is a functor wrapped by
 *   `future<T>`. It must accept a `std::shared_ptr<future_shared_state<T>>` as
 *   its single input parameter.
 * @tparam T the type contained in the input future.
 */
template <
    typename Functor, typename T,
    typename std::enable_if<
        is_invocable<Functor, std::shared_ptr<future_shared_state<T>>>::value,
        int>::type = 0>
// NOLINTNEXTLINE(readability-identifier-naming)
struct unwrapping_continuation_helper {
  /// The type returned by calling the functor with the given future type.
  using functor_result_t =
      invoke_result_t<Functor, std::shared_ptr<future_shared_state<T>>>;

  /// The type returned by `.then()`, which is a `future<U>`.
  using result_t = typename unwrap_internal<functor_result_t>::type;

  /// `std::true_type` if `functor_result_t` is a `future<R>`, `std::false_type`
  /// otherwise.
  using requires_unwrap_t =
      typename unwrap_internal<functor_result_t>::requires_unwrap_t;

  /// The type of the shared state created by `.then()`.
  using state_t = future_shared_state<result_t>;
};

/**
 * A metafunction to implement `future<T>::%then(Functor&&)`.
 *
 * This metafunction implements a number of useful results given a functor type
 * @p Functor, and the value type @p T of a `future<T>`.
 *
 * * First it determines if `Functor` meets the requirements, i.e., that it can
 *   be invoked with an object of type `future<T>` as its only argument.
 * * Then it computes the type of the expression `functor(fut)`, where `functor`
 *   is of type `Functor` and `fut` is of type `future<T>`.
 * * It determines if the resulting type requires implicit unwrapping because it
 *   is a `future<U>`.
 * * It computes the type of future returned from `future<T>::%then(Functor&&)`.
 *
 * @tparam Functor the functor to call. It must accept a `future<T>` as its
 *     single input parameter.
 * @tparam T the type contained in the input future.
 */
template <typename Functor, typename T,
          typename std::enable_if<is_invocable<Functor, future<T>>::value,
                                  int>::type = 0>
struct then_helper {  // NOLINT(readability-identifier-naming)
  /// The type returned by the functor
  using functor_result_t = invoke_result_t<Functor, future<T>>;

  /// The unwrapped type returned by the functor, i.e., with any `future<>`
  /// stripped.
  using result_t = typename unwrap_then<functor_result_t>::type;

  /// `std::true_type` if `functor_result_t` is a `future<R>`, `std::false_type`
  /// otherwise.
  using requires_unwrap_t =
      typename unwrap_then<functor_result_t>::requires_unwrap_t;

  /// The future type returned by `.then()`.
  using future_t = future<result_t>;

  /// The type of the shared state created by `.then()`.
  using state_t = future_shared_state<result_t>;
};

template <typename T, typename U>
struct make_ready_helper {  // NOLINT(readability-identifier-naming)
  using type = typename std::decay<T>::type;
};

template <typename T, typename X>
struct make_ready_helper<T, std::reference_wrapper<X>> {
  using type = X&;
};

/**
 * Compute the return type of make_ready_future<T>.
 */
template <typename T>
struct make_ready_return {  // NOLINT(readability-identifier-naming)
  using type =
      typename make_ready_helper<T, typename std::decay<T>::type>::type;
};

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FUTURE_THEN_META_H
