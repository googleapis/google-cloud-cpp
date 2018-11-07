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
#include "google/cloud/testing_util/chrono_literals.h"
#include <gmock/gmock.h>
#include <functional>

// C++ futures only make sense when exceptions are enabled.
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace {
using ::testing::HasSubstr;
using namespace testing_util::chrono_literals;

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
      throw std::runtime_error("test message");
    }
    return 2 * value;
  });
  EXPECT_FALSE(fut.valid());
  EXPECT_TRUE(next.valid());
  EXPECT_FALSE(called);

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
}

// The following tests reference the technical specification:
//   http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/p0159r0.html
// The test names match the section and paragraph from the TS.

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestInt, conform_2_3_2) {
  // TODO(#1345) - implement unwrapping constructor from future<future<int>>.
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

  // TODO(#1345) - test with callables returning non-void.
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
  // TODO(#1345) - test with an actual value when future<T> is implemented.
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
    throw std::runtime_error("test exception in functor");
  });
  EXPECT_TRUE(next.valid());
  p.set_value(42);
  EXPECT_EQ(std::future_status::ready, next.wait_for(0_ms));
  EXPECT_THROW(try { next.get(); } catch (std::runtime_error const& ex) {
    EXPECT_THAT(ex.what(), HasSubstr("test exception in functor"));
    throw;
  },
               std::runtime_error);
  EXPECT_FALSE(next.valid());
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

  // TODO(#1345) - implement the unwrapping functions.
  // The current implementation only declares that `.then()` should unwrap the
  // types, but does not actually work if called. These tests simply validate
  // that the declared types are correct.
  auto returns_void = [](future<int>) -> future<int> {
    return promise<int>().get_future();
  };
  EXPECT_TRUE(
      (std::is_same<future<int>, decltype(f.then(returns_void))>::value));

  // TODO(#1345) - add tests for other types when future<T> is implemented.
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
  EXPECT_THROW(try { f.is_ready(); } catch (std::future_error const& ex) {
    EXPECT_EQ(std::future_errc::no_state, ex.code());
    throw;
  },
               std::future_error);
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
