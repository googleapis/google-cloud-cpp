// Copyright 2021 Google LLC
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

#include "google/cloud/storage/internal/hash_values.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

TEST(HashValuesTest, Format) {
  struct Test {
    std::string expected;
    HashValues values;
  } cases[] = {
      {"", {"", ""}},
      {"md5-hash", {"", "md5-hash"}},
      {"crc32c-hash", {"crc32c-hash", ""}},
      {"crc32c=crc32c-hash, md5=md5-hash", {"crc32c-hash", "md5-hash"}},
  };

  for (auto const& test : cases) {
    EXPECT_EQ(test.expected, Format(test.values));
  }
}

TEST(HashValuesTest, Merge) {
  struct Test {
    std::string expected;
    HashValues a;
    HashValues b;
  } cases[] = {
      {"crc32c=crc32c-b, md5=md5-a", {"", "md5-a"}, {"crc32c-b", "md5-b"}},
      {"crc32c=crc32c-a, md5=md5-b", {"crc32c-a", ""}, {"crc32c-b", "md5-b"}},
      {"crc32c=crc32c-b, md5=md5-b", {"", ""}, {"crc32c-b", "md5-b"}},
  };

  for (auto const& test : cases) {
    EXPECT_EQ(test.expected, Format(Merge(test.a, test.b)));
  }
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
