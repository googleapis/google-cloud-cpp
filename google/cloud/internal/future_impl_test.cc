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

#include "google/cloud/internal/future_impl.h"
#include "google/cloud/internal/make_unique.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/expect_future_error.h"
#include "google/cloud/testing_util/testing_types.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

using ::testing::HasSubstr;
using testing_util::ExpectFutureError;
using testing_util::NoDefaultConstructor;
using testing_util::Observable;

using namespace google::cloud::testing_util::chrono_literals;

TEST(FutureImplBaseTest, Basic) {
  future_shared_state_base shared_state;
  EXPECT_FALSE(shared_state.is_ready());
}

TEST(FutureImplBaseTest, WaitFor) {
  future_shared_state_base shared_state;
  auto start = std::chrono::steady_clock::now();
  auto s = shared_state.wait_for(100_us);
  auto elapsed = std::chrono::steady_clock::now() - start;
  EXPECT_EQ(static_cast<int>(s), static_cast<int>(std::future_status::timeout));
  EXPECT_LE(100_us, elapsed);
  EXPECT_FALSE(shared_state.is_ready());
}

TEST(FutureImplBaseTest, WaitForReady) {
  future_shared_state_base shared_state;
  shared_state.set_exception(
      std::make_exception_ptr(std::runtime_error("test_message")));
  auto s = shared_state.wait_for(100_us);
  EXPECT_EQ(std::future_status::ready, s);
  EXPECT_TRUE(shared_state.is_ready());
}

TEST(FutureImplBaseTest, WaitUntil) {
  future_shared_state_base shared_state;
  EXPECT_FALSE(shared_state.is_ready());
  auto start = std::chrono::steady_clock::now();
  auto s = shared_state.wait_until(std::chrono::system_clock::now() + 100_us);
  auto elapsed = std::chrono::steady_clock::now() - start;
  EXPECT_EQ(static_cast<int>(s), static_cast<int>(std::future_status::timeout));
  EXPECT_LE(100_us, elapsed);
  EXPECT_FALSE(shared_state.is_ready());
}

TEST(FutureImplBaseTest, WaitUntilReady) {
  future_shared_state_base shared_state;
  shared_state.set_exception(
      std::make_exception_ptr(std::runtime_error("test message")));
  auto s = shared_state.wait_until(std::chrono::system_clock::now() + 100_us);
  EXPECT_EQ(static_cast<int>(s), static_cast<int>(std::future_status::ready));
  EXPECT_TRUE(shared_state.is_ready());
}

TEST(FutureImplBaseTest, SetExceptionCanBeCalledOnlyOnce) {
  future_shared_state_base shared_state;
  EXPECT_FALSE(shared_state.is_ready());

  shared_state.set_exception(
      std::make_exception_ptr(std::runtime_error("test message")));
  EXPECT_TRUE(shared_state.is_ready());
  ExpectFutureError(
      [&] {
        shared_state.set_exception(
            std::make_exception_ptr(std::runtime_error("blah")));
      },
      std::future_errc::promise_already_satisfied);

  EXPECT_TRUE(shared_state.is_ready());
}

TEST(FutureImplBaseTest, Abandon) {
  future_shared_state_base shared_state;
  shared_state.abandon();
  EXPECT_TRUE(shared_state.is_ready());
}

TEST(FutureImplBaseTest, AbandonReady) {
  future_shared_state_base shared_state;
  shared_state.set_exception(
      std::make_exception_ptr(std::runtime_error("test message")));
  shared_state.abandon();
  SUCCEED();
  EXPECT_TRUE(shared_state.is_ready());
}

// @test Verify that we can create continuations.
TEST(ContinuationVoidTest, Constructor) {
  auto functor = [](std::shared_ptr<future_shared_state<void>> state) {};

  using tested_type = continuation<decltype(functor), void>;

  auto input = std::make_shared<future_shared_state<void>>();
  auto cont = std::make_shared<tested_type>(std::move(functor), input);

  auto current = cont->input.lock();
  EXPECT_EQ(input.get(), current.get());
}

