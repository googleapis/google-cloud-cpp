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

#include "google/cloud/internal/sha256_hmac.h"
#include "google/cloud/internal/sha256_hash.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

TEST(Sha256Hmac, WikipediaString) {
  // https://en.wikipedia.org/wiki/HMAC lists these values, but you can also get
  // them using:
  //    echo -n "The quick brown fox jumps over the lazy dog" |
  //    openssl dgst -sha256 -hex -mac HMAC -macopt key:key
  auto data = std::string{"The quick brown fox jumps over the lazy dog"};
  // Compare against the HEX-encoded value just because they are easier to read.
  auto const expected = std::string{
      "f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8"};
  EXPECT_EQ(expected, HexEncode(Sha256Hmac("key", data)));
  EXPECT_EQ(expected, HexEncode(Sha256Hmac("key", absl::Span<char>(data))));
  EXPECT_EQ(expected,
            HexEncode(Sha256Hmac("key", absl::Span<char const>(data))));
  std::vector<std::uint8_t> v(data.begin(), data.end());
  EXPECT_EQ(expected,
            HexEncode(Sha256Hmac("key", absl::Span<std::uint8_t>(v))));
  EXPECT_EQ(expected,
            HexEncode(Sha256Hmac("key", absl::Span<std::uint8_t const>(v))));
}

TEST(Sha256Hmac, Rehash) {
  // echo -n "The quick brown fox jumps over the lazy dog" |
  //     openssl dgst -sha256 -mac HMAC -macopt ...
  //     hexkey:f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8
  // SHA2-256(stdin)=
  // f61f354e69e4e35dd936913f098993f4a07254ef9c156e26842f07e998d6e61e
  auto data = std::string{"The quick brown fox jumps over the lazy dog"};
  // Compare against the HEX-encoded value just because they are easier to read.
  auto const expected = std::string{
      "f61f354e69e4e35dd936913f098993f4a07254ef9c156e26842f07e998d6e61e"};
  auto key = Sha256Hmac("key", data);
  EXPECT_EQ(expected, HexEncode(Sha256Hmac(key, data)));
  EXPECT_EQ(expected, HexEncode(Sha256Hmac(key, absl::Span<char>(data))));
  EXPECT_EQ(expected, HexEncode(Sha256Hmac(key, absl::Span<char const>(data))));
  std::vector<std::uint8_t> v(data.begin(), data.end());
  EXPECT_EQ(expected, HexEncode(Sha256Hmac(key, absl::Span<std::uint8_t>(v))));
  EXPECT_EQ(expected,
            HexEncode(Sha256Hmac(key, absl::Span<std::uint8_t const>(v))));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
