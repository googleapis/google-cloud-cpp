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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_INVOKE_RESULT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_INVOKE_RESULT_H

#include "google/cloud/version.h"
#include <type_traits>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
/**
 * A helper to detect if expressions are valid and use SFINAE.
 *
 * This class will be introduced in C++17, it makes it easier to write SFINAE
 * expressions, basically you use `void_t< some expression here >` in the SFINAE
 * branch that is trying to test if `some expression here` is valid. If the
 * expression is valid it expands to `void` and your branch continues, if it is
 * invalid it fails and the default SFINAE branch is used.
 *
 * @see https://en.cppreference.com/w/cpp/types/void_t for more details about
 *   this class.
 */
template <typename...>
using void_t = void;

/**
 * A helper class to determine the result type of a function-like object.
 *
 * We want to define a class similar to `std::invoke_result<>` from C++17, to do
 * so we need to know the result type of calling the function-like `F` with some
 * parameters. But knowing how to call it depends on what `F` is:
 *
 * For function-like objects you would want to test the expression `f(a...)`,
 * where `f` is of type `F` and `a...` are the arguments
 *
 * For pointer to member-like objects you would want to test the expression
 * `a->f(b...)` where a is the first parameter (if a pointer) and `b...` are the
 * remaining parameters.
 *
 * This of course gets more complicated with references (`a.f(b...)`), derived
 * classes, etc.
 *
 * Fortunately, we just need to implement the easy case (function-like things)
 * in the library:
 *
 * @tparam T the decayed (via `std::decay`) type of F. Currently unused, but if
 *     we ever specialize this class to work with pointers to member functions
 *     and other more complex function-like things it would be used to pick the
 *     specialization.
 */
template <typename T>
struct invoke_impl {
  /**
   * Determine the result type of calling `f(ArgTypes...)`
   *
   * @param f the function-like object to call.
   * @param a the parameters to call @p f with.
   * @tparam F the type of the function-like object, note that in some cases the
   *     function-like object may have multiple overloaded `operator()`, with
   *     different return types for each.
   * @tparam ArgTypes the types of the parameters.
   * @return the result of `f(a...)`;
   */
  template <typename F, typename... ArgTypes>
  static auto call(F&& f, ArgTypes&&... a)
      -> decltype(std::forward<F>(f)(std::forward<ArgTypes>(a)...)) {
    return std::forward<F>(f)(std::forward<ArgTypes>(a)...);
  }
};

/**
 * A specialization of invoke_impl for pointers to member functions.
 *
 * TODO(#1360): extend the implementation to support `shared_ptr<>`,
 * `reference_wrapper`, etc.
 *
 * @tparam B the decayed (via `std::decay`) type of a base class to which the
 *     member function belongs. Currently there is no need for it, but there
 *     might in the future if we want to implement support for smart pointers.
 * @tparam MT the type of the member function, as obtained by `decltype`
 */
template <class B, class MT>
struct invoke_impl<MT B::*> {
  /**
   * Determine the result type of calling `(t.*f)(ArgTypes...)`
   *
   * @param f the member function to call.
   * @param t the object on which to call the member function.
   * @param a the parameters to call @p f with.
   * @tparam T the type of the object on which the member function is run.
   * @tparam ArgTypes the types of the parameters.
   * @return the result of `(t.*f)(a...)`.
   */
  template <class T, class... Args,
            typename std::enable_if<std::is_function<MT>::value, int>::type = 0>
  static auto call(MT B::*f, T&& t, Args&&... a)
      -> decltype((std::forward<T>(t).*f)(std::forward<Args>(a)...));
};

/**
 * Discover the result type of `INVOKE(f, a...)`.
 *
 * Recall that C++11 defines *invoking* a function-like object with given
 * parameters in as calling `F` with the given arguments, where `F` can be a
 * lambda type, a class with (potentially multiple) `operator()` defined, a
 * pointer to function, a pointer to member function, etc.
 *
 * @param f a function-like object to be called.
 * @param a the arguments to call @p `f` with.
 * @tparam F the type of the function-like object.
 * @tparam ArgTypes
 * @tparam Fd the decayed (via std::decay<>) of F.
 * @return
 *
 * @see https://en.cppreference.com/w/cpp/utility/functional/invoke for a longer
 *     discussion about invoking functions in C++.
 *
 * @see https://en.cppreference.com/w/cpp/types/decay for a discussion of
 *     decaying function types.
 */
template <class F, class... ArgTypes, class Fd = typename std::decay<F>::type>
auto invoker_function(F&& f, ArgTypes&&... a)
    -> decltype(invoke_impl<Fd>::call(std::forward<F>(f),
                                      std::forward<ArgTypes>(a)...));

// The default (failure) SFINAE branch for `invoke_result_impl`.
template <typename AlwaysVoid, typename, typename...>
struct invoke_result_impl {};

/**
 * The implementation details for `invoke_result<>`.
 *
 * This meta function discovers if `F` can be invoked with `ArgTypes`, and if
 * so, captures the return type in the `type` trait.
 *
 * @tparam F the type of the function-like object.
 * @tparam ArgTypes the types of the arguments.
 */
template <typename F, typename... ArgTypes>
struct invoke_result_impl<decltype(void(invoker_function(
                              std::declval<F>(), std::declval<ArgTypes>()...))),
                          F, ArgTypes...> {
  using type = decltype(
      invoker_function(std::declval<F>(), std::declval<ArgTypes>()...));
};

/**
 * Implement `std::invoke_result<>` from C++17.
 *
 * `std::invoke_result<>` is a meta-function to discover (a) if
 * `std::invoke(F&&, ArgTypes&&...)` is valid (roughly: if one can call a
 * function-like object of type `F` with parameters `ArgTypes&&`), and (b) what
 * is the return type of `std::invoke(...)` if that is the case.
 *
 * C++11 provides a similar meta-function: `std::result_of<>`, but unfortunately
 * it is undefined behavior to use it with an invalid expression.
 *
 * @tparam F the type of the function-like object.
 * @tparam ArgTypes the type of the arguments.
 *
 * @see https://en.cppreference.com/w/cpp/types/result_of for more information
 *     about `std::invoke_result` and `std::result_of` (deprecated in C++17).
 */
template <typename F, typename... ArgTypes>
struct invoke_result : public invoke_result_impl<void, F, ArgTypes...> {};

/// Implement the C++17 `std::invoke_result_t<>` meta-function.
template <typename F, typename... ArgTypes>
using invoke_result_t = typename invoke_result<F, ArgTypes...>::type;

// The default (failure) SFINAE branch for `is_invocable_impl`.
template <typename AlwaysVoid, typename, typename...>
struct is_invocable_impl : public std::false_type {};

template <typename F, typename... ArgTypes>
struct is_invocable_impl<decltype(void(invoker_function(
                             std::declval<F>(), std::declval<ArgTypes>()...))),
                         F, ArgTypes...> : public std::true_type {};

/**
 * Implement `std::is_invocable<>` from C++17.
 *
 * `std::is_invocable<>` is a meta-function to discover if
 * `std::invoke(F&&, ArgTypes&&...)` is valid (roughly: if one can call a
 * function-like object of type `F` with parameters `ArgTypes&&`).
 *
 * @tparam F the type of the function-like object.
 * @tparam ArgTypes the type of the arguments.
 */
template <typename F, typename... ArgTypes>
struct is_invocable : public is_invocable_impl<void, F, ArgTypes...> {};

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_INVOKE_RESULT_H
