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

#include "google/cloud/storage/internal/grpc/split_write_object_data.h"
#include "google/cloud/internal/random.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::internal::DefaultPRNG;
using ::google::cloud::storage::internal::ConstBuffer;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;

auto constexpr kExpectedChunkSize = 2 * 1024 * 1024L;

std::string RandomData(DefaultPRNG& generator, std::size_t size) {
  return google::cloud::internal::Sample(
      generator, static_cast<int>(size),
      "abcdefghijklmnopqrstuvwxyz0123456789");
}

TEST(SplitWriteObjectRequestString, SingleString) {
  auto generator = DefaultPRNG(std::random_device{}());
  auto const data = RandomData(generator, kExpectedChunkSize / 2);
  auto tested = SplitObjectWriteData<std::string>(data);
  ASSERT_FALSE(tested.Done());
  auto const b = tested.Next();
  EXPECT_THAT(b, ElementsAreArray(data));
  ASSERT_TRUE(tested.Done());
}

TEST(SplitWriteObjectRequestString, MultipleString) {
  auto generator = DefaultPRNG(std::random_device{}());
  auto const data =
      RandomData(generator, 2 * kExpectedChunkSize + kExpectedChunkSize / 2);
  auto tested = SplitObjectWriteData<std::string>(data);
  std::vector<std::string> actual;
  while (!tested.Done()) actual.push_back(tested.Next());
  EXPECT_THAT(actual,
              ElementsAre(data.substr(0, kExpectedChunkSize),
                          data.substr(kExpectedChunkSize, kExpectedChunkSize),
                          data.substr(2 * kExpectedChunkSize)));
}

TEST(SplitWriteObjectRequestString, SingleBuffer) {
  auto generator = DefaultPRNG(std::random_device{}());
  auto const data = RandomData(generator, kExpectedChunkSize / 2);
  auto tested = SplitObjectWriteData<std::string>({ConstBuffer(data)});
  ASSERT_FALSE(tested.Done());
  auto const b = tested.Next();
  EXPECT_THAT(b, ElementsAreArray(data));
  ASSERT_TRUE(tested.Done());
}

TEST(SplitWriteObjectRequestString, MultipleBuffer) {
  auto generator = DefaultPRNG(std::random_device{}());
  auto const d0 = RandomData(generator, kExpectedChunkSize / 2);
  auto const d1 = RandomData(generator, kExpectedChunkSize);
  auto const d2 = RandomData(generator, kExpectedChunkSize);
  auto const d3 = RandomData(generator, kExpectedChunkSize);
  auto tested = SplitObjectWriteData<std::string>(
      {ConstBuffer(d0), ConstBuffer(d1), ConstBuffer(d2), ConstBuffer(d3)});
  std::vector<std::string> actual;
  while (!tested.Done()) actual.push_back(tested.Next());
  auto const data = d0 + d1 + d2 + d3;
  EXPECT_THAT(
      actual,
      ElementsAre(data.substr(0, kExpectedChunkSize),
                  data.substr(kExpectedChunkSize, kExpectedChunkSize),
                  data.substr(2 * kExpectedChunkSize, kExpectedChunkSize),
                  data.substr(3 * kExpectedChunkSize)));
}

TEST(SplitWriteObjectRequestCord, SingleString) {
  auto generator = DefaultPRNG(std::random_device{}());
  auto const data = RandomData(generator, kExpectedChunkSize / 2);
  auto tested = SplitObjectWriteData<absl::Cord>(data);
  ASSERT_FALSE(tested.Done());
  auto const b = tested.Next();
  EXPECT_THAT(std::string(b), ElementsAreArray(data));
  ASSERT_TRUE(tested.Done());
}

TEST(SplitWriteObjectRequestCord, MultipleString) {
  auto generator = DefaultPRNG(std::random_device{}());
  auto const data =
      RandomData(generator, 2 * kExpectedChunkSize + kExpectedChunkSize / 2);
  auto tested = SplitObjectWriteData<absl::Cord>(data);
  std::vector<std::string> actual;
  while (!tested.Done()) actual.emplace_back(tested.Next());
  EXPECT_THAT(actual,
              ElementsAre(data.substr(0, kExpectedChunkSize),
                          data.substr(kExpectedChunkSize, kExpectedChunkSize),
                          data.substr(2 * kExpectedChunkSize)));
}

TEST(SplitWriteObjectRequestCord, SingleBuffer) {
  auto generator = DefaultPRNG(std::random_device{}());
  auto const data = RandomData(generator, kExpectedChunkSize / 2);
  auto tested = SplitObjectWriteData<absl::Cord>({ConstBuffer(data)});
  ASSERT_FALSE(tested.Done());
  auto const b = tested.Next();
  EXPECT_THAT(std::string(b), ElementsAreArray(data));
  ASSERT_TRUE(tested.Done());
}

TEST(SplitWriteObjectRequestCord, MultipleBuffer) {
  auto generator = DefaultPRNG(std::random_device{}());
  auto const d0 = RandomData(generator, kExpectedChunkSize / 2);
  auto const d1 = RandomData(generator, kExpectedChunkSize);
  auto const d2 = RandomData(generator, kExpectedChunkSize);
  auto const d3 = RandomData(generator, kExpectedChunkSize);
  auto tested = SplitObjectWriteData<absl::Cord>(
      {ConstBuffer(d0), ConstBuffer(d1), ConstBuffer(d2), ConstBuffer(d3)});
  std::vector<std::string> actual;
  while (!tested.Done()) actual.emplace_back(tested.Next());
  auto const data = d0 + d1 + d2 + d3;
  EXPECT_THAT(
      actual,
      ElementsAre(data.substr(0, kExpectedChunkSize),
                  data.substr(kExpectedChunkSize, kExpectedChunkSize),
                  data.substr(2 * kExpectedChunkSize, kExpectedChunkSize),
                  data.substr(3 * kExpectedChunkSize)));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
