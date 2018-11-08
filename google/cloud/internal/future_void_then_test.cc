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

#include "google/cloud/future.h"
#include "google/cloud/internal/throw_delegate.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/expect_future_error.h"
#include <gmock/gmock.h>
#include <functional>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace {
using ::testing::HasSubstr;
using namespace testing_util::chrono_literals;
using testing_util::ExpectFutureError;

TEST(FutureTestVoid, ThenSimple) {
  promise<void> p;
  future<void> fut = p.get_future();
  EXPECT_TRUE(fut.valid());

  bool called = false;
  future<void> next = fut.then([&called](future<void> r) { called = true; });
  EXPECT_FALSE(fut.valid());
  EXPECT_TRUE(next.valid());
  EXPECT_FALSE(called);

  p.set_value();
  EXPECT_TRUE(called);
  EXPECT_TRUE(next.valid());
  EXPECT_EQ(std::future_status::ready, next.wait_for(0_ms));

  next.get();
  SUCCEED();
  EXPECT_FALSE(next.valid());
}

TEST(FutureTestVoid, ThenException) {
  promise<void> p;
  future<void> fut = p.get_future();
  EXPECT_TRUE(fut.valid());

  bool called = false;
  future<void> next = fut.then([&called](future<void> r) {
    called = true;
    internal::RaiseRuntimeError("test message");
  });
  EXPECT_FALSE(fut.valid());
  EXPECT_TRUE(next.valid());
  EXPECT_FALSE(called);

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  p.set_value();
  EXPECT_TRUE(called);
  EXPECT_TRUE(next.valid());
  EXPECT_EQ(std::future_status::ready, next.wait_for(0_ms));

  EXPECT_THROW(
      try { next.get(); } catch (std::runtime_error const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("test message"));
        throw;
      },
      std::runtime_error);
  EXPECT_FALSE(next.valid());
#else
  EXPECT_DEATH_IF_SUPPORTED(p.set_value(), "test message");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(FutureTestVoid, ThenUnwrap) {
  promise<void> p;
  future<void> fut = p.get_future();
  EXPECT_TRUE(fut.valid());

  promise<std::string> pp;
  bool called = false;
  auto cont = [&pp, &called](future<void> r) {
    called = true;
    return pp.get_future();
  };
  future<std::string> next = fut.then(std::move(cont));
  EXPECT_FALSE(fut.valid());
  EXPECT_TRUE(next.valid());
  EXPECT_FALSE(next.is_ready());

  p.set_value();
  EXPECT_TRUE(called);
  EXPECT_FALSE(next.is_ready());

  pp.set_value("value=42");
  EXPECT_TRUE(next.is_ready());
  EXPECT_EQ("value=42", next.get());
  EXPECT_FALSE(next.valid());
}

// The following tests reference the technical specification:
//   http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/p0159r0.html
// The test names match the section and paragraph from the TS.

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestVoid, conform_2_3_2_a) {
  // future<void> should have an unwrapping constructor.
  promise<future<void>> p;
  future<future<void>> f = p.get_future();

  future<void> unwrapped(std::move(f));
  EXPECT_FALSE(noexcept(future<void>(p.get_future())));
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestVoid, conform_2_3_3_a) {
  // A future<void> created via the unwrapping constructor becomes satisfied
  // when both become satisfied.
  promise<future<void>> p;

  future<void> unwrapped(p.get_future());
  EXPECT_TRUE(unwrapped.valid());
  EXPECT_FALSE(unwrapped.is_ready());

  promise<void> p2;
  p.set_value(p2.get_future());
  EXPECT_FALSE(unwrapped.is_ready());

  p2.set_value();
  EXPECT_TRUE(unwrapped.is_ready());
  unwrapped.get();
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestVoid, conform_2_3_3_b) {
  // A future<void> created via the unwrapping constructor becomes satisfied
  // when the wrapped future is satisfied by an exception.
  promise<future<void>> p;

  future<void> unwrapped(p.get_future());
  EXPECT_TRUE(unwrapped.valid());
  EXPECT_FALSE(unwrapped.is_ready());

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  p.set_exception(std::make_exception_ptr(std::runtime_error("test message")));
  EXPECT_TRUE(unwrapped.is_ready());
  EXPECT_THROW(
      try { unwrapped.get(); } catch (std::runtime_error const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("test message"));
        throw;
      },
      std::runtime_error);
