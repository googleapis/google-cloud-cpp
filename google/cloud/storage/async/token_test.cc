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

#include "google/cloud/storage/async/token.h"
#include <gmock/gmock.h>
#include <type_traits>
#include <utility>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

TEST(AsyncToken, Basic) {
  EXPECT_FALSE(std::is_copy_constructible<AsyncToken>::value);
  EXPECT_FALSE(std::is_copy_assignable<AsyncToken>::value);
  EXPECT_TRUE(std::is_move_constructible<AsyncToken>::value);
  EXPECT_TRUE(std::is_move_assignable<AsyncToken>::value);

  int const placeholder1 = 0;
  int const placeholder2 = 0;
  AsyncToken const default_constructed;
  EXPECT_FALSE(default_constructed.valid());
  auto const impl1 = storage_internal::MakeAsyncToken(&placeholder1);
  auto const impl2 = storage_internal::MakeAsyncToken(&placeholder1);
  EXPECT_TRUE(impl1.valid());
  EXPECT_TRUE(impl2.valid());
  auto const impl3 = storage_internal::MakeAsyncToken(&placeholder2);
  EXPECT_NE(impl3, impl2);
}

TEST(AsyncToken, MoveConstructor) {
  int const placeholder1 = 0;
  auto t1 = storage_internal::MakeAsyncToken(&placeholder1);
  auto t2 = storage_internal::MakeAsyncToken(&placeholder1);
  EXPECT_TRUE(t1.valid());
  EXPECT_TRUE(t2.valid());
  EXPECT_EQ(t1, t2);
  AsyncToken t3(std::move(t1));
  EXPECT_FALSE(t1.valid());  // NOLINT(bugprone-use-after-move)
  EXPECT_TRUE(t3.valid());
  EXPECT_EQ(t3, t2);
}

TEST(AsyncToken, MoveAssignment) {
  int const placeholder1 = 0;
  auto t1 = storage_internal::MakeAsyncToken(&placeholder1);
  auto t2 = storage_internal::MakeAsyncToken(&placeholder1);
  EXPECT_TRUE(t1.valid());
  EXPECT_TRUE(t2.valid());
  EXPECT_EQ(t1, t2);
  AsyncToken t3;
  t3 = std::move(t1);
  EXPECT_FALSE(t1.valid());  // NOLINT(bugprone-use-after-move)
  EXPECT_TRUE(t3.valid());
  EXPECT_EQ(t3, t2);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google
