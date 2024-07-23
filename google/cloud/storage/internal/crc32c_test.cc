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

#include "google/cloud/storage/internal/crc32c.h"
#include <gmock/gmock.h>
#include <algorithm>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::internal::ConstBuffer;
using ::google::cloud::storage::internal::ConstBufferSequence;

TEST(Crc32c, Empty) {
  auto const expected = std::uint32_t{0};
  EXPECT_EQ(expected, Crc32c(absl::string_view{}));
  EXPECT_EQ(expected, Crc32c(absl::Cord{}));
  EXPECT_EQ(expected, Crc32c(ConstBufferSequence{}));
  EXPECT_EQ(expected,
            Crc32c(ConstBufferSequence{ConstBuffer{}, ConstBuffer{}}));
}

TEST(Crc32c, Quick) {
  auto const expected = std::uint32_t{0x22620404};
  auto const input = std::string("The quick brown fox jumps over the lazy dog");
  EXPECT_EQ(expected, Crc32c(absl::string_view(input)));
  EXPECT_EQ(expected, Crc32c(absl::Cord(input)));
  EXPECT_EQ(expected, Crc32c(ConstBufferSequence{ConstBuffer(input)}));
}

TEST(Crc32c, ExtendNotPrecomputedStringView) {
  auto const expected = std::uint32_t{0x22620404};
  std::vector<std::string> const inputs{"The",  " quick", " brown",
                                        " fox", " jumps", " over",
                                        " the", " lazy",  " dog"};
  absl::Cord data;
  for (auto const& input : inputs) {
    data.Append(input);
  }
  EXPECT_EQ(expected, Crc32c(data));
}

TEST(Crc32c, ExtendNotPrecomputedCord) {
  auto const expected = std::uint32_t{0x22620404};
  std::vector<std::string> const inputs{"The",  " quick", " brown",
                                        " fox", " jumps", " over",
                                        " the", " lazy",  " dog"};
  auto crc = std::uint32_t{0};
  for (auto const& input : inputs) {
    crc = ExtendCrc32c(crc, absl::Cord{input});
  }
  EXPECT_EQ(expected, crc);
}

TEST(Crc32c, ExtendNotPrecomputedConstBuffer) {
  auto const expected = std::uint32_t{0x22620404};
  std::vector<std::string> const inputs{"The",  " quick", " brown",
                                        " fox", " jumps", " over",
                                        " the", " lazy",  " dog"};
  auto crc = std::uint32_t{0};
  for (auto const& input : inputs) {
    crc = ExtendCrc32c(crc, {ConstBuffer{input}});
  }
  EXPECT_EQ(expected, crc);
}

TEST(Crc32c, ExtendNotPrecomputedConstBufferFull) {
  auto const expected = std::uint32_t{0x22620404};
  std::vector<std::string> const inputs{"The",  " quick", " brown",
                                        " fox", " jumps", " over",
                                        " the", " lazy",  " dog"};
  ConstBufferSequence payload(inputs.size());
  std::transform(inputs.begin(), inputs.end(), payload.begin(),
                 [](auto const& s) { return ConstBuffer(s); });
  auto crc = std::uint32_t{0};
  crc = ExtendCrc32c(crc, payload);
  EXPECT_EQ(expected, crc);
}

TEST(Crc32c, ExtendPrecomputedStringView) {
  auto const expected = std::uint32_t{0x22620404};
  std::vector<std::string> const inputs{"The",  " quick", " brown",
                                        " fox", " jumps", " over",
                                        " the", " lazy",  " dog"};
  auto crc = std::uint32_t{0};
  for (auto const& input : inputs) {
    auto const input_crc = Crc32c(input);
    crc = ExtendCrc32c(crc, absl::string_view{input}, input_crc);
  }
  EXPECT_EQ(expected, crc);
}

TEST(Crc32c, ExtendPrecomputedConstBuffer) {
  auto const expected = std::uint32_t{0x22620404};
  std::vector<std::string> const inputs{"The",  " quick", " brown",
                                        " fox", " jumps", " over",
                                        " the", " lazy",  " dog"};
  auto crc = std::uint32_t{0};
  for (auto const& input : inputs) {
    auto const input_crc = Crc32c(input);
    crc = ExtendCrc32c(crc, {ConstBuffer{input}}, input_crc);
  }
  EXPECT_EQ(expected, crc);
}

TEST(Crc32c, ExtendPrecomputedConstBufferFull) {
  auto const expected = std::uint32_t{0x22620404};
  std::vector<std::string> const inputs{"The",  " quick", " brown",
                                        " fox", " jumps", " over",
                                        " the", " lazy",  " dog"};
  ConstBufferSequence payload(inputs.size());
  std::transform(inputs.begin(), inputs.end(), payload.begin(),
                 [](auto const& s) { return ConstBuffer(s); });
  auto crc = std::uint32_t{0};
  crc = ExtendCrc32c(crc, payload, expected);
  EXPECT_EQ(expected, crc);
}

TEST(Crc32c, ExtendPrecomputedCord) {
  auto const expected = std::uint32_t{0x22620404};
  std::vector<std::string> const inputs{"The",  " quick", " brown",
                                        " fox", " jumps", " over",
                                        " the", " lazy",  " dog"};
  auto crc = std::uint32_t{0};
  for (auto const& input : inputs) {
    auto const input_crc = Crc32c(input);
    crc = ExtendCrc32c(crc, absl::Cord{input}, input_crc);
  }
  EXPECT_EQ(expected, crc);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