#else
  EXPECT_DEATH_IF_SUPPORTED(
      p.set_exception(
          std::make_exception_ptr(std::runtime_error("test message"))),
      "future<T>::get\\(\\) had an exception but exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestVoid, conform_2_3_3_c) {
  // A future<void> created via the unwrapping constructor becomes satisfied
  // when the inner future is satisfied by an exception.
  promise<future<void>> p;

  future<void> unwrapped(p.get_future());
  EXPECT_TRUE(unwrapped.valid());
  EXPECT_FALSE(unwrapped.is_ready());

  promise<void> p2;
  p.set_value(p2.get_future());
  EXPECT_FALSE(unwrapped.is_ready());

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  p2.set_exception(std::make_exception_ptr(std::runtime_error("test message")));
  EXPECT_TRUE(unwrapped.is_ready());
  EXPECT_THROW(
      try { unwrapped.get(); } catch (std::runtime_error const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("test message"));
        throw;
      },
      std::runtime_error);
#else
  std::string expected = "future_error\\[";
  expected += std::make_error_code(std::future_errc::promise_already_satisfied)
                  .message();
  expected += "\\]";
  EXPECT_DEATH_IF_SUPPORTED(p.set_exception(std::make_exception_ptr(
                                std::runtime_error("test message"))),
                            expected);
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestVoid, conform_2_3_3_d) {
  // A future<void> created via the unwrapping constructor becomes satisfied
  // when the inner future is invalid.
  promise<future<void>> p;

  future<void> unwrapped(p.get_future());
  EXPECT_TRUE(unwrapped.valid());
  EXPECT_FALSE(unwrapped.is_ready());

  promise<void> p2;
  p.set_value(future<void>{});
  EXPECT_TRUE(unwrapped.is_ready());

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      try { unwrapped.get(); } catch (std::future_error const& ex) {
        EXPECT_EQ(std::future_errc::broken_promise, ex.code());
        throw;
      },
      std::future_error);
#else
  EXPECT_DEATH_IF_SUPPORTED(
      unwrapped.get(),
      "future<void>::get\\(\\) had an exception but exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestVoid, conform_2_3_4) {
  // future<void> should leaves the source invalid.
  promise<future<void>> p;
  future<future<void>> f = p.get_future();

  future<void> unwrapped(std::move(f));
  EXPECT_TRUE(unwrapped.valid());
  EXPECT_FALSE(f.valid());
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestVoid, conform_2_3_5) {
  // future<void>::then() is a template member function that takes callables.
  future<void> f;
  auto callable = [](future<void> f) -> void { f.get(); };
  EXPECT_TRUE((std::is_same<future<void>, decltype(f.then(callable))>::value));

  auto c2 = [](future<void> f) -> int {
    f.get();
    return 42;
  };
  EXPECT_TRUE((std::is_same<future<int>, decltype(f.then(c2))>::value));
  auto c3 = [](future<void> f) -> std::string {
    f.get();
    return "";
  };
  EXPECT_TRUE((std::is_same<future<std::string>, decltype(f.then(c3))>::value));
}

// Use SFINAE to test if future<void>::then() accepts a T parameter.
template <typename T>
auto test_then(int)
    -> decltype(std::declval<future<void>>().then(T()), std::true_type{}) {
  return std::true_type{};
}

