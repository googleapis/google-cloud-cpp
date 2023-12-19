// Copyright 2023 Google LLC
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

#include "google/cloud/internal/future_then_impl.h"
#include <gmock/gmock.h>
#include <cstdint>
#include <memory>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::testing::HasSubstr;

template <typename T>
void TestSetExceptionPtr(future_shared_state<T>& input) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  input.set_exception(std::make_exception_ptr(std::runtime_error("test-only")));
#else
  input.set_exception(nullptr);
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(FutureThenImpl, EvalVoid) {
  auto const r = FutureThenImpl::eval([](int) {}, 42);
  EXPECT_TRUE((std::is_same<FutureVoid const, decltype(r)>::value));
}

TEST(FutureThenImpl, EvalNonVoid) {
  auto const r = FutureThenImpl::eval([](int x) { return 2 * x; }, 42);
  EXPECT_EQ(r, 84);
}

TEST(FutureThenImpl, EvalVoidMoveOnlyArg) {
  auto const r = FutureThenImpl::eval([](std::unique_ptr<int>) {},
                                      std::make_unique<int>(42));
  EXPECT_TRUE((std::is_same<FutureVoid const, decltype(r)>::value));
}

TEST(FutureThenImpl, EvalNonVoidMoveOnlyArg) {
  auto const r = FutureThenImpl::eval([](std::unique_ptr<int> x) { return *x; },
                                      std::make_unique<int>(42));
  EXPECT_EQ(r, 42);
}

TEST(FutureThenImpl, EvalVoidMoveOnlyCallable) {
  auto f = [x = std::make_unique<int>()](int) {};
  auto const r = FutureThenImpl::eval(std::move(f), 42);
  EXPECT_TRUE((std::is_same<FutureVoid const, decltype(r)>::value));
}

TEST(FutureThenImpl, EvalNonVoidMoveOnlyCallable) {
  auto f = [x = std::make_unique<int>()](int a) { return a; };
  auto const r = FutureThenImpl::eval(std::move(f), 42);
  EXPECT_EQ(r, 42);
}

TEST(FutureThenImpl, SetResultValue) {
  auto output = std::make_shared<future_shared_state<std::size_t>>();
  FutureThenImpl::set_result(
      output, [](int x) -> std::size_t { return x; }, 42);
  ASSERT_TRUE(output->is_ready());
  EXPECT_EQ(output->get(), 42);
}

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
TEST(FutureThenImpl, SetResultException) {
  auto output = std::make_shared<future_shared_state<int>>();
  FutureThenImpl::set_result(
      output, [](int) -> int { throw std::runtime_error("test-only"); }, 42);
  ASSERT_TRUE(output->is_ready());
  EXPECT_THROW(
      try { output->get(); } catch (std::runtime_error const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("test-only"));
        throw;
      },
      std::runtime_error);
}
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS

TEST(FutureThenImpl, UnwrapMatchingTypesValue) {
  auto output = std::make_shared<future_shared_state<std::int64_t>>();
  auto input = std::make_shared<future_shared_state<int>>();
  FutureThenImpl::unwrap(output, input);
  EXPECT_FALSE(output->is_ready());
  input->set_value(42);
  EXPECT_TRUE(output->is_ready());
  EXPECT_EQ(output->get(), 42);
}

TEST(FutureThenImpl, UnwrapMatchingTypesException) {
  auto output = std::make_shared<future_shared_state<std::int64_t>>();
  auto input = std::make_shared<future_shared_state<int>>();
  FutureThenImpl::unwrap(output, input);
  EXPECT_FALSE(output->is_ready());
  TestSetExceptionPtr(*input);
  EXPECT_TRUE(output->is_ready());

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      try { output->get(); } catch (std::runtime_error const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("test-only"));
        throw;
      },
      std::runtime_error);
#else
  EXPECT_DEATH_IF_SUPPORTED(output->get(), "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(FutureThenImpl, DISABLED_UnwrapMatchingTypesAbandoned) {
  auto output = std::make_shared<future_shared_state<std::int64_t>>();
  auto input = std::make_shared<future_shared_state<int>>();
  FutureThenImpl::unwrap(output, input);
  EXPECT_FALSE(output->is_ready());
  input->abandon();
  EXPECT_TRUE(output->is_ready());

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      try { output->get(); } catch (std::future_error const& ex) {
        EXPECT_EQ(ex.code(), std::future_errc::broken_promise);
        throw;
      },
      std::runtime_error);
#else
  EXPECT_DEATH_IF_SUPPORTED(output->get(), "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(FutureThenImpl, UnwrapFutureValue) {
  auto output = std::make_shared<future_shared_state<int>>();
  auto input = std::make_shared<future_shared_state<future<int>>>();
  FutureThenImpl::unwrap(output, input);
  EXPECT_FALSE(output->is_ready());
  promise<int> p;
  input->set_value(p.get_future());
  EXPECT_FALSE(output->is_ready());
  p.set_value(42);
  EXPECT_TRUE(output->is_ready());
  EXPECT_EQ(output->get(), 42);
}

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
TEST(FutureThenImpl, UnwrapFutureException1) {
  auto output = std::make_shared<future_shared_state<int>>();
  auto input = std::make_shared<future_shared_state<future<int>>>();
  FutureThenImpl::unwrap(output, input);
  EXPECT_FALSE(output->is_ready());
  promise<int> p;
  input->set_value(p.get_future());
  EXPECT_FALSE(output->is_ready());
  p.set_exception(std::make_exception_ptr(std::runtime_error("test-only")));

  EXPECT_TRUE(output->is_ready());
  EXPECT_THROW(
      try { output->get(); } catch (std::runtime_error const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("test-only"));
        throw;
      },
      std::runtime_error);
}
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS

