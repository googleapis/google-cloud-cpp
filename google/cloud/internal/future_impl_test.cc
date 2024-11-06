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

#include "google/cloud/internal/future_impl.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/expect_future_error.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::testing::HasSubstr;
using testing_util::chrono_literals::operator""_us;
using testing_util::ExpectFutureError;

TEST(FutureImplInt, Basic) {
  future_shared_state<int> shared_state;
  EXPECT_FALSE(shared_state.is_ready());
}

TEST(FutureImplInt, WaitFor) {
  future_shared_state<int> shared_state;
  auto start = std::chrono::steady_clock::now();
  auto s = shared_state.wait_for(100_us);
  auto elapsed = std::chrono::steady_clock::now() - start;
  EXPECT_EQ(static_cast<int>(s), static_cast<int>(std::future_status::timeout));
  EXPECT_LE(100_us, elapsed);
  EXPECT_FALSE(shared_state.is_ready());
}

TEST(FutureImplInt, WaitForReady) {
  future_shared_state<int> shared_state;
  shared_state.set_exception(
      std::make_exception_ptr(std::runtime_error("test_message")));
  auto s = shared_state.wait_for(100_us);
  EXPECT_EQ(std::future_status::ready, s);
  EXPECT_TRUE(shared_state.is_ready());
}

TEST(FutureImplInt, WaitUntil) {
  future_shared_state<int> shared_state;
  EXPECT_FALSE(shared_state.is_ready());
  auto start = std::chrono::steady_clock::now();
  auto s = shared_state.wait_until(std::chrono::system_clock::now() + 100_us);
  auto elapsed = std::chrono::steady_clock::now() - start;
  EXPECT_EQ(static_cast<int>(s), static_cast<int>(std::future_status::timeout));
  EXPECT_LE(100_us, elapsed);
  EXPECT_FALSE(shared_state.is_ready());
}

TEST(FutureImplInt, WaitUntilReady) {
  future_shared_state<int> shared_state;
  shared_state.set_exception(
      std::make_exception_ptr(std::runtime_error("test message")));
  auto s = shared_state.wait_until(std::chrono::system_clock::now() + 100_us);
  EXPECT_EQ(static_cast<int>(s), static_cast<int>(std::future_status::ready));
  EXPECT_TRUE(shared_state.is_ready());
}

TEST(FutureImplInt, SetExceptionCanBeCalledOnlyOnce) {
  future_shared_state<int> shared_state;
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

TEST(FutureImplInt, Abandon) {
  future_shared_state<int> shared_state;
  shared_state.abandon();
  EXPECT_TRUE(shared_state.is_ready());
}

TEST(FutureImplInt, AbandonReady) {
  future_shared_state<int> shared_state;
  shared_state.set_exception(
      std::make_exception_ptr(std::runtime_error("test message")));
  shared_state.abandon();
  SUCCEED();
  EXPECT_TRUE(shared_state.is_ready());
}

class TestContinuation : public Continuation<int> {
 public:
  explicit TestContinuation(int* r) : execute_counter(r) {}
  void Execute(SharedStateType<int>&) override { (*execute_counter)++; }

  int* execute_counter;
};

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
  std::cerr << "About to die\n";
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
      std::make_unique<TestContinuation>(&execute_counter));
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
      std::make_unique<TestContinuation>(&execute_counter));
  ExpectFutureError(
      [&] {
        shared_state.set_continuation(
            std::make_unique<TestContinuation>(&execute_counter));
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
      std::make_unique<TestContinuation>(&execute_counter));
  EXPECT_EQ(1, execute_counter);

  EXPECT_EQ(42, shared_state.get());
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