template <typename T>
auto test_then(long) -> std::false_type {
  return std::false_type{};
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestVoid, conform_2_3_7) {
  // future<void>::then() requires callables that take future<void> as a
  // parameter.
  future<void> f;
  using valid_callable_type = std::function<void(future<void>)>;
  using invalid_callable_type = std::function<void(int)>;

  EXPECT_TRUE(decltype(test_then<valid_callable_type>(0))::value);
  EXPECT_FALSE(decltype(test_then<invalid_callable_type>(0))::value);

  EXPECT_TRUE(decltype(test_then<std::function<int(future<void>)>>(0))::value);
  EXPECT_FALSE(decltype(test_then<std::function<int()>>(0))::value);
  EXPECT_TRUE(
      decltype(test_then<std::function<std::string(future<void>)>>(0))::value);
  EXPECT_FALSE(decltype(test_then<std::function<std::string()>>(0))::value);
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestVoid, conform_2_3_8_a) {
  // future<void>::then() creates a future with a valid shared state.
  promise<void> p;
  future<void> f = p.get_future();

  future<void> next = f.then([&](future<void> r) {});
  EXPECT_TRUE(next.valid());
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestVoid, conform_2_3_8_b) {
  // future<void>::then() calls the functor when the future becomes ready.
  promise<void> p;
  future<void> f = p.get_future();

  bool called = false;
  future<void> next = f.then([&](future<void> r) { called = true; });
  EXPECT_TRUE(next.valid());
  EXPECT_FALSE(called);

  p.set_value();
  EXPECT_TRUE(called);
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestVoid, conform_2_3_8_c) {
  // future<void>::then() calls the functor if the future was ready.
  promise<void> p;
  future<void> f = p.get_future();

  p.set_value();
  bool called = false;
  future<void> next = f.then([&](future<void> r) { called = true; });
  EXPECT_TRUE(next.valid());
  EXPECT_TRUE(called);
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestVoid, conform_2_3_8_d) {
  // future<void>::then() propagates the value from the functor to the returned
  // future.
  promise<void> p;
  future<void> f = p.get_future();

  future<int> next = f.then([&](future<void> r) -> int { return 42; });
  EXPECT_TRUE(next.valid());
  p.set_value();
  EXPECT_EQ(std::future_status::ready, next.wait_for(0_ms));
  EXPECT_EQ(42, next.get());
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestVoid, conform_2_3_8_e) {
  // future<void>::then() propagates exceptions raised by the functor to the
  // returned future.
  promise<void> p;
  future<void> f = p.get_future();

  future<void> next = f.then([&](future<void> r) {
    internal::RaiseRuntimeError("test exception in functor");
  });
  EXPECT_TRUE(next.valid());

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  p.set_value();
  EXPECT_EQ(std::future_status::ready, next.wait_for(0_ms));
  EXPECT_THROW(
      try { next.get(); } catch (std::runtime_error const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("test exception in functor"));
        throw;
      },
      std::runtime_error);
  EXPECT_FALSE(next.valid());
