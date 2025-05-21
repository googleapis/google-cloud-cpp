// Copyright 2018 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FUTURE_THEN_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FUTURE_THEN_IMPL_H

#include "google/cloud/future_generic.h"
#include "google/cloud/future_void.h"
#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/version.h"
#include "absl/functional/function_ref.h"
#include <exception>
#include <type_traits>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

// The implementation of set_value() depends on whether exceptions are enabled.
// Putting a #ifdef in the header can result in ODR violations if the library
// is compiled with exceptions enabled, and then it is used from code where
// exceptions are disabled. Admittedly there are probably many other problems
// with a program compiled in such a way. We can avoid the ODR violation though.
void FutureSetResultDelegate(absl::FunctionRef<void()> set_value,
                             absl::FunctionRef<void(std::exception_ptr)>);

/// Implementation helpers for `future<T>::then().
struct FutureThenImpl {
  // Normalize functor evaluation such that they return the correct monostate
  // for `future<void>`
  template <
      typename Functor, typename T,
      std::enable_if_t<std::is_same<void, invoke_result_t<Functor, T>>::value,
                       int> = 0>
  static auto eval(Functor&& functor, T&& input) {
    functor(std::forward<T>(input));
    return internal::FutureVoid{};
  }

  template <
      typename Functor, typename T,
      std::enable_if_t<!std::is_same<void, invoke_result_t<Functor, T>>::value,
                       int> = 0>
  static auto eval(Functor&& functor, T&& input) {
    return functor(std::forward<T>(input));
  }

  template <typename T>
  static void set_value(future_shared_state<FutureVoid>& output, T&&) {
    output.set_value(FutureVoid{});
  }

  template <typename U, typename T,
            std::enable_if_t<std::is_constructible<U, T>::value, int> = 0>
  static void set_value(future_shared_state<U>& output, T&& value) {
    output.set_value(static_cast<U>(std::forward<T>(value)));
  }

  template <typename T, typename Functor, typename U>
  static void set_result(std::shared_ptr<future_shared_state<U>> output,
                         Functor&& functor, T&& input) {
    FutureSetResultDelegate(
        [&] {
          set_value(*output, eval(std::forward<Functor>(functor),
                                  std::forward<T>(input)));
        },
        [&](std::exception_ptr ex) { output->set_exception(std::move(ex)); });
  }

  /// Transfer the value from @p input to @p output when @p input becomes
  /// satisfied.
  template <typename T, typename U>
  static void unwrap(std::shared_ptr<future_shared_state<U>> output,
                     std::shared_ptr<future_shared_state<T>> input) {
    if (!input) {
      return output->set_exception(
          MakeFutureError(std::future_errc::broken_promise));
    }
    class AndThen : public Continuation<T> {
     public:
      explicit AndThen(std::shared_ptr<future_shared_state<U>> o)
          : output_(std::move(o)) {}

      void Execute(SharedStateType<T>& s) override {
        struct Visitor {
          std::shared_ptr<future_shared_state<U>> output;

          void operator()(absl::monostate) { output->abandon(); }
          void operator()(std::exception_ptr e) {
            output->set_exception(std::move(e));
          }
          void operator()(FutureValueRetrieved) { output->abandon(); }
          void operator()(T v) { set_value(*output, std::move(v)); }
        };
        return absl::visit(Visitor{std::move(output_)}, s.value());
      }

     private:
      std::shared_ptr<future_shared_state<U>> output_;
    };
    input->set_continuation(std::make_unique<AndThen>(std::move(output)));
  }

  /// Transfer the value from @p input to @p output when @p input *and* the
  /// value contained in @p input become satisfied.
  template <typename T, typename U>
  static void unwrap(std::shared_ptr<future_shared_state<U>> output,
                     std::shared_ptr<future_shared_state<future<T>>> input) {
    if (!input) {
      return output->set_exception(
          MakeFutureError(std::future_errc::broken_promise));
    }
    class AndThen : public Continuation<future<T>> {
     public:
      explicit AndThen(std::shared_ptr<future_shared_state<U>> o)
          : output_(std::move(o)) {}

      void Execute(SharedStateType<future<T>>& s) override {
        struct Visitor {
          std::shared_ptr<future_shared_state<U>> output;

          void operator()(absl::monostate) { output->abandon(); }
          void operator()(std::exception_ptr e) {
            output->set_exception(std::move(e));
          }
          void operator()(FutureValueRetrieved) { output->abandon(); }
          void operator()(future<T> v) {
            unwrap(std::move(output), std::move(v.shared_state_));
          }
        };
        return absl::visit(Visitor{std::move(output_)}, s.value());
      }

     private:
      std::shared_ptr<future_shared_state<U>> output_;
    };
    input->set_continuation(std::make_unique<AndThen>(std::move(output)));
  }

