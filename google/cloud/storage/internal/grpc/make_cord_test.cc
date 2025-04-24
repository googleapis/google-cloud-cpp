// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/internal/async/write_payload_impl.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/random.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::ElementsAre;

TEST(MakeCord, IsPayloadType) {
  EXPECT_TRUE(IsPayloadType<char>::value);
  EXPECT_TRUE(IsPayloadType<std::uint8_t>::value);
  EXPECT_FALSE(IsPayloadType<std::string>::value);
  EXPECT_FALSE(IsPayloadType<std::uint32_t>::value);
}

TEST(MakeCord, FromString) {
  auto const input = std::string("The quick brown fox jumps over the lazy dog");
  auto actual = MakeCord(input);
  EXPECT_EQ(static_cast<std::string>(actual), input);
}

TEST(MakeCord, FromStringLong) {
  auto const input = std::string(4 * 1024 * 1024, '\0');
  auto actual = MakeCord(input);
  std::vector<absl::string_view> chunks{actual.chunk_begin(),
                                        actual.chunk_end()};
  EXPECT_THAT(chunks, ElementsAre(absl::string_view(input)));
}

TEST(MakeCord, FromStringVector) {
  auto const a = std::string(1024, 'a');
  auto const b = std::string(2048, 'b');
  auto const c = std::string(4096, 'c');
  auto actual = MakeCord(std::vector<std::string>{a, b, c});
  std::vector<absl::string_view> chunks{actual.chunk_begin(),
                                        actual.chunk_end()};
  EXPECT_THAT(chunks, ElementsAre(absl::string_view(a), absl::string_view(b),
                                  absl::string_view(c)));
}

template <typename T>
class MakeCordFromVector : public ::testing::Test {
 public:
  using TestType = T;
};

using TestVariations = ::testing::Types<
#if GOOGLE_CLOUD_CPP_CPP_VERSION >= 201703L
    std::vector<std::byte>,
#endif
    std::string, std::vector<char>, std::vector<signed char>,
    std::vector<unsigned char>, std::vector<std::uint8_t> >;

TYPED_TEST_SUITE(MakeCordFromVector, TestVariations, );

TYPED_TEST(MakeCordFromVector, MakeCord) {
  using Collection = typename TestFixture::TestType;
  static auto generator = internal::MakeDefaultPRNG();
  auto constexpr kTestDataSize = 8 * 1024 * 1024L;
  auto const buffer =
      internal::RandomData<Collection>(generator, kTestDataSize);
  auto const view = absl::string_view(
      reinterpret_cast<char const*>(buffer.data()), buffer.size());

  auto const actual = MakeCord(buffer);
  std::vector<absl::string_view> chunks{actual.chunk_begin(),
                                        actual.chunk_end()};
  EXPECT_THAT(chunks, ElementsAre(view));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
