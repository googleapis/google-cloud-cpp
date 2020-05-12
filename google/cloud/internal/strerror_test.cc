// Copyright 2020 Google LLC
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

#include "google/cloud/internal/strerror.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

using ::testing::HasSubstr;
using ::testing::Not;

TEST(StrErrorTest, Simple) {
  auto const actual = ::google::cloud::internal::strerror(ENAMETOOLONG);
  EXPECT_FALSE(actual.empty());
  EXPECT_THAT(actual, Not(HasSubstr("Cannot get error message")));
}

TEST(StrErrorTest, InvalidErrno) {
  auto const actual = ::google::cloud::internal::strerror(-123456789);
  EXPECT_FALSE(actual.empty());
  EXPECT_THAT(actual, HasSubstr("-123456789"));
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
