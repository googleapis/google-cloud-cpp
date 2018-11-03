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

// The following tests reference the technical specification:
//   http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/p0159r0.html
// The test names match the section and paragraph from the TS.

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