/// @test Verify that satisfying the shared state with an exception calls the
/// continuation.
TEST(ContinuationVoidTest, SetExceptionCallsContinuation) {
  bool called = false;
  auto functor = [&called](std::shared_ptr<future_shared_state<void>> state) {
    called = true;
    state->get();
  };

  auto input = std::make_shared<future_shared_state<void>>();
  std::shared_ptr<future_shared_state<void>> output =
      input->make_continuation(input, std::move(functor));

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  input->set_exception(
      std::make_exception_ptr(std::runtime_error("test message")));
  EXPECT_TRUE(called);
  EXPECT_TRUE(output->is_ready());
  EXPECT_THROW(
      try { output->get(); } catch (std::runtime_error const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("test message"));
        throw;
      },
      std::runtime_error);
#else
  EXPECT_DEATH_IF_SUPPORTED(
      input->set_exception(
          std::make_exception_ptr(std::runtime_error("test message"))),
      "future<void>::get\\(\\) had an exception but exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify that satisfying the shared state with a value calls the
/// continuation.
TEST(ContinuationVoidTest, SetValueCallsContinuation) {
  bool called = false;
  auto functor = [&called](std::shared_ptr<future_shared_state<void>> state) {
    called = true;
    state->get();
  };

  auto input = std::make_shared<future_shared_state<void>>();
  std::shared_ptr<future_shared_state<void>> output =
      input->make_continuation(input, std::move(functor));

  input->set_value();
  EXPECT_TRUE(called);
  EXPECT_TRUE(output->is_ready());
  output->get();
  SUCCEED();
}

class TestContinuation : public continuation_base {
 public:
  TestContinuation(int* r) : execute_counter(r) {}
  void execute() override { (*execute_counter)++; }

  int* execute_counter;
};

TEST(FutureImplVoid, SetValue) {
  future_shared_state<void> shared_state;
  EXPECT_FALSE(shared_state.is_ready());
  shared_state.set_value();
  EXPECT_TRUE(shared_state.is_ready());
  shared_state.get();
  SUCCEED();
}

TEST(FutureImplVoid, SetValueCanBeCalledOnlyOnce) {
  future_shared_state<void> shared_state;
  EXPECT_FALSE(shared_state.is_ready());

  shared_state.set_value();
  ExpectFutureError([&] { shared_state.set_value(); },
                    std::future_errc::promise_already_satisfied);

  shared_state.get();
  SUCCEED();
}

TEST(FutureImplVoid, GetException) {
  future_shared_state<void> shared_state;
  EXPECT_FALSE(shared_state.is_ready());
  shared_state.set_exception(
      std::make_exception_ptr(std::runtime_error("test message")));
  EXPECT_TRUE(shared_state.is_ready());
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      try { shared_state.get(); } catch (std::runtime_error const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("test message"));
        throw;
      },
      std::runtime_error);
#else
  EXPECT_DEATH_IF_SUPPORTED(
      shared_state.get(),
      "future<void>::get\\(\\) had an exception but exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(FutureImplVoid, Abandon) {
  future_shared_state<void> shared_state;
  shared_state.abandon();
  EXPECT_TRUE(shared_state.is_ready());
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      try { shared_state.get(); } catch (std::future_error const& ex) {
        EXPECT_EQ(std::future_errc::broken_promise, ex.code());
        throw;
      },
      std::future_error);
#else
  EXPECT_DEATH_IF_SUPPORTED(
      shared_state.get(),
      "future<void>::get\\(\\) had an exception but exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(FutureImplVoid, SetContinuation) {
  future_shared_state<void> shared_state;
  EXPECT_FALSE(shared_state.is_ready());

  int execute_counter = 0;
  shared_state.set_continuation(
      google::cloud::internal::make_unique<TestContinuation>(&execute_counter));
  EXPECT_EQ(0, execute_counter);
  EXPECT_FALSE(shared_state.is_ready());
  shared_state.set_value();
  EXPECT_EQ(1, execute_counter);

  shared_state.get();
  SUCCEED();
}

