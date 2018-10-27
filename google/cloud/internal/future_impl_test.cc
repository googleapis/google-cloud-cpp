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

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

using ::testing::HasSubstr;

using namespace google::cloud::testing_util::chrono_literals;

// C++ futures only make sense when exceptions are enabled.
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
TEST(FutureImplBaseTest, Basic) {
  future_shared_state_base shared_state;
  EXPECT_FALSE(shared_state.is_ready());
}

TEST(FutureImplBaseTest, WaitFor) {
  future_shared_state_base shared_state;
  auto s = shared_state.wait_for(100_us);
  EXPECT_EQ(static_cast<int>(s), static_cast<int>(std::future_status::timeout));
  EXPECT_FALSE(shared_state.is_ready());
}

TEST(FutureImplBaseTest, WaitUntil) {
  future_shared_state_base shared_state;
  EXPECT_FALSE(shared_state.is_ready());
  auto s = shared_state.wait_until(std::chrono::system_clock::now() + 100_us);
  EXPECT_EQ(static_cast<int>(s), static_cast<int>(std::future_status::timeout));
  EXPECT_FALSE(shared_state.is_ready());

  shared_state.set_exception(
      std::make_exception_ptr(std::runtime_error("test message")));
  s = shared_state.wait_until(std::chrono::system_clock::now() + 100_us);
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
  // TODO(#1345) - use future_shared_state<void> and call .get();
  future_shared_state_base shared_state;
  shared_state.abandon();
  EXPECT_TRUE(shared_state.is_ready());
}
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
