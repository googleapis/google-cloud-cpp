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

#include "google/cloud/future_generic.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/expect_future_error.h"
#include "google/cloud/testing_util/scoped_thread.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace {
using ::testing::HasSubstr;
using testing_util::chrono_literals::operator"" _ms;
using testing_util::ExpectFutureError;
using testing_util::ScopedThread;

/// @test Verify that destructing a promise does not introduce race conditions.
TEST(FutureTestInt, DestroyInWaitingThread) {
  for (int i = 0; i != 1000; ++i) {
    std::thread t;
    {
      promise<int> p;
      t = std::thread([&p] { p.set_value(42); });
      p.get_future().get();
    }
    if (t.joinable()) t.join();
  }
}

/// @test Verify that destructing a promise does not introduce race conditions.
TEST(FutureTestInt, DestroyInSignalingThread) {
  for (int i = 0; i != 1000; ++i) {
    std::thread t;
    {
      promise<int> p;
      future<int> f = p.get_future();
      t = std::thread([](promise<int> p) { p.set_value(42); }, std::move(p));
      f.get();
    }
    if (t.joinable()) t.join();
  }
}

/// @test Verify conformance with section 30.6.5 of the C++14 spec.
// NOLINTNEXTLINE(google-readability-avoid-underscore-in-googletest-name)
TEST(FutureTestInt, conform_30_6_5_3) {
  // TODO(coryan) - allocators are not supported for now.
  static_assert(!std::uses_allocator<promise<int>, std::allocator<int>>::value,
                "promise<int> should use allocators if provided");
}

/// @test Verify conformance with section 30.6.5 of the C++14 spec.
// NOLINTNEXTLINE(google-readability-avoid-underscore-in-googletest-name)
TEST(FutureTestInt, conform_30_6_5_4_default) {
  promise<int> p0;
  auto f0 = p0.get_future();
  p0.set_value(42);
  ASSERT_EQ(std::future_status::ready, f0.wait_for(0_ms));
  f0.get();
  SUCCEED();
}

/// @test Verify conformance with section 30.6.5 of the C++14 spec.
// NOLINTNEXTLINE(google-readability-avoid-underscore-in-googletest-name)
TEST(FutureTestInt, conform_30_6_5_5) {
  // promise<R> move constructor clears the shared state on the moved-from
  // promise.
  promise<int> p0;

  promise<int> p1(std::move(p0));
  auto f1 = p1.get_future();
  p1.set_value(42);
  ASSERT_EQ(std::future_status::ready, f1.wait_for(0_ms));
  f1.get();

  ExpectFutureError(
      [&] {  // NOLINT(bugprone-use-after-move)
        p0.set_value(42);
      },
      std::future_errc::no_state);
}

/// @test Verify conformance with section 30.6.5 of the C++14 spec.
// NOLINTNEXTLINE(google-readability-avoid-underscore-in-googletest-name)
TEST(FutureTestInt, conform_30_6_5_7) {
  // promise<R> destructor abandons the shared state, the associated future
  // becomes satisfied with an exception.
  future<int> f0;
  {
    promise<int> p0;
    f0 = p0.get_future();
    ASSERT_NE(std::future_status::ready, f0.wait_for(0_ms));
    EXPECT_TRUE(f0.valid());
  }
  EXPECT_TRUE(f0.valid());
  ASSERT_EQ(std::future_status::ready, f0.wait_for(0_ms));
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      try { f0.get(); } catch (std::future_error const& ex) {
        EXPECT_EQ(std::future_errc::broken_promise, ex.code());
        throw;
      },
      std::future_error);
