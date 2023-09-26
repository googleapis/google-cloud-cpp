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

using ::testing::IsEmpty;
using ::testing::Not;

TEST(WritePayloadImpl, Default) {
  auto const actual = storage_experimental::WritePayload();
  EXPECT_TRUE(actual.empty());
  EXPECT_EQ(actual.size(), 0);
  EXPECT_THAT(actual.payload(), IsEmpty());
}

TEST(WritePayloadImpl, Make) {
  auto const expected =
      std::string{"The quick brown fox jumps over the lazy dog"};
  auto const actual = WritePayloadImpl::Make(absl::Cord(expected));
  EXPECT_FALSE(actual.empty());
  EXPECT_EQ(actual.size(), expected.size());
  EXPECT_THAT(actual.payload(), Not(IsEmpty()));
  auto full = absl::StrJoin(actual.payload(), "");
  EXPECT_EQ(full, expected);
}

TEST(WritePayloadImpl, GetImpl) {
  auto const expected =
      std::string{"The quick brown fox jumps over the lazy dog"};
  auto const actual = WritePayloadImpl::Make(absl::Cord(expected));
  EXPECT_EQ(WritePayloadImpl::GetImpl(actual), absl::Cord(expected));
}

template <typename T>
class Typed : public ::testing::Test {
 public:
  using TestType = T;
};

using TestVariations = ::testing::Types<
#if GOOGLE_CLOUD_CPP_CPP_VERSION >= 201703L
    std::vector<std::byte>,
#endif
    std::string, std::vector<char>, std::vector<signed char>,
    std::vector<unsigned char>, std::vector<std::uint8_t> >;

TYPED_TEST_SUITE(Typed, TestVariations);

TYPED_TEST(Typed, MakeWritePayload) {
  using Collection = typename TestFixture::TestType;

  auto const expected =
      std::string{"The quick brown fox jumps over the lazy dog"};

  auto const actual =
      MakeWritePayload(internal::RandomDataToCollection<Collection>(expected));
  EXPECT_FALSE(actual.empty());
  EXPECT_EQ(actual.size(), expected.size());
  EXPECT_THAT(actual.payload(), Not(IsEmpty()));
  auto full = absl::StrJoin(actual.payload(), "");
  EXPECT_EQ(full, expected);
}

TYPED_TEST(Typed, MakeWritePayloadFromVector) {
  using Collection = typename TestFixture::TestType;

  static auto generator = internal::MakeDefaultPRNG();
  auto constexpr kTestDataSize = 1024;
  auto const part = internal::RandomData<std::string>(generator, kTestDataSize);
  auto const expected = part + part + part;
  auto const actual = MakeWritePayload(std::vector<Collection>{
      {internal::RandomDataToCollection<Collection>(part),
       internal::RandomDataToCollection<Collection>(part),
       internal::RandomDataToCollection<Collection>(part)}});
  EXPECT_FALSE(actual.empty());
  EXPECT_EQ(actual.size(), expected.size());
  EXPECT_THAT(actual.payload(), Not(IsEmpty()));
  auto full = absl::StrJoin(actual.payload(), "");
  EXPECT_EQ(full, expected);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
