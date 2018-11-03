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

#include "google/cloud/internal/future_void.h"
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

  EXPECT_NO_THROW(next.get());
  EXPECT_FALSE(next.valid());
}

TEST(FutureTestVoid, ThenException) {
  promise<void> p;
  future<void> fut = p.get_future();
  EXPECT_TRUE(fut.valid());

  bool called = false;
  future<void> next = fut.then([&called](future<void> r) {
    called = true;
    throw std::runtime_error("test message");
  });
  EXPECT_FALSE(fut.valid());
  EXPECT_TRUE(next.valid());
  EXPECT_FALSE(called);

  p.set_value();
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
TEST(FutureTestVoid, conform_2_3_2) {
  // TODO(#1345) - implement unwrapping constructor from future<future<void>>.
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestVoid, conform_2_3_5) {
  // future<void>::then() is a template member function that takes callables.
  future<void> f;
  auto callable = [](future<void> f) -> void { f.get(); };
  EXPECT_TRUE((std::is_same<future<void>, decltype(f.then(callable))>::value));

  // TODO(#1345) - test with callables returning non-void.
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

  // TODO(#1345) - test with callables returning non-void.
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
  // TODO(#1345) - test with an actual value when future<T> is implemented.
  // future<void>::then() propagates the value from the functor to the returned
  // future.
  promise<void> p;
  future<void> f = p.get_future();

  future<void> next = f.then([&](future<void> r) {});
  EXPECT_TRUE(next.valid());
  p.set_value();
  EXPECT_EQ(std::future_status::ready, next.wait_for(0_ms));
  EXPECT_NO_THROW(next.get());
}

/// @test Verify conformance with section 2.3 of the Concurrency TS.
TEST(FutureTestVoid, conform_2_3_8_e) {
  // future<void>::then() propagates exceptions raised by the functort to the
  // returned future.
  promise<void> p;
  future<void> f = p.get_future();

  future<void> next = f.then([&](future<void> r) {
    throw std::runtime_error("test exception in functor");
  });
  EXPECT_TRUE(next.valid());
  p.set_value();
  EXPECT_EQ(std::future_status::ready, next.wait_for(0_ms));
  EXPECT_THROW(try { next.get(); } catch (std::runtime_error const& ex) {
    EXPECT_THAT(ex.what(), HasSubstr("test exception in functor"));
    throw;
  },
               std::runtime_error);
  EXPECT_FALSE(next.valid());
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

  // TODO(#1345) - implement the unwrapping functions.
  // The current implementation only declares that `.then()` should unwrap the
  // types, but does not actually work if called. These tests simply validate
  // that the declared types are correct.
  auto returns_void = [](future<void>) -> future<void> {
    return promise<void>().get_future();
  };
  EXPECT_TRUE(
      (std::is_same<future<void>, decltype(f.then(returns_void))>::value));

  // TODO(#1345) - add tests for other types when future<T> is implemented.
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
  EXPECT_THROW(try { f.is_ready(); } catch(std::future_error const& ex) {
    EXPECT_EQ(std::future_errc::no_state, ex.code());
    throw;
  }, std::future_error);
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