TEST(FutureImplVoid, SetContinuationAlreadySet) {
  future_shared_state<void> shared_state;
  EXPECT_FALSE(shared_state.is_ready());

  int execute_counter = 0;
  shared_state.set_continuation(
      google::cloud::internal::make_unique<TestContinuation>(&execute_counter));

  ExpectFutureError(
      [&] {
        shared_state.set_continuation(
            google::cloud::internal::make_unique<TestContinuation>(
                &execute_counter));
      },
      std::future_errc::future_already_retrieved);
}

TEST(FutureImplVoid, SetContinuationAlreadySatisfied) {
  future_shared_state<void> shared_state;
  EXPECT_FALSE(shared_state.is_ready());

  int execute_counter = 0;
  shared_state.set_value();
  EXPECT_EQ(0, execute_counter);
  shared_state.set_continuation(
      google::cloud::internal::make_unique<TestContinuation>(&execute_counter));
  EXPECT_EQ(1, execute_counter);

  shared_state.get();
  SUCCEED();
}

TEST(FutureImplVoid, MarkRetrieved) {
  auto sh = std::make_shared<future_shared_state<void>>();
  future_shared_state<void>::mark_retrieved(sh);
  SUCCEED();
}

TEST(FutureImplVoid, MarkRetrievedCanBeCalledOnlyOnce) {
  auto sh = std::make_shared<future_shared_state<void>>();
  future_shared_state<void>::mark_retrieved(sh);
  ExpectFutureError([&] { future_shared_state<void>::mark_retrieved(sh); },
                    std::future_errc::future_already_retrieved);
}

TEST(FutureImplVoid, MarkRetrievedFailure) {
  std::shared_ptr<future_shared_state<void>> sh;
  ExpectFutureError([&] { future_shared_state<void>::mark_retrieved(sh); },
                    std::future_errc::no_state);
}

TEST(FutureImplInt, SetException) {
  future_shared_state<int> shared_state;
  EXPECT_FALSE(shared_state.is_ready());

  shared_state.set_exception(
      std::make_exception_ptr(std::runtime_error("test message")));
  EXPECT_TRUE(shared_state.is_ready());
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      try { shared_state.get(); } catch (std::runtime_error const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("test message"));
        throw;
      },
      std::runtime_error);
#else
  std::cerr << "About to die" << std::endl;
  EXPECT_DEATH_IF_SUPPORTED(
      shared_state.get(),
      "future<T>::get\\(\\) had an exception but exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(FutureImplInt, SetValue) {
  future_shared_state<int> shared_state;
  EXPECT_FALSE(shared_state.is_ready());
  shared_state.set_value(42);
  EXPECT_TRUE(shared_state.is_ready());
  EXPECT_EQ(42, shared_state.get());
}

TEST(FutureImplInt, SetValueCanBeCalledOnlyOnce) {
  future_shared_state<int> shared_state;
  EXPECT_FALSE(shared_state.is_ready());

  shared_state.set_value(42);
  ExpectFutureError([&] { shared_state.set_value(42); },
                    std::future_errc::promise_already_satisfied);

  EXPECT_EQ(42, shared_state.get());
}

TEST(FutureImplInt, GetException) {
  future_shared_state<int> shared_state;
  EXPECT_FALSE(shared_state.is_ready());
  shared_state.set_exception(
      std::make_exception_ptr(std::runtime_error("test message")));
  EXPECT_TRUE(shared_state.is_ready());
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      try { shared_state.get(); } catch (std::runtime_error const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("test message"));
        throw;
      },
      std::runtime_error);