#else
  EXPECT_DEATH_IF_SUPPORTED(
      f0.get(),
      "future<T>::get\\(\\) had an exception but exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify conformance with section 30.6.5 of the C++14 spec.
// NOLINTNEXTLINE(google-readability-avoid-underscore-in-googletest-name)
TEST(FutureTestInt, conform_30_6_5_8) {
  // promise<R> move assignment clears the shared state in the moved-from
  // promise.
  promise<int> p0;

  promise<int> p1;
  p1 = std::move(p0);
  auto f1 = p1.get_future();
  p1.set_value(42);
  ASSERT_EQ(std::future_status::ready, f1.wait_for(0_ms));
  f1.get();

  ExpectFutureError(
      [&] {  // NOLINT(bugprone-use-after-move)
        p0.set_value(42);
      },
      std::future_errc::no_state);
}

/// @test Verify conformance with section 30.6.5 of the C++14 spec.
// NOLINTNEXTLINE(google-readability-avoid-underscore-in-googletest-name)
TEST(FutureTestInt, conform_30_6_5_10) {
  // promise<R>::swap() actually swaps shared states.
  promise<int> p0;
  promise<int> p1;
  p0.set_value(42);
  p0.swap(p1);

  auto f0 = p0.get_future();
  auto f1 = p1.get_future();
  ASSERT_NE(std::future_status::ready, f0.wait_for(0_ms));
  ASSERT_EQ(std::future_status::ready, f1.wait_for(0_ms));
  f1.get();
  SUCCEED();
  p0.set_value(42);
  SUCCEED();
  ASSERT_EQ(std::future_status::ready, f0.wait_for(0_ms));
}

/// @test Verify conformance with section 30.6.5 of the C++14 spec.
// NOLINTNEXTLINE(google-readability-avoid-underscore-in-googletest-name)
TEST(FutureTestInt, conform_30_6_5_14_1) {
  // promise<R>::get_future() raises if future was already retrieved.
  promise<int> p0;
  auto f0 = p0.get_future();
  ExpectFutureError([&] { p0.get_future(); },
                    std::future_errc::future_already_retrieved);
}

/// @test Verify conformance with section 30.6.5 of the C++14 spec.
// NOLINTNEXTLINE(google-readability-avoid-underscore-in-googletest-name)
TEST(FutureTestInt, conform_30_6_5_14_2) {
  // promise<R>::get_future() raises if there is no shared state.
  promise<int> p0;
  promise<int> p1(std::move(p0));
  ExpectFutureError([&]  // NOLINT(bugprone-use-after-move)
                    { p0.get_future(); },
                    std::future_errc::no_state);
}

/// @test Verify conformance with section 30.6.5 of the C++14 spec.
// NOLINTNEXTLINE(google-readability-avoid-underscore-in-googletest-name)
TEST(FutureTestInt, conform_30_6_5_15) {
  // promise<R>::set_value() stores the value in the shared state and makes it
  // ready.
  promise<int> p0;
  auto f0 = p0.get_future();
  ASSERT_NE(std::future_status::ready, f0.wait_for(0_ms));
  p0.set_value(42);
  ASSERT_EQ(std::future_status::ready, f0.wait_for(0_ms));
  EXPECT_EQ(42, f0.get());
}

/// @test Verify conformance with section 30.6.5 of the C++14 spec.
// NOLINTNEXTLINE(google-readability-avoid-underscore-in-googletest-name)
TEST(FutureTestInt, conform_30_6_5_16_1) {
  // promise<R>::set_value() raises if there is a value in the shared state.
  promise<int> p0;
  p0.set_value(42);
  ExpectFutureError([&] { p0.set_value(42); },
                    std::future_errc::promise_already_satisfied);
}

/// @test Verify conformance with section 30.6.5 of the C++14 spec.
// NOLINTNEXTLINE(google-readability-avoid-underscore-in-googletest-name)
TEST(FutureTestInt, conform_30_6_5_17_2) {
  // promise<R>::set_value() raises if there is no shared state.
  promise<int> p0;
  promise<int> p1(std::move(p0));
  ExpectFutureError([&]  // NOLINT(bugprone-use-after-move)
                    { p0.set_value(42); },
                    std::future_errc::no_state);
}

/// @test Verify conformance with section 30.6.5 of the C++14 spec.
// NOLINTNEXTLINE(google-readability-avoid-underscore-in-googletest-name)
TEST(FutureTestInt, conform_30_6_5_18) {
  // promise<R>::set_exception() sets an exception and makes the shared state
  // ready.
  promise<int> p0;
  auto f0 = p0.get_future();
  ASSERT_NE(std::future_status::ready, f0.wait_for(0_ms));
  p0.set_exception(std::make_exception_ptr(std::runtime_error("testing")));
  ASSERT_EQ(std::future_status::ready, f0.wait_for(0_ms));
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      try { f0.get(); } catch (std::runtime_error const& ex) {
        EXPECT_EQ(std::string("testing"), ex.what());
        throw;
      },
      std::runtime_error);
#else
  EXPECT_DEATH_IF_SUPPORTED(
      f0.get(),
      "future<T>::get\\(\\) had an exception but exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify conformance with section 30.6.5 of the C++14 spec.
// NOLINTNEXTLINE(google-readability-avoid-underscore-in-googletest-name)
TEST(FutureTestInt, conform_30_6_5_20_1_value) {
  // promise<R>::set_exception() raises if the shared state is already storing
  // a value.
  promise<int> p0;
  p0.set_value(42);
  ExpectFutureError(
      [&] {
        p0.set_exception(
            std::make_exception_ptr(std::runtime_error("testing")));
      },
      std::future_errc::promise_already_satisfied);
}

/// @test Verify conformance with section 30.6.5 of the C++14 spec.
// NOLINTNEXTLINE(google-readability-avoid-underscore-in-googletest-name)
TEST(FutureTestInt, conform_30_6_5_20_1_exception) {
  // promise<R>::set_exception() raises if the shared state is already storing
  // an exception.
  promise<int> p0;
  p0.set_exception(std::make_exception_ptr(std::runtime_error("original ex")));
  ExpectFutureError(
      [&] {
        p0.set_exception(
            std::make_exception_ptr(std::runtime_error("testing")));
      },
      std::future_errc::promise_already_satisfied);
}

/// @test Verify conformance with section 30.6.5 of the C++14 spec.
// NOLINTNEXTLINE(google-readability-avoid-underscore-in-googletest-name)
TEST(FutureTestInt, conform_30_6_5_20_2) {
  // promise<R>::set_exception() raises if the promise does not have a shared
  // state.
  promise<int> p0;
  promise<int> p1(std::move(p0));
  ExpectFutureError(
      [&] {  // NOLINT(bugprone-use-after-move)
        p0.set_exception(
            std::make_exception_ptr(std::runtime_error("testing")));
      },
      std::future_errc::no_state);
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
// NOLINTNEXTLINE(google-readability-avoid-underscore-in-googletest-name)
TEST(FutureTestInt, conform_30_6_6_3_a) {
  // Calling get() on a future with `valid() == false` is undefined, but the
  // implementation is encouraged to raise `future_error` with an error code
  // of `future_errc::no_state`
  future<int> f;
  EXPECT_FALSE(f.valid());
  ExpectFutureError([&] { f.get(); }, std::future_errc::no_state);
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
// NOLINTNEXTLINE(google-readability-avoid-underscore-in-googletest-name)
TEST(FutureTestInt, conform_30_6_6_3_b) {
  // Calling wait() on a future with `valid() == false` is undefined, but the
  // implementation is encouraged to raise `future_error` with an error code
  // of `future_errc::no_state`
  future<int> f;
  EXPECT_FALSE(f.valid());
  ExpectFutureError([&] { f.wait(); }, std::future_errc::no_state);
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
// NOLINTNEXTLINE(google-readability-avoid-underscore-in-googletest-name)
TEST(FutureTestInt, conform_30_6_6_3_c) {
  // Calling wait_for() on a future with `valid() == false` is undefined, but
  // the implementation is encouraged to raise `future_error` with an error code
  // of `future_errc::no_state`
  future<int> f;
  EXPECT_FALSE(f.valid());
  ExpectFutureError([&] { f.wait_for(3_ms); }, std::future_errc::no_state);
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
// NOLINTNEXTLINE(google-readability-avoid-underscore-in-googletest-name)
TEST(FutureTestInt, conform_30_6_6_3_d) {
  // Calling wait_until() on a future with `valid() == false` is undefined, but
  // the implementation is encouraged to raise `future_error` with an error code
  // of `future_errc::no_state`
  future<int> f;
  EXPECT_FALSE(f.valid());
  ExpectFutureError(
      [&] { f.wait_until(std::chrono::system_clock::now() + 3_ms); },
      std::future_errc::no_state);
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
// NOLINTNEXTLINE(google-readability-avoid-underscore-in-googletest-name)
TEST(FutureTestInt, conform_30_6_6_5) {
  // future<int>::future() constructs an empty future with no shared state.
  future<int> f;
  EXPECT_FALSE(f.valid());
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
// NOLINTNEXTLINE(google-readability-avoid-underscore-in-googletest-name)
TEST(FutureTestInt, conform_30_6_6_7) {
  // future<int> move constructor is `noexcept`.
  future<int> f0;
  EXPECT_TRUE(noexcept(future<int>(std::move(f0))));
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
// NOLINTNEXTLINE(google-readability-avoid-underscore-in-googletest-name)
TEST(FutureTestInt, conform_30_6_6_8_a) {
  // future<int> move constructor transfers futures with valid state.
  promise<int> p;
  future<int> f0 = p.get_future();
  EXPECT_TRUE(f0.valid());

  future<int> f1(std::move(f0));
  EXPECT_FALSE(f0.valid());  // NOLINT(bugprone-use-after-move)
  EXPECT_TRUE(f1.valid());
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
// NOLINTNEXTLINE(google-readability-avoid-underscore-in-googletest-name)
TEST(FutureTestInt, conform_30_6_6_8_b) {
  // future<int> move constructor transfers futures with no state.
  future<int> f0;
  EXPECT_FALSE(f0.valid());

  future<int> f1(std::move(f0));
  EXPECT_FALSE(f0.valid());  // NOLINT(bugprone-use-after-move)
  EXPECT_FALSE(f1.valid());
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
// NOLINTNEXTLINE(google-readability-avoid-underscore-in-googletest-name)
TEST(FutureTestInt, conform_30_6_6_9) {
  // future<int> destructor releases the shared state.
  promise<int> p;
  future<int> f0 = p.get_future();
  EXPECT_TRUE(f0.valid());
  // This behavior is not observable, but any violation should be detected by
  // the ASAN builds.
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
// NOLINTNEXTLINE(google-readability-avoid-underscore-in-googletest-name)
TEST(FutureTestInt, conform_30_6_6_10) {
  // Move assignment is noexcept.
  promise<int> p;
  future<int> f0 = p.get_future();
  EXPECT_TRUE(f0.valid());

  future<int> f1;
  EXPECT_TRUE(noexcept(f1 = std::move(f0)));
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
// NOLINTNEXTLINE(google-readability-avoid-underscore-in-googletest-name)
TEST(FutureTestInt, conform_30_6_6_11_a) {
  // future<int> move assignment transfers futures with valid state.
  promise<int> p;
  future<int> f0 = p.get_future();
  EXPECT_TRUE(f0.valid());

  future<int> f1;
  f1 = std::move(f0);
  EXPECT_FALSE(f0.valid());  // NOLINT(bugprone-use-after-move)
  EXPECT_TRUE(f1.valid());
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
// NOLINTNEXTLINE(google-readability-avoid-underscore-in-googletest-name)
TEST(FutureTestInt, conform_30_6_6_11_b) {
  // future<int> move assignment transfers futures with invalid state.
  future<int> f0;
  EXPECT_FALSE(f0.valid());

  future<int> f1;
  f1 = std::move(f0);
  EXPECT_FALSE(f0.valid());  // NOLINT(bugprone-use-after-move)
  EXPECT_FALSE(f1.valid());
}

// Paragraphs 30.6.6.{12,13,14} are about shared_futures which we are
// not implementing yet.

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
// NOLINTNEXTLINE(google-readability-avoid-underscore-in-googletest-name)
TEST(FutureTestInt, conform_30_6_6_15) {
  // future<int>::get() only returns once the promise is satisfied.
  promise<int> p;

  // We use std::promise<> and std::future<> to test our promises and futures.
  // This test uses a number of promises to track progress in a separate thread,
  // and checks the expected conditions in each one.
  std::promise<void> p_get_future_called;
  std::promise<void> f_get_called;

  ScopedThread t([&] {
    future<int> f = p.get_future();
    p_get_future_called.set_value();
    f.get();
    f_get_called.set_value();
  });

  p_get_future_called.get_future().get();
  auto waiter = f_get_called.get_future();
  // thread `t` cannot make progress until we set the promise value.
  EXPECT_EQ(std::future_status::timeout, waiter.wait_for(2_ms));

  p.set_value(42);
  // now thread `t` can make progress.
  ASSERT_EQ(std::future_status::ready, waiter.wait_for(500_ms));
  waiter.get();
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
// NOLINTNEXTLINE(google-readability-avoid-underscore-in-googletest-name)
TEST(FutureTestInt, conform_30_6_6_16_3) {
  // future<int>::get() returns void.
  promise<int> p;
  future<int> f = p.get_future();
  EXPECT_TRUE((std::is_same<decltype(f.get()), int>::value));
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
// NOLINTNEXTLINE(google-readability-avoid-underscore-in-googletest-name)
TEST(FutureTestInt, conform_30_6_6_17) {
  // future<int>::get() throws if an exception was set in the promise.
  promise<int> p;

  future<int> f = p.get_future();
  p.set_exception(std::make_exception_ptr(std::runtime_error("test message")));
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      try { f.get(); } catch (std::runtime_error const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("test message"));
        throw;
      },
      std::runtime_error);
#else
  EXPECT_DEATH_IF_SUPPORTED(
      f.get(),
      "future<T>::get\\(\\) had an exception but exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
// NOLINTNEXTLINE(google-readability-avoid-underscore-in-googletest-name)
TEST(FutureTestInt, conform_30_6_6_18_a) {
  // future<int>::get() releases the shared state.
  promise<int> p;
  future<int> f = p.get_future();
  p.set_value(42);
  EXPECT_EQ(42, f.get());
  EXPECT_FALSE(f.valid());
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
// NOLINTNEXTLINE(google-readability-avoid-underscore-in-googletest-name)
TEST(FutureTestInt, conform_30_6_6_18_b) {
  // future<int>::get() throws releases the shared state.
  promise<int> p;
  future<int> f = p.get_future();
  p.set_exception(std::make_exception_ptr(std::runtime_error("unused")));
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(f.get(), std::runtime_error);
  EXPECT_FALSE(f.valid());
#else
  EXPECT_DEATH_IF_SUPPORTED(
      f.get(),
      "future<T>::get\\(\\) had an exception but exceptions are disabled");
  // Cannot evaluate side effects with EXPECT_DEATH*()
  //     EXPECT_FALSE(f.valid());
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
// NOLINTNEXTLINE(google-readability-avoid-underscore-in-googletest-name)
TEST(FutureTestInt, conform_30_6_6_19_a) {
  // future<int>::valid() returns true when the future has a shared state.
  promise<int> p;
  future<int> const f = p.get_future();
  EXPECT_TRUE(noexcept(f.valid()));
  EXPECT_TRUE(f.valid());
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
// NOLINTNEXTLINE(google-readability-avoid-underscore-in-googletest-name)
TEST(FutureTestInt, conform_30_6_6_19_b) {
  // future<int>::valid() returns false when the future has no shared state.
  future<int> const f;
  EXPECT_TRUE(noexcept(f.valid()));
  EXPECT_FALSE(f.valid());
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
// NOLINTNEXTLINE(google-readability-avoid-underscore-in-googletest-name)
TEST(FutureTestInt, conform_30_6_6_20) {
  // future<int>::wait() blocks until state is ready.
  promise<int> p;
  future<int> const f = p.get_future();

  // We use std::promise<> and std::future<> to test our promises and futures.
  // This test uses a number of promises to track progress in a separate thread,
  // and checks the expected conditions in each one.
  std::promise<void> thread_started;
  std::promise<void> f_wait_returned;

  ScopedThread t([&] {
    thread_started.set_value();
    f.wait();
    f_wait_returned.set_value();
  });

  thread_started.get_future().get();

  auto waiter = f_wait_returned.get_future();
  EXPECT_EQ(std::future_status::timeout, waiter.wait_for(2_ms));
  p.set_value(42);
  EXPECT_EQ(std::future_status::ready, waiter.wait_for(500_ms));
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
// NOLINTNEXTLINE(google-readability-avoid-underscore-in-googletest-name)
TEST(FutureTestInt, conform_30_6_6_21) {
  // future<int>::wait_for() blocks until state is ready.
  promise<int> p;
  future<int> const f = p.get_future();

  // We use std::promise<> and std::future<> to test our promises and futures.
  // This test uses a number of promises to track progress in a separate thread,
  // and checks the expected conditions in each one.
  std::promise<void> thread_started;
  std::promise<void> f_wait_returned;

  ScopedThread t([&] {
    thread_started.set_value();
    f.wait_for(500_ms);
    f_wait_returned.set_value();
  });

  thread_started.get_future().get();

  auto waiter = f_wait_returned.get_future();
  EXPECT_EQ(std::future_status::timeout, waiter.wait_for(2_ms));
  p.set_value(42);
  EXPECT_EQ(std::future_status::ready, waiter.wait_for(500_ms));
}

// Paragraph 30.6.6.22.1 refers to futures that hold a deferred function (like
// those returned by std::async()), we are not implement those, so there is no
// test needed.

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
// NOLINTNEXTLINE(google-readability-avoid-underscore-in-googletest-name)
TEST(FutureTestInt, conform_30_6_6_22_2) {
  // wait_for() returns std::future_status::ready if the future is ready.
  promise<int> p0;
  auto f0 = p0.get_future();

  p0.set_value(42);
  auto s = f0.wait_for(0_ms);
  ASSERT_EQ(std::future_status::ready, s);
  EXPECT_EQ(42, f0.get());
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
// NOLINTNEXTLINE(google-readability-avoid-underscore-in-googletest-name)
TEST(FutureTestInt, conform_30_6_6_22_3) {
  // wait_for() returns std::future_status::timeout if the future is not ready.
  promise<int> p0;
  auto f0 = p0.get_future();

  auto s = f0.wait_for(0_ms);
  EXPECT_EQ(std::future_status::timeout, s);
  ASSERT_NE(std::future_status::ready, f0.wait_for(0_ms));
}

// Paragraph 30.6.6.23 asserts that if the clock raises then wait_for() might
// raise too. We do not need to test for that, exceptions are always propagated,
// this is just giving implementors freedom.

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
// NOLINTNEXTLINE(google-readability-avoid-underscore-in-googletest-name)
TEST(FutureTestInt, conform_30_6_6_24) {
  // future<int>::wait_until() blocks until state is ready.
  promise<int> p;
  future<int> const f = p.get_future();

  // We use std::promise<> and std::future<> to test our promises and futures.
  // This test uses a number of promises to track progress in a separate thread,
  // and checks the expected conditions in each one.
  std::promise<void> thread_started;
  std::promise<void> f_wait_returned;

  ScopedThread t([&] {
    thread_started.set_value();
    f.wait_until(std::chrono::system_clock::now() + 500_ms);
    f_wait_returned.set_value();
  });

  thread_started.get_future().get();

  auto waiter = f_wait_returned.get_future();
  EXPECT_EQ(std::future_status::timeout, waiter.wait_for(2_ms));
  p.set_value(42);
  EXPECT_EQ(std::future_status::ready, waiter.wait_for(500_ms));
}

// Paragraph 30.6.6.25.1 refers to futures that hold a deferred function (like
// those returned by std::async()), we are not implement those, so there is no
// test needed.

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
// NOLINTNEXTLINE(google-readability-avoid-underscore-in-googletest-name)
TEST(FutureTestInt, conform_30_6_6_25_2) {
  // wait_until() returns std::future_status::ready if the future is ready.
  promise<int> p0;
  auto f0 = p0.get_future();

  p0.set_value(42);
  auto s = f0.wait_until(std::chrono::system_clock::now());
  ASSERT_EQ(std::future_status::ready, s);
  ASSERT_EQ(std::future_status::ready, f0.wait_for(0_ms));
  EXPECT_EQ(42, f0.get());
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
// NOLINTNEXTLINE(google-readability-avoid-underscore-in-googletest-name)
TEST(FutureTestInt, conform_30_6_6_25_3) {
  // wait_until() returns std::future_status::timeout if the future is not
  // ready.
  promise<int> p0;
  auto f0 = p0.get_future();

  auto s = f0.wait_until(std::chrono::system_clock::now());
  EXPECT_EQ(std::future_status::timeout, s);
  ASSERT_NE(std::future_status::ready, f0.wait_for(0_ms));
}

// Paragraph 30.6.6.26 asserts that if the clock raises then wait_until() might
// raise too. We do not need to test for that, exceptions are always propagated,
// this is just giving implementors freedom.

/// @test Verify the behavior around cancellation.
TEST(FutureTestInt, CancellationWithoutSatisfaction) {
  bool cancelled = false;
  promise<int> p0([&cancelled] { cancelled = true; });
  auto f0 = p0.get_future();
  ASSERT_TRUE(f0.cancel());
  ASSERT_TRUE(cancelled);
}

/// @test Verify the case for cancel then satisfy.
TEST(FutureTestInt, CancellationAndSatisfaction) {
  bool cancelled = false;
  promise<int> p0([&cancelled] { cancelled = true; });
  auto f0 = p0.get_future();
  ASSERT_TRUE(f0.cancel());
  p0.set_value(1);
  ASSERT_EQ(std::future_status::ready, f0.wait_for(0_ms));
  ASSERT_EQ(1, f0.get());
  ASSERT_TRUE(cancelled);
}

/// @test Verify the cancellation fails on satisfied promise.
TEST(FutureTestInt, CancellationAfterSatisfaction) {
  bool cancelled = false;
  promise<int> p0([&cancelled] { cancelled = true; });
  auto f0 = p0.get_future();
  p0.set_value(1);
  ASSERT_FALSE(f0.cancel());
  ASSERT_FALSE(cancelled);
  ASSERT_EQ(1, f0.get());
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
