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
#include <gmock/gmock.h>

// C++ futures only make sense when exceptions are enabled.
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

using ::testing::HasSubstr;

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

  EXPECT_THROW(
      try {
        shared_state.set_exception(
            std::make_exception_ptr(std::runtime_error("blah")));
      } catch (std::future_error const& ex) {
        EXPECT_EQ(std::future_errc::promise_already_satisfied, ex.code());
        throw;
      },
      std::future_error);

  EXPECT_TRUE(shared_state.is_ready());
}

TEST(FutureImplBaseTest, Abandon) {
  future_shared_state_base shared_state;
  shared_state.abandon();
  EXPECT_TRUE(shared_state.is_ready());
}

TEST(FutureImplBaseTest, AbandonReady) {
  // TODO(#1345) - use future_shared_state<void> and call .get();
  future_shared_state_base shared_state;
  shared_state.set_exception(
      std::make_exception_ptr(std::runtime_error("test message")));
  EXPECT_NO_THROW(shared_state.abandon());
  EXPECT_TRUE(shared_state.is_ready());
}

TEST(FutureImplVoid, SetValue) {
  future_shared_state<void> shared_state;
  EXPECT_FALSE(shared_state.is_ready());
  shared_state.set_value();
  EXPECT_TRUE(shared_state.is_ready());
  EXPECT_NO_THROW(shared_state.get());
}

TEST(FutureImplVoid, GetException) {
  future_shared_state<void> shared_state;
  EXPECT_FALSE(shared_state.is_ready());
  shared_state.set_exception(
      std::make_exception_ptr(std::runtime_error("test message")));
  EXPECT_TRUE(shared_state.is_ready());
  EXPECT_THROW(try { shared_state.get(); } catch (std::runtime_error const& ex) {
    EXPECT_THAT(ex.what(), HasSubstr("test message"));
    throw;
  },
               std::runtime_error);
}

TEST(FutureImplVoid, Abandon) {
  future_shared_state<void> shared_state;
  shared_state.abandon();
  EXPECT_TRUE(shared_state.is_ready());
  EXPECT_THROW(try { shared_state.get(); } catch (std::future_error const& ex) {
    EXPECT_EQ(std::future_errc::broken_promise, ex.code());
    throw;
  },
               std::future_error);
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
