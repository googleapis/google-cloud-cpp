// Copyright 2018 Google LLC
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

#include "google/cloud/storage/internal/generate_message_boundary.h"
#include "google/cloud/internal/random.h"
#include <gmock/gmock.h>
#include <algorithm>
#include <cctype>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Return;

char constexpr kChars[] =
    "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

TEST(GenerateMessageBoundaryTest, Candidate) {
  auto constexpr kMaxBoundarySize = 70;
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());

  for (int i = 0; i != 10; ++i) {
    auto const candidate = GenerateMessageBoundaryCandidate(generator);
    ASSERT_THAT(candidate, Not(IsEmpty()));
    EXPECT_LE(candidate.size(), kMaxBoundarySize);
    auto f = std::find_if(candidate.begin(), candidate.end(),
                          [](char c) { return std::isalnum(c) == 0; });
    EXPECT_TRUE(f == candidate.end())
        << "found non-alphanumeric character in " << candidate;
  }
}

TEST(GenerateMessageBoundaryTest, Simple) {
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());

  // The magic constants here are uninteresting. We just want a large message
  // and a relatively short string to start searching for a boundary.
  auto message = google::cloud::internal::Sample(generator, 128, kChars);
  for (int i = 0; i != 10; ++i) {
    auto boundary = GenerateMessageBoundary(message, [&generator]() {
      return GenerateMessageBoundaryCandidate(generator);
    });
    EXPECT_THAT(message, Not(HasSubstr(boundary)));
  }
}

TEST(GenerateMessageBoundaryTest, RequiresMoreCandidates) {
  ::testing::MockFunction<std::string()> candidate_generator;
  EXPECT_CALL(candidate_generator, Call)
      .WillOnce(Return("abc"))
      .WillOnce(Return("abcd"))
      .WillOnce(Return("good"));

  auto const actual = GenerateMessageBoundary(
      "abc123abcd", candidate_generator.AsStdFunction());
  EXPECT_EQ(actual, "good");
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
