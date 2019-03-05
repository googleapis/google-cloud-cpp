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

TEST(FutureTestInt, ThenSimple) {
  promise<int> p;
  future<int> fut = p.get_future();
  EXPECT_TRUE(fut.valid());

  bool called = false;
  future<int> next = fut.then([&called](future<int> r) {
    called = true;
    return 2 * r.get();
  });
  EXPECT_FALSE(fut.valid());
  EXPECT_TRUE(next.valid());
  EXPECT_FALSE(called);

  p.set_value(42);
  EXPECT_TRUE(called);
  EXPECT_TRUE(next.valid());
  EXPECT_EQ(std::future_status::ready, next.wait_for(0_ms));

  EXPECT_EQ(84, next.get());
  EXPECT_FALSE(next.valid());
}

TEST(FutureTestInt, ThenException) {
  promise<int> p;
  future<int> fut = p.get_future();
  EXPECT_TRUE(fut.valid());

  bool called = false;
  future<int> next = fut.then([&called](future<int> r) {
    called = true;
    int value = r.get();
    if (value == 42) {
      internal::ThrowRuntimeError("test message");
    }
    return 2 * value;
  });
  EXPECT_FALSE(fut.valid());
  EXPECT_TRUE(next.valid());
  EXPECT_FALSE(called);

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  p.set_value(42);
  EXPECT_TRUE(called);
  EXPECT_TRUE(next.valid());
  EXPECT_EQ(std::future_status::ready, next.wait_for(0_ms));

  EXPECT_THROW(try { next.get(); } catch (std::runtime_error const& ex) {
    EXPECT_THAT(ex.what(), HasSubstr("test message"));
    throw;
  },
               std::runtime_error);
  EXPECT_FALSE(next.valid());
#else
  EXPECT_DEATH_IF_SUPPORTED(p.set_value(42), "test message");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(FutureTestInt, ThenUnwrap) {
  promise<int> p;
  future<int> fut = p.get_future();
  EXPECT_TRUE(fut.valid());

  promise<std::string> pp;
  bool called = false;
  auto cont = [&pp, &called](future<int> r) {
    called = true;
    return pp.get_future();
  };
  future<std::string> next = fut.then(std::move(cont));
  EXPECT_FALSE(fut.valid());
  EXPECT_TRUE(next.valid());
  EXPECT_FALSE(next.is_ready());

  p.set_value(42);
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
TEST(FutureTestInt, conform_2_3_2_a) {
  // future<T> should have an unwrapping constructor.
  promise<future<int>> p;
  future<future<int>> f = p.get_future();

  future<int> unwrapped(std::move(f));
  EXPECT_FALSE(noexcept(future<int>(p.get_future())));
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestInt, conform_2_3_3_a) {
  // A future<T> created via the unwrapping constructor becomes satisfied
  // when both become satisfied.
  promise<future<int>> p;

  future<int> unwrapped(p.get_future());
  EXPECT_TRUE(unwrapped.valid());
  EXPECT_FALSE(unwrapped.is_ready());

  promise<int> p2;
  p.set_value(p2.get_future());
  EXPECT_FALSE(unwrapped.is_ready());

  p2.set_value(42);
  EXPECT_TRUE(unwrapped.is_ready());
  EXPECT_EQ(42, unwrapped.get());
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestInt, conform_2_3_3_b) {
  // A future<T> created via the unwrapping constructor becomes satisfied
  // when the wrapped future is satisfied by an exception.
  promise<future<int>> p;

  future<int> unwrapped(p.get_future());
  EXPECT_TRUE(unwrapped.valid());
  EXPECT_FALSE(unwrapped.is_ready());

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  p.set_exception(std::make_exception_ptr(std::runtime_error("test message")));
  EXPECT_TRUE(unwrapped.is_ready());
  EXPECT_THROW(try { unwrapped.get(); } catch (std::runtime_error const& ex) {
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
TEST(FutureTestInt, conform_2_3_3_c) {
  // A future<T> created via the unwrapping constructor becomes satisfied
  // when the inner future is satisfied by an exception.
  promise<future<int>> p;

  future<int> unwrapped(p.get_future());
  EXPECT_TRUE(unwrapped.valid());
  EXPECT_FALSE(unwrapped.is_ready());

  promise<int> p2;
  p.set_value(p2.get_future());
  EXPECT_FALSE(unwrapped.is_ready());

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  p2.set_exception(std::make_exception_ptr(std::runtime_error("test message")));
  EXPECT_TRUE(unwrapped.is_ready());
  EXPECT_THROW(try { unwrapped.get(); } catch (std::runtime_error const& ex) {
    EXPECT_THAT(ex.what(), HasSubstr("test message"));
    throw;
  },
               std::runtime_error);
#else
  EXPECT_DEATH_IF_SUPPORTED(
      p2.set_exception(
          std::make_exception_ptr(std::runtime_error("test message"))),
      "future<T>::get\\(\\) had an exception but exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestInt, conform_2_3_3_d) {
  // A future<T> created via the unwrapping constructor becomes satisfied
  // when the inner future is invalid.
  promise<future<int>> p;

  future<int> unwrapped(p.get_future());
  EXPECT_TRUE(unwrapped.valid());
  EXPECT_FALSE(unwrapped.is_ready());

  promise<int> p2;
  p.set_value(future<int>{});
  EXPECT_TRUE(unwrapped.is_ready());
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(try { unwrapped.get(); } catch (std::future_error const& ex) {
    EXPECT_EQ(std::future_errc::broken_promise, ex.code());
    throw;
  },
               std::future_error);
#else
  EXPECT_DEATH_IF_SUPPORTED(
      unwrapped.get(),
      "future<T>::get\\(\\) had an exception but exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestInt, conform_2_3_4) {
  // future<T> should leaves the source invalid.
  promise<future<int>> p;
  future<future<int>> f = p.get_future();

  future<int> unwrapped(std::move(f));
  EXPECT_TRUE(unwrapped.valid());
  EXPECT_FALSE(f.valid());
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestInt, conform_2_3_5) {
  // future<int>::then() is a template member function that takes callables.
  future<int> f;
  auto callable = [](future<int> f) -> int { return 2 * f.get(); };
  EXPECT_TRUE((std::is_same<future<int>, decltype(f.then(callable))>::value));

  auto void_callable = [](future<int> f) { f.get(); };
  EXPECT_TRUE(
      (std::is_same<future<void>, decltype(f.then(void_callable))>::value));

  auto long_callable = [](future<int> f) -> long { return 2 * f.get(); };
  EXPECT_TRUE(
      (std::is_same<future<long>, decltype(f.then(long_callable))>::value));

  auto string_callable = [](future<int> f) -> std::string { return "42"; };
  EXPECT_TRUE((std::is_same<future<std::string>,
                            decltype(f.then(string_callable))>::value));
}

// Use SFINAE to test if future<int>::then() accepts a T parameter.
template <typename T>
auto test_then(int)
    -> decltype(std::declval<future<int>>().then(T()), std::true_type{}) {
  return std::true_type{};
}

template <typename T>
auto test_then(long) -> std::false_type {
  return std::false_type{};
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestInt, conform_2_3_7) {
  // future<int>::then() requires callables that take future<int> as a
  // parameter.
  future<int> f;
  using valid_callable_type = std::function<void(future<int>)>;
  using invalid_callable_type = std::function<void(int)>;

  EXPECT_TRUE(decltype(test_then<valid_callable_type>(0))::value);
  EXPECT_FALSE(decltype(test_then<invalid_callable_type>(0))::value);

  EXPECT_TRUE(decltype(test_then<std::function<int(future<int>)>>(0))::value);
  EXPECT_FALSE(decltype(test_then<std::function<int()>>(0))::value);
  EXPECT_TRUE(
      decltype(test_then<std::function<std::string(future<int>)>>(0))::value);
  EXPECT_FALSE(decltype(test_then<std::function<std::string()>>(0))::value);
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestInt, conform_2_3_8_a) {
  // future<int>::then() creates a future with a valid shared state.
  promise<int> p;
  future<int> f = p.get_future();

  future<void> next = f.then([&](future<int> r) {});
  EXPECT_TRUE(next.valid());
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestInt, conform_2_3_8_b) {
  // future<int>::then() calls the functor when the future becomes ready.
  promise<int> p;
  future<int> f = p.get_future();

  bool called = false;
  future<void> next = f.then([&](future<int> r) { called = true; });
  EXPECT_TRUE(next.valid());
  EXPECT_FALSE(called);

  p.set_value(42);
  EXPECT_TRUE(called);
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestInt, conform_2_3_8_c) {
  // future<int>::then() calls the functor if the future was ready.
  promise<int> p;
  future<int> f = p.get_future();

  p.set_value(42);
  bool called = false;
  future<void> next = f.then([&](future<int> r) { called = true; });
  EXPECT_TRUE(next.valid());
  EXPECT_TRUE(called);
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestInt, conform_2_3_8_d) {
  // future<int>::then() propagates the value from the functor to the returned
  // future.
  promise<int> p;
  future<int> f = p.get_future();

  future<int> next = f.then([&](future<int> r) { return 2 * r.get(); });
  EXPECT_TRUE(next.valid());
  p.set_value(42);
  EXPECT_EQ(std::future_status::ready, next.wait_for(0_ms));
  EXPECT_EQ(84, next.get());
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestInt, conform_2_3_8_e) {
  // future<int>::then() propagates exceptions raised by the functort to the
  // returned future.
  promise<int> p;
  future<int> f = p.get_future();

  future<void> next = f.then([&](future<int> r) {
    internal::ThrowRuntimeError("test exception in functor");
  });
  EXPECT_TRUE(next.valid());
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  p.set_value(42);
  EXPECT_EQ(std::future_status::ready, next.wait_for(0_ms));
  EXPECT_THROW(try { next.get(); } catch (std::runtime_error const& ex) {
    EXPECT_THAT(ex.what(), HasSubstr("test exception in functor"));
    throw;
  },
               std::runtime_error);
  EXPECT_FALSE(next.valid());
#else
  EXPECT_DEATH_IF_SUPPORTED(p.set_value(42), "test exception in functor");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestInt, conform_2_3_9_a) {
  // future<int>::then() returns a functor containing the type of the value
  // returned by the functor.
  promise<int> p;
  future<int> f = p.get_future();

  auto returns_void = [](future<int>) -> void {};
  EXPECT_TRUE(
      (std::is_same<future<void>, decltype(f.then(returns_void))>::value));

  auto returns_int = [](future<int>) -> int { return 42; };
  EXPECT_TRUE(
      (std::is_same<future<int>, decltype(f.then(returns_int))>::value));

  auto returns_string = [](future<int>) -> std::string { return "42"; };
  EXPECT_TRUE((std::is_same<future<std::string>,
                            decltype(f.then(returns_string))>::value));
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestInt, conform_2_3_9_b) {
  // future<int>::then() implicitly unwraps the future type when a functor
  // returns a future<>.
  promise<int> p;
  future<int> f = p.get_future();

  auto returns_void = [](future<int>) -> future<void> {
    return promise<void>().get_future();
  };
  EXPECT_TRUE(
      (std::is_same<future<void>, decltype(f.then(returns_void))>::value));

  auto returns_int = [](future<int>) -> future<int> {
    return promise<int>().get_future();
  };
  EXPECT_TRUE(
      (std::is_same<future<int>, decltype(f.then(returns_int))>::value));

  // The spec says the returned type must be future<R2> *exactly*, references do
  // not count.
  promise<int> p_int;
  future<int> f_int = p_int.get_future();
  auto returns_int_ref = [&f_int](future<int>) -> future<int>& {
    return f_int;
  };
  EXPECT_FALSE(
      (std::is_same<future<int>, decltype(f.then(returns_int_ref))>::value));
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestInt, conform_2_3_9_c) {
  // future<int>::then() implicitly unwrapping captures the returned value.
  promise<int> p;
  future<int> f = p.get_future();

  promise<int> p2;
  bool called = false;
  future<int> r = f.then([&p2, &called](future<int> f) {
    called = true;
    EXPECT_EQ(7, f.get());
    return p2.get_future();
  });
  EXPECT_TRUE(r.valid());
  EXPECT_FALSE(r.is_ready());
  EXPECT_FALSE(f.valid());

  p.set_value(7);
  EXPECT_TRUE(called);
  EXPECT_FALSE(r.is_ready());

  p2.set_value(42);
  EXPECT_TRUE(r.is_ready());
  EXPECT_EQ(42, r.get());
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestInt, conform_2_3_9_d) {
  // future<int>::then() implicitly unwrapping captures exceptions.
  promise<int> p;
  future<int> f = p.get_future();

  promise<int> p2;
  bool called = false;
  future<int> r = f.then([&p2, &called](future<int> f) {
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
  EXPECT_THROW(try { r.get(); } catch (std::runtime_error const& ex) {
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
      "future<T>::get\\(\\) had an exception but exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestInt, conform_2_3_9_e) {
  // future<int>::then() implicitly unwrapping raises on invalid future
  // returned by continuation.
  promise<int> p;
  future<int> f = p.get_future();

  bool called = false;
  future<int> r = f.then([&called](future<int> f) {
    called = true;
    f.get();
    return future<int>{};
  });
  EXPECT_TRUE(r.valid());
  EXPECT_FALSE(r.is_ready());
  EXPECT_FALSE(f.valid());

  p.set_value(7);
  EXPECT_TRUE(called);
  EXPECT_TRUE(r.is_ready());

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(try { r.get(); } catch (std::future_error const& ex) {
    EXPECT_EQ(std::future_errc::broken_promise, ex.code());
    throw;
  },
               std::future_error);
#else
  EXPECT_DEATH_IF_SUPPORTED(
      r.get(),
      "future<T>::get\\(\\) had an exception but exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestInt, conform_2_3_10) {
  // future<int>::then() invalidates the source future.
  promise<int> p;
  future<int> f = p.get_future();
  future<int> r = f.then([](future<int> f) { return 2 * f.get(); });
  EXPECT_TRUE(r.valid());
  EXPECT_FALSE(r.is_ready());
  EXPECT_FALSE(f.valid());

  p.set_value(42);
  EXPECT_TRUE(r.is_ready());
  EXPECT_EQ(2 * 42, r.get());
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestInt, conform_2_3_11_a) {
  // future<int>::is_ready() returns false for futures that are not ready.
  promise<int> p;
  future<int> const f = p.get_future();
  EXPECT_FALSE(f.is_ready());
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestInt, conform_2_3_11_b) {
  // future<int>::is_ready() returns true for futures that are ready.
  promise<int> p;
  future<int> const f = p.get_future();
  p.set_value(42);
  EXPECT_TRUE(f.is_ready());
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestInt, conform_2_3_11_c) {
  // future<int>::is_ready() raises for futures that are not valid.
  future<int> const f;
  ExpectFutureError([&] { f.is_ready(); }, std::future_errc::no_state);
}

/// @test Verify conformance with section 2.10 of the Concurrency TS.
TEST(FutureTestString, conform_2_10_4_2_a) {
  // When T is a simple value type we get back T.
  future<std::string> f = make_ready_future(std::string("42"));
  EXPECT_TRUE(f.valid());
  EXPECT_EQ(std::future_status::ready, f.wait_for(0_ms));
  EXPECT_EQ("42", f.get());
}

/// @test Verify conformance with section 2.10 of the Concurrency TS.
TEST(FutureTestString, conform_2_10_4_2_b) {
  // When T is a reference we get std::decay<T>::type.
  std::string value("42");
  std::string& sref = value;
  future<std::string> f = make_ready_future(sref);
  EXPECT_TRUE(f.valid());
  EXPECT_EQ(std::future_status::ready, f.wait_for(0_ms));
  EXPECT_EQ("42", f.get());
}

//// @test Verify conformance with section 2.10 of the Concurrency TS.
TEST(FutureTestString, conform_2_10_4_2_c) {
#if 1
  using V =
      internal::make_ready_return<std::reference_wrapper<std::string>>::type;
  static_assert(std::is_same<V, std::string&>::value, "Expected std::string&");
#else
  // TODO(#1410) - Implement future<R&> specialization.
  // When T is a reference wrapper get R&.
  std::string value("42");
  future<std::string&> f = make_ready_future(std::ref(value));
  EXPECT_TRUE(f.valid());
  EXPECT_EQ(std::future_status::ready, f.wait_for(0_ms));
  EXPECT_EQ("42", f.get());
#endif  // 1
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
