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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FUTURE_THEN_IMPL_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FUTURE_THEN_IMPL_H_
/**
 * @file
 *
 * Define the `future<T>::then_impl()` template functions.
 *
 * These functions cannot be defined inline, as most template functions are,
 * because the full definition of `future<T>`, `future<R&>` and `future<void>`,
 * must be visible at the point of definition. There is no order of definition
 * for the `future<T>` specializations that would permit us to define the
 * functions inline.
 */

#include "google/cloud/internal/future_generic.h"
#include "google/cloud/internal/future_void.h"

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
// This function cannot be defined inline, as most template functions are,
// because the full definition of `future<T>`, `future<R&>` and `future<void>`,
// must be visible at the point of definition. There is no order of definition
// for the `future<T>` specializations that would permit us to define the
// functions inline.
template <typename T>
template <typename F>
typename internal::then_helper<F, T>::future_t future<T>::then_impl(
    F&& functor, std::false_type) {
  // g++-4.9 gets confused about the use of a protected type alias here, so
  // create a non-protected one:
  using local_state_type = typename internal::future_shared_state<T>;
  // Some type aliases to make the rest of the code more readable.
  using functor_result_t =
      typename internal::then_helper<F, T>::functor_result_t;
  using future_t = typename internal::then_helper<F, T>::future_t;

  // The `shared_state_type` (aka `future_shared_state<T>`) is be written
  // without any reference to the `future<T>` class, otherwise there would
  // be cycling dependencies between the two classes. We must adapt the
  // provided functor, which takes a `future<T>` parameter to take a
  // `shared_ptr<shared_state_type` parameter so it can be consumed by the
  // underlying class. Because we need to support C++11, we use a local class
  // instead of a lambda, as support for move+capture in lambdas is a C++14
  // feature.
  struct adapter {
    explicit adapter(F&& func) : functor(func) {}

    auto operator()(std::shared_ptr<local_state_type> state)
        -> functor_result_t {
      return functor(future<T>(std::move(state)));
    }

    F functor;
  };

  auto output_shared_state = local_state_type::make_continuation(
      this->shared_state_, adapter(std::forward<F>(functor)));

  // Nothing throws after this point, and we have not changed the state if
  // anything did throw.
  this->shared_state_.reset();
  return future_t(std::move(output_shared_state));
}

// This function cannot be defined inline, as most template functions are,
// because the full definition of `future<T>`, `future<R&>` and `future<void>`,
// must be visible at the point of definition. There is no order of definition
// for the `future<T>` specializations that would permit us to define the
// functions inline.
template <typename F>
typename internal::then_helper<F, void>::future_t future<void>::then_impl(
    F&& functor, std::false_type) {
  // g++-4.9 gets confused about the use of a protected type alias here, so
  // create a non-protected one:
  using local_state_type = typename internal::future_shared_state<void>;
  // Some type aliases to make the rest of the code more readable.
  using functor_result_t =
      typename internal::then_helper<F, void>::functor_result_t;
  using future_t = typename internal::then_helper<F, void>::future_t;

  // The `shared_state_type` (aka `future_shared_state<void>`) is written
  // without any reference to the `future<void>` class, otherwise there would
  // be cyclic dependencies between the two classes. We must adapt the
  // provided functor, which takes a `future<void>` parameter to take a
  // `shared_ptr<shared_state_type` parameter so it can be consumed by the
  // underlying class. Because we need to support C++11, we use a local class
  // instead of a lambda, as support for move+capture in lambdas is a C++14
  // feature.
  struct adapter {
    explicit adapter(F&& func) : functor(func) {}

    auto operator()(std::shared_ptr<local_state_type> state)
        -> functor_result_t {
      return functor(future<void>(std::move(state)));
    }

    F functor;
  };

  auto output_shared_state = shared_state_type::make_continuation(
      shared_state_, adapter(std::forward<F>(functor)));

  // Nothing throws after this point, and we have not changed the state if
  // anything did throw.
  shared_state_.reset();
  return future_t(std::move(output_shared_state));
}

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FUTURE_THEN_IMPL_H_
