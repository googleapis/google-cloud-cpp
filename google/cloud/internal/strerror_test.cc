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
#include <cerrno>
#include <cstring>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

using ::testing::HasSubstr;

#include "google/cloud/internal/disable_msvc_crt_secure_warnings.inc"
TEST(StrErrorTest, Simple) {
  auto const actual = ::google::cloud::internal::strerror(EDOM);
  // In the test we can call `std::strerror()` because the test is single
  // threaded.
  std::string expected = std::strerror(EDOM);
  EXPECT_EQ(actual, expected);
}
#include "google/cloud/internal/diagnostics_pop.inc"

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