#else
  EXPECT_DEATH_IF_SUPPORTED(p.set_value(), "test exception in functor");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestVoid, conform_2_3_9_a) {
  // future<void>::then() returns a functor containing the type of the value
  // returned by the functor.
  promise<void> p;
  future<void> f = p.get_future();

  auto returns_void = [](future<void>) -> void {};
  EXPECT_TRUE(
      (std::is_same<future<void>, decltype(f.then(returns_void))>::value));

  auto returns_int = [](future<void>) -> int { return 42; };
  EXPECT_TRUE(
      (std::is_same<future<int>, decltype(f.then(returns_int))>::value));

  auto returns_string = [](future<void>) -> std::string { return "42"; };
  EXPECT_TRUE((std::is_same<future<std::string>,
                            decltype(f.then(returns_string))>::value));
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestVoid, conform_2_3_9_b) {
  // future<void>::then() implicitly unwraps the future type when a functor
  // returns a future<>.
  promise<void> p;
  future<void> f = p.get_future();

  auto returns_void = [](future<void>) -> future<void> {
    return promise<void>().get_future();
  };
  EXPECT_TRUE(
      (std::is_same<future<void>, decltype(f.then(returns_void))>::value));

  auto returns_int = [](future<void>) -> future<int> {
    return promise<int>().get_future();
  };
  EXPECT_TRUE(
      (std::is_same<future<int>, decltype(f.then(returns_int))>::value));

  // The spec says the returned type must be future<R2> *exactly*, references do
  // not count.
  promise<int> p_int;
  future<int> f_int = p_int.get_future();
  auto returns_int_ref = [&f_int](future<void>) -> future<int>& {
    return f_int;
  };
  EXPECT_FALSE(
      (std::is_same<future<int>, decltype(f.then(returns_int_ref))>::value));
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestVoid, conform_2_3_9_c) {
  // future<void>::then() implicitly unwrapping captures the returned value.
  promise<void> p;
  future<void> f = p.get_future();

  promise<int> p2;
  bool called = false;
  future<int> r = f.then([&p2, &called](future<void> f) {
    called = true;
    f.get();
    return p2.get_future();
  });
  EXPECT_TRUE(r.valid());
  EXPECT_FALSE(r.is_ready());
  EXPECT_FALSE(f.valid());

  p.set_value();
  EXPECT_TRUE(called);
  EXPECT_FALSE(r.is_ready());

  p2.set_value(42);
  EXPECT_TRUE(r.is_ready());
  EXPECT_EQ(42, r.get());
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestVoid, conform_2_3_9_d) {
  // future<void>::then() implicitly unwrapping captures exceptions.
  promise<void> p;
  future<void> f = p.get_future();

  promise<int> p2;
  bool called = false;
  future<int> r = f.then([&p2, &called](future<void> f) {
    called = true;
    f.get();
    return p2.get_future();
  });
  EXPECT_TRUE(r.valid());
  EXPECT_FALSE(r.is_ready());
  EXPECT_FALSE(f.valid());

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  p.set_exception(std::make_exception_ptr(std::runtime_error("test message")));
  EXPECT_TRUE(called);
  EXPECT_TRUE(r.is_ready());

  EXPECT_THROW(
      try { r.get(); } catch (std::runtime_error const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("test message"));
        throw;
      },
      std::runtime_error);
#else
  // With exceptions disabled the program terminates as soon as the exception is
  // set.
  EXPECT_DEATH_IF_SUPPORTED(
      p.set_exception(
          std::make_exception_ptr(std::runtime_error("test message"))),
      "future<void>::get\\(\\) had an exception but exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestVoid, conform_2_3_9_e) {
  // future<void>::then() implicitly unwrapping raises on invalid future
  // returned by continuation.
  promise<void> p;
  future<void> f = p.get_future();

  bool called = false;
  future<int> r = f.then([&called](future<void> f) {
    called = true;
    f.get();
    return future<int>{};
  });
  EXPECT_TRUE(r.valid());
  EXPECT_FALSE(r.is_ready());
  EXPECT_FALSE(f.valid());

  p.set_value();
  EXPECT_TRUE(called);
  EXPECT_TRUE(r.is_ready());
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      try { r.get(); } catch (std::future_error const& ex) {
        EXPECT_EQ(std::future_errc::broken_promise, ex.code());
        throw;
      },
      std::future_error);
#else
  // With exceptions disabled setting the value immediately terminates.
  EXPECT_DEATH_IF_SUPPORTED(
      r.get(),
      "future<T>::get\\(\\) had an exception but exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestVoid, conform_2_3_10) {
  // future<void>::then() invalidates the source future.
  promise<void> p;
  future<void> f = p.get_future();
  future<void> r = f.then([](future<void> f) { f.get(); });
  EXPECT_TRUE(r.valid());
  EXPECT_FALSE(r.is_ready());
  EXPECT_FALSE(f.valid());

  p.set_value();
  EXPECT_TRUE(r.is_ready());
  r.get();
  SUCCEED();
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestVoid, conform_2_3_11_a) {
  // future<void>::is_ready() returns false for futures that are not ready.
  promise<void> p;
  future<void> const f = p.get_future();
  EXPECT_FALSE(f.is_ready());
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestVoid, conform_2_3_11_b) {
  // future<void>::is_ready() returns true for futures that are ready.
  promise<void> p;
  future<void> const f = p.get_future();
  p.set_value();
  EXPECT_TRUE(f.is_ready());
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestVoid, conform_2_3_11_c) {
  // future<void>::is_ready() raises for futures that are not valid.
  future<void> const f;
  ExpectFutureError([&] { f.is_ready(); }, std::future_errc::no_state);
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
