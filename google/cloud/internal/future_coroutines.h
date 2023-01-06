// Copyright 2022 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FUTURE_COROUTINES_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FUTURE_COROUTINES_H

#include "google/cloud/internal/port_platform.h"
#if GOOGLE_CLOUD_CPP_HAVE_COROUTINES
#include "google/cloud/future_generic.h"
#include "google/cloud/future_void.h"
#include "google/cloud/version.h"
#include <coroutine>
#include <memory>

/// Specialize `std::coroutine_traits` for `google::cloud::future<T>`.
template <typename T, typename... Args>
requires(!std::is_void_v<T> && !std::is_reference_v<T>) /**/
    struct std::coroutine_traits<google::cloud::future<T>, Args...> {
  struct promise_type {
    google::cloud::promise<T> impl;

    [[nodiscard]] google::cloud::future<T> get_return_object() noexcept {
      return impl.get_future();
    }

    [[nodiscard]] std::suspend_never initial_suspend() const noexcept {
      return {};
    }
    [[nodiscard]] std::suspend_never final_suspend() const noexcept {
      return {};
    }

    void return_value(T const& value) noexcept(
        std::is_nothrow_copy_constructible_v<T>) {
      impl.set_value(value);
    }
    void return_value(T&& value) noexcept(
        std::is_nothrow_move_constructible_v<T>) {
      impl.set_value(std::move(value));
    }
    void unhandled_exception() noexcept {
      impl.set_exception(std::current_exception());
    }
  };
};

/// Specialize `std::coroutine_traits` for `google::cloud::future<void>`.
template <typename... Args>
struct std::coroutine_traits<google::cloud::future<void>, Args...> {
  struct promise_type {
    google::cloud::promise<void> impl;

    [[nodiscard]] google::cloud::future<void> get_return_object() noexcept {
      return impl.get_future();
    }

    [[nodiscard]] std::suspend_never initial_suspend() const noexcept {
      return {};
    }
    [[nodiscard]] std::suspend_never final_suspend() const noexcept {
      return {};
    }

    void return_void() noexcept { impl.set_value(); }
    void unhandled_exception() noexcept {
      impl.set_exception(std::current_exception());
    }
  };
};

namespace google::cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// Overload `co_await` for `google::cloud::future<T>`
template <typename T>
auto operator co_await(future<T> f) noexcept
    requires(!std::is_reference_v<T>) /**/ {
  struct awaiter {
    future<T> impl;

    /// Return `true` if the future is already satisfied.
    [[nodiscard]] bool await_ready() const noexcept {
      using namespace std::chrono_literals;
      return impl.is_ready();
    }

    /// Suspend execution until the future becomes satisfied.
    void await_suspend(std::coroutine_handle<> h) {
      struct continuation : public internal::continuation_base {
        std::coroutine_handle<> handle;

        explicit continuation(std::coroutine_handle<>&& h)
            : handle(std::move(h)) {}

        // When the future becomes satisfied we resume the coroutine. At that
        // point the coroutine will call `await_resume()` to get the value.
        void execute() override { handle.resume(); }
      };

      // We cannot use `impl.then()` because that returns a new future, and
      // coroutines expect the future to remain unchanged.  We reach into the
      // future's internals to set up a callback without invalidating the
      // future.
      auto shared_state = internal::CoroutineSupport::get_shared_state(impl);
      shared_state->set_continuation(
          std::make_unique<continuation>(std::move(h)));
    }

    // Get the value (or exception) from the future.
    T await_resume() { return impl.get(); }
  };

  return awaiter{std::move(f)};
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace google::cloud

#endif  // GOOGLE_CLOUD_CPP_CPP_HAVE_COROUTINES
#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FUTURE_COROUTINES_H