  /// Implements `future<T>::then()`.
  template <typename T, typename F>
  static auto then_impl(future<T>& fut, F&& functor) {
    using result_t = invoke_result_t<F, future<T>>;
    using unwrapped_result_t = UnwrappedType<result_t>;

    class AndThen : public Continuation<SharedStateValue<T>> {
     public:
      AndThen(std::shared_ptr<SharedStateType<result_t>> o, F&& f)
          : output_(std::move(o)), functor_(std::forward<F>(f)) {}

      void Execute(SharedStateType<T>& s) override {
        set_result(std::move(output_), std::move(functor_),
                   future<T>(std::make_shared<SharedStateType<T>>(s.value())));
      }

     private:
      std::shared_ptr<SharedStateType<result_t>> output_;
      std::decay_t<F> functor_;
    };

    fut.check_valid();
    auto input = std::move(fut.shared_state_);
    fut.shared_state_.reset();
    auto output = std::make_shared<SharedStateType<unwrapped_result_t>>(
        input->release_cancellation_callback());
    auto result = std::make_shared<SharedStateType<result_t>>();
    input->set_continuation(
        std::make_unique<AndThen>(result, std::forward<F>(functor)));
    unwrap(output, result);
    return future<unwrapped_result_t>(output);
  }

  /// Implements `future<T>::future<T>(future<future<T>>)`.
  template <typename T>
  static auto ctor_unwrap(future<future<T>>&& fut) {
    fut.check_valid();
    auto input = std::shared_ptr<SharedStateType<future<T>>>{};
    input.swap(fut.shared_state_);
    auto output = std::make_shared<SharedStateType<T>>(
        input->release_cancellation_callback());
    unwrap(output, input);
    return output;
  }

  template <typename T, typename U>
  static auto ctor_convert(future<U>&& fut) {
    fut.check_valid();
    auto input = std::shared_ptr<SharedStateType<U>>{};
    input.swap(fut.shared_state_);
    auto output = std::make_shared<SharedStateType<T>>(
        input->release_cancellation_callback());
    unwrap(output, input);
    return output;
  }
};

}  // namespace internal

// These functions cannot be defined inline, as most template functions are,
// because the full definition of `future<T>`, `future<R&>` and `future<void>`,
// must be visible at the point of definition. There is no order of definition
// for the `future<T>` specializations that would permit us to define the
// functions inline.

template <typename T>
future<T>::future(future<future<T>>&& rhs)
    : future<T>(internal::FutureThenImpl::ctor_unwrap(std::move(rhs))) {}

template <typename T>
template <typename U, typename Enable>
future<T>::future(future<U>&& rhs)
    : future<T>(internal::FutureThenImpl::ctor_convert<T>(std::move(rhs))) {}

template <typename T>
template <typename F>
auto future<T>::then(F&& functor)
    -> future<
        internal::UnwrappedType<internal::invoke_result_t<F, future<T>>>> {
  return internal::FutureThenImpl::then_impl(*this, std::forward<F>(functor));
}

inline future<void>::future(future<future<void>>&& rhs)
    : future<void>(internal::FutureThenImpl::ctor_unwrap(std::move(rhs))) {}

template <typename T>
future<void>::future(future<T>&& rhs)
    : future<void>(
          internal::FutureThenImpl::ctor_convert<void>(std::move(rhs))) {}

template <typename F>
auto future<void>::then(F&& functor)
    -> future<
        internal::UnwrappedType<internal::invoke_result_t<F, future<void>>>> {
  return internal::FutureThenImpl::then_impl(*this, std::forward<F>(functor));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FUTURE_THEN_IMPL_H
