// Copyright 2019 Google LLC
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

#include "google/cloud/storage/internal/sha256_hash.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

TEST(Sha256Hash, HexEncodeEmpty) { EXPECT_EQ("", HexEncode({})); }

TEST(Sha256Hash, HexEncodeBasic) {
  EXPECT_EQ("0001ff7f10", HexEncode({0x00, 0x01, 0xFF, 0x7F, 0x10}));
}

TEST(Sha256Hash, Empty) {
  // The magic string was obtained using:
  //    /bin/echo -n "" | openssl sha256 -hex
  std::string expected =
      "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
  std::string actual = HexEncode(Sha256Hash(""));
  EXPECT_EQ(expected, actual);
}

TEST(Sha256Hash, Simple) {
  // The magic string was obtained using:
  //   /bin/echo -n 'The quick brown fox jumps over the lazy dog' |
  //       openssl sha256 -hex
  std::string expected =
      "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592";
  std::string actual =
      HexEncode(Sha256Hash("The quick brown fox jumps over the lazy dog"));
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
