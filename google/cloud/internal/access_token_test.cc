// Copyright 2022 Google LLC
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

#include "google/cloud/internal/access_token.h"
#include "absl/time/time.h"
#include <gmock/gmock.h>
#include <sstream>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

using ::testing::HasSubstr;

TEST(AccessToken, Compare) {
  auto now = std::chrono::system_clock::now();
  auto a = AccessToken{"a", now};
  auto b = AccessToken{"b", now};
  auto c = AccessToken{"b", now + std::chrono::seconds(10)};
  auto d = AccessToken{"b", now + std::chrono::seconds(10)};
  EXPECT_EQ(a, a);
  EXPECT_NE(a, b);
  EXPECT_NE(a, c);
  EXPECT_EQ(c, d);
}

TEST(AccessToken, Stream) {
  auto now = std::chrono::system_clock::now();
  auto const token = std::string{
      "123456789a"
      "123456789b"
      "123456789c"
      "123456789d"};
  auto const input = AccessToken{token, now};
  std::ostringstream os;
  os << input;
  auto const actual = std::move(os).str();
  EXPECT_THAT(actual, HasSubstr("token=<"
                                "123456789a"
                                "123456789b"
                                "123456789c"
                                "12>"));
  auto expiration = absl::FormatTime(absl::FromChrono(now));
  EXPECT_THAT(actual, HasSubstr("expiration=" + expiration));
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
