// Copyright 2018 Google LLC
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

#include "google/cloud/storage/internal/generate_message_boundary.h"
#include "google/cloud/internal/random.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

using ::testing::HasSubstr;
using ::testing::Not;

TEST(GenerateMessageBoundaryTest, Simple) {
  auto generator = google::cloud::internal::MakeDefaultPRNG();

  auto string_generator = [&generator](int n) {
    static std::string const kChars =
        "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    return google::cloud::internal::Sample(generator, n, kChars);
  };

  // The magic constants here are uninteresting. We just want a large message
  // and a relatively short string to start searching for a boundary.
  auto message = string_generator(1024);
  auto boundary =
      GenerateMessageBoundary(message, std::move(string_generator), 16, 4);
  EXPECT_THAT(message, Not(HasSubstr(boundary)));
}

TEST(GenerateMessageBoundaryTest, RequiresGrowth) {
  static std::string const kChars =
      "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

  auto generator = google::cloud::internal::MakeDefaultPRNG();

  // This test will ensure that both the message and the initial string contain
  // at least this many common characters.
  int constexpr kMatchedStringLength = 32;
  int constexpr kMismatchedStringLength = 512;

  auto g1 = google::cloud::internal::MakeDefaultPRNG();
  std::string message =
      google::cloud::internal::Sample(g1, kMismatchedStringLength, kChars);
  // Copy the PRNG to obtain the same sequence of random numbers that
  // `generator` will create later.
  g1 = generator;
  message += google::cloud::internal::Sample(g1, kMatchedStringLength, kChars);
  g1 = google::cloud::internal::MakeDefaultPRNG();
  message +=
      google::cloud::internal::Sample(g1, kMismatchedStringLength, kChars);

  auto string_generator = [&generator](int n) {
    return google::cloud::internal::Sample(generator, n, kChars);
  };

  // The initial_size and growth_size parameters are set to
  // (kMatchedStringLength / 2) and (kMatchedStringLength / 4) respectively,
  // that forces the algorithm to find the initial string, and to grow it
  // several times before the kMatchedStringLength common characters are
  // exhausted.
  auto boundary = GenerateMessageBoundary(message, std::move(string_generator),
                                          kMatchedStringLength / 2,
                                          kMatchedStringLength / 4);
  EXPECT_THAT(message, Not(HasSubstr(boundary)));

  // We expect that the string is longer than the common characters.
  EXPECT_LT(kMatchedStringLength, boundary.size());
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
