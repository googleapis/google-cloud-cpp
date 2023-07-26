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

#include "google/cloud/internal/curl_writev.h"
#include <gmock/gmock.h>
#include <cstdio>
#include <vector>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

TEST(WriteVector, Simple) {
  auto const a = std::string(32, 'a');
  auto const b = std::string(32, 'b');
  auto tested =
      WriteVector({absl::Span<char const>(a), absl::Span<char const>(b)});

  auto buffer = std::vector<char>(a.size() + b.size(), 'c');
  auto offset = std::size_t{0};
  ASSERT_EQ(4, tested.MoveTo(absl::MakeSpan(buffer.data() + offset, 4)));
  offset += 4;
  ASSERT_EQ(32, tested.MoveTo(absl::MakeSpan(buffer.data() + offset, 32)));
  offset += 32;
  ASSERT_EQ(28, tested.MoveTo(absl::MakeSpan(buffer.data() + offset, 64)));
  offset += 28;
  EXPECT_EQ(std::string(buffer.begin(), buffer.end()), a + b);
}

TEST(WriteVector, Rewind) {
  auto const a = std::string(32, 'a');
  auto const b = std::string(32, 'b');
  auto tested =
      WriteVector({absl::Span<char const>(a), absl::Span<char const>(b)});

  auto buffer = std::vector<char>(a.size() + b.size(), 'c');
  auto offset = std::size_t{0};
  ASSERT_EQ(4, tested.MoveTo(absl::MakeSpan(buffer.data() + offset, 4)));
  offset += 4;
  ASSERT_EQ(32, tested.MoveTo(absl::MakeSpan(buffer.data() + offset, 32)));
  offset += 32;
  tested.Seek(16, SEEK_SET);
  offset = 16;
  ASSERT_EQ(32, tested.MoveTo(absl::MakeSpan(buffer.data() + offset, 32)));
  offset += 32;
  ASSERT_EQ(16, tested.MoveTo(absl::MakeSpan(buffer.data() + offset, 32)));
  EXPECT_EQ(std::string(buffer.begin(), buffer.end()), a + b);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
