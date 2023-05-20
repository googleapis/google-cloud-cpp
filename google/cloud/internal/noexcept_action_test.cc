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

#include "google/cloud/internal/noexcept_action.h"
#include "google/cloud/internal/throw_delegate.h"
#include <gmock/gmock.h>
#include <stdexcept>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::testing::Optional;

TEST(NoExceptActionVoid, ActionThatThrows) {
  auto action = [] { ThrowRuntimeError("fail"); };
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_FALSE(NoExceptAction(action));
#else
  EXPECT_DEATH_IF_SUPPORTED(NoExceptAction(action), "fail");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(NoExceptActionVoid, ActionThatDoesNotThrow) {
  auto action = [] {};
  EXPECT_TRUE(NoExceptAction(action));
}

TEST(NoExceptActionNonVoid, ActionThatThrows) {
  auto action = []() -> int { ThrowRuntimeError("fail"); };
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_EQ(NoExceptAction<int>(action), absl::nullopt);
#else
  EXPECT_DEATH_IF_SUPPORTED(NoExceptAction(action), "fail");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(NoExceptActionNonVoid, ActionThatDoesNotThrow) {
  auto action = [] { return 5; };
  EXPECT_THAT(NoExceptAction<int>(action), Optional(5));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
