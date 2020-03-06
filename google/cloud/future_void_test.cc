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

#include "google/cloud/future_void.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/expect_future_error.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace {
using ::testing::HasSubstr;
using namespace testing_util::chrono_literals;
using testing_util::ExpectFutureError;

// Verify we are testing the types we think we should be testing.
static_assert(std::is_same<future<void>, ::google::cloud::future<void>>::value,
              "std::future in global namespace");
static_assert(
    std::is_same<promise<void>, ::google::cloud::promise<void>>::value,
    "std::promise in global namespace");

/// @test Verify that destructing a promise does not introduce race conditions.
TEST(FutureTestVoid, DestroyInWaitingThread) {
  for (int i = 0; i != 1000; ++i) {
    std::thread t;
    {
      promise<void> p;
      t = std::thread([&p] { p.set_value(); });
      p.get_future().get();
    }
    if (t.joinable()) t.join();
  }
}

/// @test Verify that destructing a promise does not introduce race conditions.
TEST(FutureTestVoid, DestroyInSignalingThread) {
  for (int i = 0; i != 1000; ++i) {
    std::thread t;
    {
      promise<void> p;
      future<void> f = p.get_future();
      t = std::thread([](promise<void> p) { p.set_value(); }, std::move(p));
      f.get();
    }
    if (t.joinable()) t.join();
  }
}

/// @test Verify conformance with section 30.6.5 of the C++14 spec.
TEST(FutureTestVoid, conform_30_6_5_3) {
  // TODO(#1364) - allocators are not supported for now.
  static_assert(!std::uses_allocator<promise<void>, std::allocator<int>>::value,
                "promise<void> should use allocators if provided");
}

/// @test Verify conformance with section 30.6.5 of the C++14 spec.
TEST(FutureTestVoid, conform_30_6_5_4_default) {
  promise<void> p0;
  auto f0 = p0.get_future();
  p0.set_value();
  ASSERT_EQ(std::future_status::ready, f0.wait_for(0_ms));
  f0.get();
  SUCCEED();
}

/// @test Verify conformance with section 30.6.5 of the C++14 spec.
TEST(FutureTestVoid, conform_30_6_5_5) {
  // promise<R> move constructor clears the shared state on the moved-from
  // promise.
  promise<void> p0;

  promise<void> p1(std::move(p0));
  auto f1 = p1.get_future();
  p1.set_value();
  ASSERT_EQ(std::future_status::ready, f1.wait_for(0_ms));
  f1.get();

  ExpectFutureError([&] { p0.set_value(); }, std::future_errc::no_state);
}

