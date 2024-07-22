// Copyright 2024 Google LLC
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

#include "google/cloud/storage/internal/md5hash.h"
#include <gmock/gmock.h>
#include <vector>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::ElementsAreArray;

TEST(MD5Hash, Empty) {
  // I used this command to get the expected value:
  // /bin/echo -n "" | openssl md5
  auto const expected =
      std::vector<std::uint8_t>{0xd4, 0x1d, 0x8c, 0xd9, 0x8f, 0x00, 0xb2, 0x04,
                                0xe9, 0x80, 0x09, 0x98, 0xec, 0xf8, 0x42, 0x7e};

  // There are many ways to represent the "empty" string:
  std::vector<std::vector<std::uint8_t>> const values{
      MD5Hash({}),
      MD5Hash(nullptr),
      MD5Hash(""),
      MD5Hash(absl::string_view{}),
  };

  for (auto const& actual : values) {
    EXPECT_THAT(actual, ElementsAreArray(expected));
  }
}

TEST(MD5Hash, Simple) {
  auto const actual = MD5Hash("The quick brown fox jumps over the lazy dog");
  // I used this command to get the expected value:
  // /bin/echo -n "The quick brown fox jumps over the lazy dog" |
  //     openssl md5 -binary | openssl base64
  auto const expected =
      std::vector<std::uint8_t>{0x9e, 0x10, 0x7d, 0x9d, 0x37, 0x2b, 0xb6, 0x82,
                                0x6b, 0xd8, 0x1d, 0x35, 0x42, 0xa4, 0x19, 0xd6};
  EXPECT_THAT(actual, ElementsAreArray(expected));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