#else
  EXPECT_DEATH_IF_SUPPORTED(
      shared_state.get(),
      "future<T>::get\\(\\) had an exception but exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(FutureImplInt, Abandon) {
  future_shared_state<int> shared_state;
  shared_state.abandon();
  EXPECT_TRUE(shared_state.is_ready());
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      try { shared_state.get(); } catch (std::future_error const& ex) {
        EXPECT_EQ(std::future_errc::broken_promise, ex.code());
        throw;
      },
      std::future_error);
#else
  EXPECT_DEATH_IF_SUPPORTED(
      shared_state.get(),
      "future<T>::get\\(\\) had an exception but exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(FutureImplInt, MarkRetrieved) {
  auto sh = std::make_shared<future_shared_state<int>>();
  future_shared_state<int>::mark_retrieved(sh);
  SUCCEED();
}

TEST(FutureImplInt, MarkRetrievedCanBeCalledOnlyOnce) {
  auto sh = std::make_shared<future_shared_state<int>>();
  future_shared_state<int>::mark_retrieved(sh);
  ExpectFutureError([&] { future_shared_state<int>::mark_retrieved(sh); },
                    std::future_errc::future_already_retrieved);
}

TEST(FutureImplInt, MarkRetrievedFailure) {
  std::shared_ptr<future_shared_state<int>> sh;
  ExpectFutureError([&] { future_shared_state<int>::mark_retrieved(sh); },
                    std::future_errc::no_state);
}

TEST(FutureImplInt, SetContinuation) {
  future_shared_state<int> shared_state;
  EXPECT_FALSE(shared_state.is_ready());

  int execute_counter = 0;
  shared_state.set_continuation(
      google::cloud::internal::make_unique<TestContinuation>(&execute_counter));
  EXPECT_EQ(0, execute_counter);
  EXPECT_FALSE(shared_state.is_ready());
  shared_state.set_value(42);
  EXPECT_EQ(1, execute_counter);

  shared_state.get();
  SUCCEED();
}

TEST(FutureImplInt, SetContinuationAlreadySet) {
  future_shared_state<int> shared_state;
  EXPECT_FALSE(shared_state.is_ready());

  int execute_counter = 0;
  shared_state.set_continuation(
      google::cloud::internal::make_unique<TestContinuation>(&execute_counter));
  ExpectFutureError(
      [&] {
        shared_state.set_continuation(
            google::cloud::internal::make_unique<TestContinuation>(
                &execute_counter));
      },
      std::future_errc::future_already_retrieved);
}

TEST(FutureImplInt, SetContinuationAlreadySatisfied) {
  future_shared_state<int> shared_state;
  EXPECT_FALSE(shared_state.is_ready());

  int execute_counter = 0;
  shared_state.set_value(42);
  EXPECT_EQ(0, execute_counter);
  shared_state.set_continuation(
      google::cloud::internal::make_unique<TestContinuation>(&execute_counter));
  EXPECT_EQ(1, execute_counter);

  EXPECT_EQ(42, shared_state.get());
}

// @test Verify that we can create continuations.
TEST(ContinuationIntTest, Constructor) {
  auto functor = [](std::shared_ptr<future_shared_state<int>> state) {};

  using tested_type = continuation<decltype(functor), int>;

  auto input = std::make_shared<future_shared_state<int>>();
  auto cont = std::make_shared<tested_type>(std::move(functor), input);

  auto current = cont->input.lock();
  EXPECT_EQ(input.get(), current.get());
}

/// @test Verify that satisfying the shared state with an exception calls the
/// continuation.
TEST(ContinuationIntTest, SetExceptionCallsContinuation) {
  bool called = false;
  auto functor = [&called](std::shared_ptr<future_shared_state<int>> state) {
    called = true;
    return 2 * state->get();
  };

  auto input = std::make_shared<future_shared_state<int>>();
  std::shared_ptr<future_shared_state<int>> output =
      input->make_continuation(input, std::move(functor));

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  input->set_exception(
      std::make_exception_ptr(std::runtime_error("test message")));
  EXPECT_TRUE(called);
  EXPECT_TRUE(output->is_ready());
  EXPECT_THROW(
      try { output->get(); } catch (std::runtime_error const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("test message"));
        throw;
      },
      std::runtime_error);