/// @test Verify conformance with section 30.6.5 of the C++14 spec.
TEST(FutureTestVoid, conform_30_6_5_7) {
  // promise<R> destructor abandons the shared state, the associated future
  // becomes satisfied with an exception.
  future<void> f0;
  {
    promise<void> p0;
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
      "future<void>::get\\(\\) had an exception but exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify conformance with section 30.6.5 of the C++14 spec.
TEST(FutureTestVoid, conform_30_6_5_8) {
  // promise<R> move assignment clears the shared state in the moved-from
  // promise.
  promise<void> p0;

  promise<void> p1;
  p1 = std::move(p0);
  auto f1 = p1.get_future();
  p1.set_value();
  ASSERT_EQ(std::future_status::ready, f1.wait_for(0_ms));
  f1.get();
  ExpectFutureError([&] { p0.set_value(); }, std::future_errc::no_state);
}

/// @test Verify conformance with section 30.6.5 of the C++14 spec.
TEST(FutureTestVoid, conform_30_6_5_10) {
  // promise<R>::swap() actually swaps shared states.
  promise<void> p0;
  promise<void> p1;
  p0.set_value();
  p0.swap(p1);

  auto f0 = p0.get_future();
  auto f1 = p1.get_future();
  ASSERT_NE(std::future_status::ready, f0.wait_for(0_ms));
  ASSERT_EQ(std::future_status::ready, f1.wait_for(0_ms));
  f1.get();
  SUCCEED();
  p0.set_value();
  SUCCEED();
  ASSERT_EQ(std::future_status::ready, f0.wait_for(0_ms));
}

/// @test Verify conformance with section 30.6.5 of the C++14 spec.
TEST(FutureTestVoid, conform_30_6_5_14_1) {
  // promise<R>::get_future() raises if future was already retrieved.
  promise<void> p0;
  auto f0 = p0.get_future();
  ExpectFutureError([&] { p0.get_future(); },
                    std::future_errc::future_already_retrieved);
}

/// @test Verify conformance with section 30.6.5 of the C++14 spec.
TEST(FutureTestVoid, conform_30_6_5_14_2) {
  // promise<R>::get_future() raises if there is no shared state.
  promise<void> p0;
  promise<void> p1(std::move(p0));
  ExpectFutureError([&] { p0.set_value(); }, std::future_errc::no_state);
}

/// @test Verify conformance with section 30.6.5 of the C++14 spec.
TEST(FutureTestVoid, conform_30_6_5_15) {
  // promise<R>::set_value() stores the value in the shared state and makes it
  // ready.
  promise<void> p0;
  auto f0 = p0.get_future();
  ASSERT_NE(std::future_status::ready, f0.wait_for(0_ms));
  p0.set_value();
  ASSERT_EQ(std::future_status::ready, f0.wait_for(0_ms));
  f0.get();
  SUCCEED();
}

/// @test Verify conformance with section 30.6.5 of the C++14 spec.
TEST(FutureTestVoid, conform_30_6_5_16_1) {
  // promise<R>::set_value() raises if there is a value in the shared state.
  promise<void> p0;
  p0.set_value();
  ExpectFutureError([&] { p0.set_value(); },
                    std::future_errc::promise_already_satisfied);
}

/// @test Verify conformance with section 30.6.5 of the C++14 spec.
TEST(FutureTestVoid, conform_30_6_5_17_2) {
  // promise<R>::set_value() raises if there is no shared state.
  promise<void> p0;
  promise<void> p1(std::move(p0));
  ExpectFutureError([&] { p0.set_value(); }, std::future_errc::no_state);
}

/// @test Verify conformance with section 30.6.5 of the C++14 spec.
TEST(FutureTestVoid, conform_30_6_5_18) {
  // promise<R>::set_exception() sets an exception and makes the shared state
  // ready.
  promise<void> p0;
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
      "future<void>::get\\(\\) had an exception but exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify conformance with section 30.6.5 of the C++14 spec.
TEST(FutureTestVoid, conform_30_6_5_20_1_value) {
  // promise<R>::set_exception() raises if the shared state is already storing
  // a value.
  promise<void> p0;
  p0.set_value();
  ExpectFutureError(
      [&] {
        p0.set_exception(
            std::make_exception_ptr(std::runtime_error("testing")));
      },
      std::future_errc::promise_already_satisfied);
}

/// @test Verify conformance with section 30.6.5 of the C++14 spec.
TEST(FutureTestVoid, conform_30_6_5_20_1_exception) {
  // promise<R>::set_exception() raises if the shared state is already storing
  // an exception.
  promise<void> p0;
  p0.set_exception(std::make_exception_ptr(std::runtime_error("original ex")));
  ExpectFutureError(
      [&] {
        p0.set_exception(
            std::make_exception_ptr(std::runtime_error("testing")));
      },
      std::future_errc::promise_already_satisfied);
}

/// @test Verify conformance with section 30.6.5 of the C++14 spec.
TEST(FutureTestVoid, conform_30_6_5_20_2) {
  // promise<R>::set_exception() raises if the promise does not have a shared
  // state.
  promise<void> p0;
  promise<void> p1(std::move(p0));
  ExpectFutureError(
      [&] {
        p0.set_exception(
            std::make_exception_ptr(std::runtime_error("testing")));
      },
      std::future_errc::no_state);
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
TEST(FutureTestVoid, conform_30_6_6_3_a) {
  // Calling get() on a future with `valid() == false` is undefined, but the
  // implementation is encouraged to raise `future_error` with an error code
  // of `future_errc::no_state`
  future<void> f;
  EXPECT_FALSE(f.valid());
  ExpectFutureError([&] { f.get(); }, std::future_errc::no_state);
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
TEST(FutureTestVoid, conform_30_6_6_3_b) {
  // Calling wait() on a future with `valid() == false` is undefined, but the
  // implementation is encouraged to raise `future_error` with an error code
  // of `future_errc::no_state`
  future<void> f;
  EXPECT_FALSE(f.valid());
  ExpectFutureError([&] { f.wait(); }, std::future_errc::no_state);
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
TEST(FutureTestVoid, conform_30_6_6_3_c) {
  // Calling wait_for() on a future with `valid() == false` is undefined, but
  // the implementation is encouraged to raise `future_error` with an error code
  // of `future_errc::no_state`
  future<void> f;
  EXPECT_FALSE(f.valid());
  ExpectFutureError([&] { f.wait_for(3_ms); }, std::future_errc::no_state);
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
TEST(FutureTestVoid, conform_30_6_6_3_d) {
  // Calling wait_until() on a future with `valid() == false` is undefined, but
  // the implementation is encouraged to raise `future_error` with an error code
  // of `future_errc::no_state`
  future<void> f;
  EXPECT_FALSE(f.valid());
  ExpectFutureError(
      [&] { f.wait_until(std::chrono::system_clock::now() + 3_ms); },
      std::future_errc::no_state);
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
TEST(FutureTestVoid, conform_30_6_6_5) {
  // future<void>::future() constructs an empty future with no shared state.
  future<void> f;
  EXPECT_FALSE(f.valid());
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
TEST(FutureTestVoid, conform_30_6_6_7) {
  // future<void> move constructor is `noexcept`.
  future<void> f0;
  EXPECT_TRUE(noexcept(future<void>(std::move(f0))));
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
TEST(FutureTestVoid, conform_30_6_6_8_a) {
  // future<void> move constructor transfers futures with valid state.
  promise<void> p;
  future<void> f0 = p.get_future();
  EXPECT_TRUE(f0.valid());

  future<void> f1(std::move(f0));
  EXPECT_FALSE(f0.valid());
  EXPECT_TRUE(f1.valid());
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
TEST(FutureTestVoid, conform_30_6_6_8_b) {
  // future<void> move constructor transfers futures with no state.
  future<void> f0;
  EXPECT_FALSE(f0.valid());

  future<void> f1(std::move(f0));
  EXPECT_FALSE(f0.valid());
  EXPECT_FALSE(f1.valid());
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
TEST(FutureTestVoid, conform_30_6_6_9) {
  // future<void> destructor releases the shared state.
  promise<void> p;
  future<void> f0 = p.get_future();
  EXPECT_TRUE(f0.valid());
  // This behavior is not observable, but any violation should be detected by
  // the ASAN builds.
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
TEST(FutureTestVoid, conform_30_6_6_10) {
  // Move assignment is noexcept.
  promise<void> p;
  future<void> f0 = p.get_future();
  EXPECT_TRUE(f0.valid());

  future<void> f1;
  EXPECT_TRUE(noexcept(f1 = std::move(f0)));
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
TEST(FutureTestVoid, conform_30_6_6_11_a) {
  // future<void> move assignment transfers futures with valid state.
  promise<void> p;
  future<void> f0 = p.get_future();
  EXPECT_TRUE(f0.valid());

  future<void> f1;
  f1 = std::move(f0);
  EXPECT_FALSE(f0.valid());
  EXPECT_TRUE(f1.valid());
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
TEST(FutureTestVoid, conform_30_6_6_11_b) {
  // future<void> move assignment transfers futures with invalid state.
  future<void> f0;
  EXPECT_FALSE(f0.valid());

  future<void> f1;
  f1 = std::move(f0);
  EXPECT_FALSE(f0.valid());
  EXPECT_FALSE(f1.valid());
}

// Paragraphs 30.6.6.{12,13,14} are about shared_futures which we are
// not implementing yet.

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
TEST(FutureTestVoid, conform_30_6_6_15) {
  // future<void>::get() only returns once the promise is satisfied.
  promise<void> p;

  // We use std::promise<> and std::future<> to test our promises and futures.
  // This test uses a number of promises to track progress in a separate thread,
  // and checks the expected conditions in each one.
  std::promise<void> p_get_future_called;
  std::promise<void> f_get_called;

  std::thread t([&] {
    future<void> f = p.get_future();
    p_get_future_called.set_value();
    f.get();
    f_get_called.set_value();
  });

  p_get_future_called.get_future().get();
  auto waiter = f_get_called.get_future();
  // thread `t` cannot make progress until we set the promise value.
  EXPECT_EQ(std::future_status::timeout, waiter.wait_for(2_ms));

  p.set_value();
  // now thread `t` can make progress.
  EXPECT_EQ(std::future_status::ready, waiter.wait_for(500_ms));

  waiter.get();
  t.join();
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
TEST(FutureTestVoid, conform_30_6_6_16_3) {
  // future<void>::get() returns void.
  promise<void> p;
  future<void> f = p.get_future();
  EXPECT_TRUE((std::is_same<decltype(f.get()), void>::value));
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
TEST(FutureTestVoid, conform_30_6_6_17) {
  // future<void>::get() throws if an exception was set in the promise.
  promise<void> p;

  future<void> f = p.get_future();
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
      "future<void>::get\\(\\) had an exception but exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
TEST(FutureTestVoid, conform_30_6_6_18_a) {
  // future<void>::get() releases the shared state.
  promise<void> p;
  future<void> f = p.get_future();
  p.set_value();
  f.get();
  EXPECT_FALSE(f.valid());
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
TEST(FutureTestVoid, conform_30_6_6_18_b) {
  // future<void>::get() throws releases the shared state.
  promise<void> p;
  future<void> f = p.get_future();
  p.set_exception(std::make_exception_ptr(std::runtime_error("unused")));
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(f.get(), std::runtime_error);
  EXPECT_FALSE(f.valid());
#else
  EXPECT_DEATH_IF_SUPPORTED(
      f.get(),
      "future<void>::get\\(\\) had an exception but exceptions are disabled");
  // Cannot evaluate side effects with EXPECT_DEATH*()
  //     EXPECT_FALSE(f.valid());
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
TEST(FutureTestVoid, conform_30_6_6_19_a) {
  // future<void>::valid() returns true when the future has a shared state.
  promise<void> p;
  future<void> const f = p.get_future();
  EXPECT_TRUE(noexcept(f.valid()));
  EXPECT_TRUE(f.valid());
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
TEST(FutureTestVoid, conform_30_6_6_19_b) {
  // future<void>::valid() returns false when the future has no shared state.
  future<void> const f;
  EXPECT_TRUE(noexcept(f.valid()));
  EXPECT_FALSE(f.valid());
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
TEST(FutureTestVoid, conform_30_6_6_20) {
  // future<void>::wait() blocks until state is ready.
  promise<void> p;
  future<void> const f = p.get_future();

  // We use std::promise<> and std::future<> to test our promises and futures.
  // This test uses a number of promises to track progress in a separate thread,
  // and checks the expected conditions in each one.
  std::promise<void> thread_started;
  std::promise<void> f_wait_returned;

  std::thread t([&] {
    thread_started.set_value();
    f.wait();
    f_wait_returned.set_value();
  });

  thread_started.get_future().get();

  auto waiter = f_wait_returned.get_future();
  EXPECT_EQ(std::future_status::timeout, waiter.wait_for(2_ms));
  p.set_value();
  EXPECT_EQ(std::future_status::ready, waiter.wait_for(500_ms));

  t.join();
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
TEST(FutureTestVoid, conform_30_6_6_21) {
  // future<void>::wait_for() blocks until state is ready.
  promise<void> p;
  future<void> const f = p.get_future();

  // We use std::promise<> and std::future<> to test our promises and futures.
  // This test uses a number of promises to track progress in a separate thread,
  // and checks the expected conditions in each one.
  std::promise<void> thread_started;
  std::promise<void> f_wait_returned;

  std::thread t([&] {
    thread_started.set_value();
    f.wait_for(500_ms);
    f_wait_returned.set_value();
  });

  thread_started.get_future().get();

  auto waiter = f_wait_returned.get_future();
  EXPECT_EQ(std::future_status::timeout, waiter.wait_for(2_ms));
  p.set_value();
  EXPECT_EQ(std::future_status::ready, waiter.wait_for(500_ms));

  t.join();
}

// Paragraph 30.6.6.22.1 refers to futures that hold a deferred function (like
// those returned by std::async()), we are not implement those, so there is no
// test needed.

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
TEST(FutureTestVoid, conform_30_6_6_22_2) {
  // wait_for() returns std::future_status::ready if the future is ready.
  promise<void> p0;
  auto f0 = p0.get_future();

  p0.set_value();
  auto s = f0.wait_for(0_ms);
  EXPECT_EQ(std::future_status::ready, s);
  f0.get();
  SUCCEED();
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
TEST(FutureTestVoid, conform_30_6_6_22_3) {
  // wait_for() returns std::future_status::timeout if the future is not ready.
  promise<void> p0;
  auto f0 = p0.get_future();

  auto s = f0.wait_for(0_ms);
  EXPECT_EQ(std::future_status::timeout, s);
  ASSERT_NE(std::future_status::ready, f0.wait_for(0_ms));
}

// Paragraph 30.6.6.23 asserts that if the clock raises then wait_for() might
// raise too. We do not need to test for that, exceptions are always propagated,
// this is just giving implementors freedom.

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
TEST(FutureTestVoid, conform_30_6_6_24) {
  // future<void>::wait_until() blocks until state is ready.
  promise<void> p;
  future<void> const f = p.get_future();

  // We use std::promise<> and std::future<> to test our promises and futures.
  // This test uses a number of promises to track progress in a separate thread,
  // and checks the expected conditions in each one.
  std::promise<void> thread_started;
  std::promise<void> f_wait_returned;

  std::thread t([&] {
    thread_started.set_value();
    f.wait_until(std::chrono::system_clock::now() + 500_ms);
    f_wait_returned.set_value();
  });

  thread_started.get_future().get();

  auto waiter = f_wait_returned.get_future();
  EXPECT_EQ(std::future_status::timeout, waiter.wait_for(2_ms));
  p.set_value();
  EXPECT_EQ(std::future_status::ready, waiter.wait_for(500_ms));

  t.join();
}

// Paragraph 30.6.6.25.1 refers to futures that hold a deferred function (like
// those returned by std::async()), we are not implement those, so there is no
// test needed.

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
TEST(FutureTestVoid, conform_30_6_6_25_2) {
  // wait_until() returns std::future_status::ready if the future is ready.
  promise<void> p0;
  auto f0 = p0.get_future();

  p0.set_value();
  auto s = f0.wait_until(std::chrono::system_clock::now());
  EXPECT_EQ(std::future_status::ready, s);
  ASSERT_EQ(std::future_status::ready, f0.wait_for(0_ms));
  f0.get();
  SUCCEED();
}

/// @test Verify conformance with section 30.6.6 of the C++14 spec.
TEST(FutureTestVoid, conform_30_6_6_25_3) {
  // wait_until() returns std::future_status::timeout if the future is not
  // ready.
  promise<void> p0;
  auto f0 = p0.get_future();

  auto s = f0.wait_until(std::chrono::system_clock::now());
  EXPECT_EQ(std::future_status::timeout, s);
  ASSERT_NE(std::future_status::ready, f0.wait_for(0_ms));
}

// Paragraph 30.6.6.26 asserts that if the clock raises then wait_until() might
// raise too. We do not need to test for that, exceptions are always propagated,
// this is just giving implementors freedom.

/// @test Verify the behavior around cancellation.
TEST(FutureTestVoid, cancellation_without_satisfaction) {
  bool cancelled = false;
  promise<void> p0([&cancelled] { cancelled = true; });
  auto f0 = p0.get_future();
  ASSERT_TRUE(f0.cancel());
  ASSERT_TRUE(cancelled);
}

/// @test Verify the case for cancel then satisfy.
TEST(FutureTestVoid, cancellation_and_satisfaction) {
  bool cancelled = false;
  promise<void> p0([&cancelled] { cancelled = true; });
  auto f0 = p0.get_future();
  ASSERT_TRUE(f0.cancel());
  p0.set_value();
  ASSERT_EQ(std::future_status::ready, f0.wait_for(0_ms));
  ASSERT_TRUE(cancelled);
}

/// @test Verify the cancellation fails on satisfied promise.
TEST(FutureTestVoid, cancellation_after_satisfaction) {
  bool cancelled = false;
  promise<void> p0([&cancelled] { cancelled = true; });
  auto f0 = p0.get_future();
  p0.set_value();
  ASSERT_FALSE(f0.cancel());
  ASSERT_FALSE(cancelled);
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
