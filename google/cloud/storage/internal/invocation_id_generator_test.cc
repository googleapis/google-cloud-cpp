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

#include "google/cloud/storage/internal/invocation_id_generator.h"
#include "absl/strings/str_split.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

using ::testing::AnyOf;
using ::testing::ElementsAre;
using ::testing::SizeIs;

std::vector<std::string> GenerateTestIds(InvocationIdGenerator& a) {
  std::vector<std::string> actual(128);
  std::generate(actual.begin(), actual.end(),
                [&] { return a.MakeInvocationId(); });
  return actual;
}

TEST(InvocationIdGenerator, Basic) {
  InvocationIdGenerator a;
  for (int i = 0; i != 128; ++i) {
    auto const id = a.MakeInvocationId();
    ASSERT_EQ(id.size(), 36);
    EXPECT_EQ(id[14], '4');                // Version
    EXPECT_THAT(id[19], AnyOf('2', '3'));  // Variant + RandomBit
    std::vector<std::string> components = absl::StrSplit(id, '-');
    ASSERT_THAT(components, ElementsAre(SizeIs(8), SizeIs(4), SizeIs(4),
                                        SizeIs(4), SizeIs(12)));
    EXPECT_EQ(id.find_first_not_of("-0123456789abcdef"), std::string::npos)
        << "id=" << id;
  }
}

TEST(InvocationIdGenerator, Unique) {
  InvocationIdGenerator a;
  auto actual = GenerateTestIds(a);
  std::sort(actual.begin(), actual.end());
  auto f = std::adjacent_find(actual.begin(), actual.end());
  EXPECT_TRUE(f == actual.end()) << "Duplicate=" << *f;
}

TEST(InvocationIdGenerator, TwoGenerators) {
  InvocationIdGenerator a;
  InvocationIdGenerator b;
  auto aout = GenerateTestIds(a);
  auto bout = GenerateTestIds(b);
  auto actual = aout;
  actual.insert(actual.end(), bout.begin(), bout.end());
  std::sort(actual.begin(), actual.end());
  auto f = std::adjacent_find(actual.begin(), actual.end());
  EXPECT_TRUE(f == actual.end()) << "Duplicate=" << *f;
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