#else
  EXPECT_DEATH_IF_SUPPORTED(
      input->set_exception(
          std::make_exception_ptr(std::runtime_error("test message"))),
      "future<T>::get\\(\\) had an exception but exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify that satisfying the shared state with a value calls the
/// continuation.
TEST(ContinuationIntTest, SetValueCallsContinuation) {
  bool called = false;
  auto functor = [&called](std::shared_ptr<future_shared_state<int>> state) {
    called = true;
    return 2 * state->get();
  };

  auto input = std::make_shared<future_shared_state<int>>();
  std::shared_ptr<future_shared_state<int>> output =
      input->make_continuation(input, std::move(functor));

  input->set_value(42);
  EXPECT_TRUE(called);
  EXPECT_TRUE(output->is_ready());
  EXPECT_EQ(84, output->get());
}

TEST(FutureImplNoDefaultConstructor, SetValue) {
  future_shared_state<NoDefaultConstructor> shared_state;
  EXPECT_FALSE(shared_state.is_ready());

  shared_state.set_value(NoDefaultConstructor("42"));
  EXPECT_TRUE(shared_state.is_ready());

  NoDefaultConstructor result = shared_state.get();
  EXPECT_EQ("42", result.str());
}

TEST(FutureImplObservable, NeverSet) {
  Observable::reset_counters();
  {
    future_shared_state<Observable> shared_state;
    EXPECT_FALSE(shared_state.is_ready());
    EXPECT_EQ(0, Observable::default_constructor);
    EXPECT_EQ(0, Observable::destructor);
  }
  EXPECT_EQ(0, Observable::default_constructor);
  EXPECT_EQ(0, Observable::destructor);
}

TEST(FutureImplObservable, SetValue) {
  Observable::reset_counters();
  {
    future_shared_state<Observable> shared_state;
    EXPECT_FALSE(shared_state.is_ready());

    shared_state.set_value(Observable("set value"));
    EXPECT_EQ(0, Observable::default_constructor);
    EXPECT_EQ(1, Observable::value_constructor);
    EXPECT_EQ(0, Observable::copy_constructor);
    EXPECT_EQ(1, Observable::move_constructor);
    EXPECT_EQ(0, Observable::copy_assignment);
    EXPECT_EQ(0, Observable::move_assignment);
    EXPECT_EQ(1, Observable::destructor);
    {
      Observable value = shared_state.get();
      EXPECT_EQ(0, Observable::default_constructor);
      EXPECT_EQ(1, Observable::value_constructor);
      EXPECT_EQ(0, Observable::copy_constructor);
      EXPECT_EQ(2, Observable::move_constructor);
      EXPECT_EQ(0, Observable::copy_assignment);
      EXPECT_EQ(0, Observable::move_assignment);
      EXPECT_EQ(1, Observable::destructor);
    }
    EXPECT_EQ(0, Observable::default_constructor);
    EXPECT_EQ(1, Observable::value_constructor);
    EXPECT_EQ(0, Observable::copy_constructor);
    EXPECT_EQ(2, Observable::move_constructor);
    EXPECT_EQ(0, Observable::copy_assignment);
    EXPECT_EQ(0, Observable::move_assignment);
    EXPECT_EQ(2, Observable::destructor);
  }
  EXPECT_EQ(0, Observable::default_constructor);
  EXPECT_EQ(1, Observable::value_constructor);
  EXPECT_EQ(0, Observable::copy_constructor);
  EXPECT_EQ(2, Observable::move_constructor);
  EXPECT_EQ(0, Observable::copy_assignment);
  EXPECT_EQ(0, Observable::move_assignment);
  EXPECT_EQ(3, Observable::destructor);
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