TEST(FutureThenImpl, UnwrapFutureException2) {
  auto output = std::make_shared<future_shared_state<int>>();
  auto input = std::make_shared<future_shared_state<future<int>>>();
  FutureThenImpl::unwrap(output, input);
  EXPECT_FALSE(output->is_ready());
  TestSetExceptionPtr(*input);
  EXPECT_TRUE(output->is_ready());

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      try { output->get(); } catch (std::runtime_error const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("test-only"));
        throw;
      },
      std::runtime_error);
#else
  EXPECT_DEATH_IF_SUPPORTED(output->get(), "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(FutureThenImpl, ThenImplValue) {
  promise<int> p;
  auto f = p.get_future();
  auto h = FutureThenImpl::then_impl(f, [](auto g) { return 2 * g.get(); });
  EXPECT_FALSE(h.is_ready());
  p.set_value(42);
  EXPECT_TRUE(h.is_ready());
  EXPECT_EQ(h.get(), 84);
}

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
TEST(FutureThenImpl, ThenImplException) {
  promise<int> p;
  auto f = p.get_future();
  auto h = FutureThenImpl::then_impl(
      f, [](auto) -> int { throw std::runtime_error("test-only"); });
  EXPECT_FALSE(h.is_ready());
  p.set_value(42);
  EXPECT_TRUE(h.is_ready());

  EXPECT_THROW(
      try { h.get(); } catch (std::runtime_error const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("test-only"));
        throw;
      },
      std::runtime_error);
}
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS

TEST(FutureThenImpl, ThenImplValueUnwrap1) {
  promise<int> p;
  auto f = p.get_future();
  future<int> h = FutureThenImpl::then_impl(f, [](auto g) { return g; });
  EXPECT_FALSE(h.is_ready());
  p.set_value(42);
  EXPECT_TRUE(h.is_ready());
  EXPECT_EQ(h.get(), 42);
}

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
TEST(FutureThenImpl, ThenImplUnwrapException) {
  promise<int> p;
  auto f = p.get_future();
  auto h = FutureThenImpl::then_impl(
      f, [](auto) -> future<int> { throw std::runtime_error("test-only"); });
  EXPECT_FALSE(h.is_ready());
  p.set_value(42);
  EXPECT_TRUE(h.is_ready());

  EXPECT_THROW(
      try { h.get(); } catch (std::runtime_error const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("test-only"));
        throw;
      },
      std::runtime_error);
}
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS

TEST(FutureThenImpl, ThenImplValueUnwrap2) {
  promise<int> p;
  auto f = p.get_future();
  future<int> h = FutureThenImpl::then_impl(
      f, [](auto g) { return make_ready_future(std::move(g)); });
  EXPECT_FALSE(h.is_ready());
  p.set_value(42);
  EXPECT_TRUE(h.is_ready());
  EXPECT_EQ(h.get(), 42);
}

TEST(FutureThenImpl, ThenImplValueUnwrap3) {
  promise<int> p;
  auto f = p.get_future();
  future<int> h = FutureThenImpl::then_impl(
      f, [](auto g) { return g.then([](auto t) { return 2 * t.get(); }); });
  EXPECT_FALSE(h.is_ready());
  p.set_value(42);
  EXPECT_TRUE(h.is_ready());
  EXPECT_EQ(h.get(), 84);
}

TEST(FutureThenImpl, ThenImplVoidUnwrap1) {
  promise<void> p;
  auto f = p.get_future();
  future<void> h = FutureThenImpl::then_impl(f, [](auto g) { return g; });
  EXPECT_FALSE(h.is_ready());
  p.set_value();
  EXPECT_TRUE(h.is_ready());
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_NO_THROW(h.get());
#else
  EXPECT_NO_FATAL_FAILURE(h.get());
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(FutureThenImpl, ThenImplVoidUnwrap2) {
  promise<int> p;
  auto f = p.get_future();
  future<void> h =
      FutureThenImpl::then_impl(f, [](auto) { return make_ready_future(); });
  EXPECT_FALSE(h.is_ready());
  p.set_value(0);
  EXPECT_TRUE(h.is_ready());
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_NO_THROW(h.get());
#else
  EXPECT_NO_FATAL_FAILURE(h.get());
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(FutureThenImpl, ThenImplVoidUnwrap3) {
  promise<void> p;
  auto f = p.get_future();
  future<int> h = FutureThenImpl::then_impl(f, [](auto) { return 42; });
  EXPECT_FALSE(h.is_ready());
  p.set_value();
  EXPECT_TRUE(h.is_ready());
  EXPECT_EQ(h.get(), 42);
}

TEST(FutureThenImpl, ThenImplVoidUnwrap4) {
  promise<void> p;
  auto f = p.get_future();
  future<int> h =
      FutureThenImpl::then_impl(f, [](auto) { return make_ready_future(42); });
  EXPECT_FALSE(h.is_ready());
  p.set_value();
  EXPECT_TRUE(h.is_ready());
  EXPECT_EQ(h.get(), 42);
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
