// Copyright 2021 Google LLC
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

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::internal::GetEnv;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;

class ObjectReadRangeIntegrationTest
    : public ::google::cloud::storage::testing::StorageIntegrationTest {
 public:
  ObjectReadRangeIntegrationTest() = default;

 protected:
  void SetUp() override {
    bucket_name_ =
        GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME").value_or("");

    ASSERT_FALSE(bucket_name_.empty());
  }

  std::string const& bucket_name() const { return bucket_name_; }

 private:
  std::string bucket_name_;
};

TEST_F(ObjectReadRangeIntegrationTest, ReadRanges) {
  auto client = MakeIntegrationTestClient();

  auto constexpr kChunk = 1000;
  auto constexpr kObjectSize = 10 * kChunk;
  auto const contents = MakeRandomData(kObjectSize);
  // Read different ranges in the object, expecting specific results.
  struct Test {
    std::int64_t begin;
    std::int64_t end;
    std::string expected;
  } cases[] = {
      {0, kChunk, contents.substr(0, kChunk)},
      {kChunk, 2 * kChunk, contents.substr(kChunk, kChunk)},
      {0, 20 * kChunk, contents},
      {8 * kChunk, 12 * kChunk, contents.substr(8 * kChunk)},
  };

  auto const object_name = MakeRandomObjectName();
  auto insert = client.InsertObject(bucket_name(), object_name, contents,
                                    IfGenerationMatch(0));
  ASSERT_THAT(insert, IsOk());
  ScheduleForDelete(*insert);
  EXPECT_THAT(contents.size(), insert->size());

  for (auto const& test : cases) {
    SCOPED_TRACE("Testing range [" + std::to_string(test.begin) + "," +
                 std::to_string(test.end) + ")");
    auto reader = client.ReadObject(bucket_name(), object_name,
                                    ReadRange(test.begin, test.end));
    EXPECT_FALSE(reader.bad());
    EXPECT_FALSE(reader.eof());
    EXPECT_FALSE(reader.fail());
    EXPECT_TRUE(reader.good());

    auto buffer = std::vector<char>(2 * kObjectSize);
    reader.read(buffer.data(), buffer.size());
    EXPECT_FALSE(reader.bad());
    EXPECT_TRUE(reader.eof());
    EXPECT_TRUE(reader.fail());
    EXPECT_FALSE(reader.good());
    EXPECT_THAT(reader.status(), IsOk());

    auto actual =
        std::string{buffer.begin(),
                    std::next(buffer.begin(),
                              static_cast<std::ptrdiff_t>(reader.gcount()))};
    EXPECT_EQ(test.expected, actual);
  }

  if (UsingEmulator()) return;
  auto reader = client.ReadObject(
      bucket_name(), object_name,
      ReadRange(kObjectSize + kChunk, kObjectSize + 2 * kChunk));
  EXPECT_TRUE(reader.bad());
  EXPECT_THAT(reader.status(), StatusIs(StatusCode::kOutOfRange));
}

TEST_F(ObjectReadRangeIntegrationTest, ReadFromOffset) {
  auto client = MakeIntegrationTestClient();

  auto constexpr kChunk = 1000;
  auto constexpr kObjectSize = 10 * kChunk;
  auto const contents = MakeRandomData(kObjectSize);
  // Read from different offsets in the object, expecting specific results.
  struct Test {
    std::int64_t begin;
    std::string expected;
  } cases[] = {
      {0, contents},
      {kChunk, contents.substr(kChunk)},
      {8 * kChunk, contents.substr(8 * kChunk)},
  };

  auto const object_name = MakeRandomObjectName();
  auto insert = client.InsertObject(bucket_name(), object_name, contents,
                                    IfGenerationMatch(0));
  ASSERT_THAT(insert, IsOk());
  ScheduleForDelete(*insert);
  EXPECT_THAT(contents.size(), insert->size());

  for (auto const& test : cases) {
    SCOPED_TRACE("Testing from offset " + std::to_string(test.begin));
    auto reader = client.ReadObject(bucket_name(), object_name,
                                    ReadFromOffset(test.begin));
    EXPECT_FALSE(reader.bad());
    EXPECT_FALSE(reader.eof());
    EXPECT_FALSE(reader.fail());
    EXPECT_TRUE(reader.good());

    auto buffer = std::vector<char>(2 * kObjectSize);
    reader.read(buffer.data(), buffer.size());
    EXPECT_FALSE(reader.bad());
    EXPECT_TRUE(reader.eof());
    EXPECT_TRUE(reader.fail());
    EXPECT_FALSE(reader.good());
    EXPECT_THAT(reader.status(), IsOk());

    auto actual =
        std::string{buffer.begin(),
                    std::next(buffer.begin(),
                              static_cast<std::ptrdiff_t>(reader.gcount()))};
    EXPECT_EQ(test.expected, actual);
  }

  if (UsingEmulator()) return;
  auto reader = client.ReadObject(bucket_name(), object_name,
                                  ReadFromOffset(kObjectSize + kChunk));
  EXPECT_TRUE(reader.bad());
  EXPECT_THAT(reader.status(), StatusIs(StatusCode::kOutOfRange));
}

TEST_F(ObjectReadRangeIntegrationTest, ReadLast) {
  auto client = MakeIntegrationTestClient();

  auto constexpr kChunk = 1000;
  auto constexpr kObjectSize = 10 * kChunk;
  auto const contents = MakeRandomData(kObjectSize);
  // Read the last part(s) of the object, expecting specific results.
  struct Test {
    std::int64_t count;
    std::string expected;
  } cases[] = {
      {kObjectSize, contents},
      {kChunk, contents.substr(contents.size() - kChunk)},
      {2 * kChunk, contents.substr(contents.size() - 2 * kChunk)},
      // GCS returns the minimum of "the last N bytes" or "all the bytes".
      {kObjectSize + kChunk, contents},
  };

  auto const object_name = MakeRandomObjectName();
  auto insert = client.InsertObject(bucket_name(), object_name, contents,
                                    IfGenerationMatch(0));
  ASSERT_THAT(insert, IsOk());
  ScheduleForDelete(*insert);
  EXPECT_THAT(contents.size(), insert->size());

  for (auto const& test : cases) {
    SCOPED_TRACE("Testing last " + std::to_string(test.count));
    auto reader =
        client.ReadObject(bucket_name(), object_name, ReadLast(test.count));
    EXPECT_FALSE(reader.bad());
    EXPECT_FALSE(reader.eof());
    EXPECT_FALSE(reader.fail());
    EXPECT_TRUE(reader.good());

    auto buffer = std::vector<char>(2 * kObjectSize);
    reader.read(buffer.data(), buffer.size());
    EXPECT_FALSE(reader.bad());
    EXPECT_TRUE(reader.eof());
    EXPECT_TRUE(reader.fail());
    EXPECT_FALSE(reader.good());
    EXPECT_THAT(reader.status(), IsOk());

    auto actual =
        std::string{buffer.begin(),
                    std::next(buffer.begin(),
                              static_cast<std::ptrdiff_t>(reader.gcount()))};
    EXPECT_EQ(test.expected, actual);
  }
}

TEST_F(ObjectReadRangeIntegrationTest, ReadRangeSmall) {
  auto client = MakeIntegrationTestClient();

  auto const contents = LoremIpsum();
  auto const object_name = MakeRandomObjectName();

  auto insert = client.InsertObject(bucket_name(), object_name, contents,
                                    IfGenerationMatch(0));
  ASSERT_THAT(insert, IsOk());
  ScheduleForDelete(*insert);
  EXPECT_THAT(contents.size(), insert->size());

  auto const size = static_cast<std::int64_t>(insert->size());

  // Read several small portions of the object, expecting specific results.
  struct Test {
    std::int64_t begin;
    std::int64_t end;
    std::string expected;
  } cases[] = {
      {0, 1, contents.substr(0, 1)},
      {4, 8, contents.substr(4, 4)},
      {0, size, contents},
  };

  for (auto const& test : cases) {
    SCOPED_TRACE("Testing range [" + std::to_string(test.begin) + "," +
                 std::to_string(test.end) + ")");
    auto reader = client.ReadObject(bucket_name(), object_name,
                                    ReadRange(test.begin, test.end));
    auto actual = std::string{std::istreambuf_iterator<char>(reader), {}};
    EXPECT_THAT(reader.status(), IsOk());
    EXPECT_EQ(test.expected, actual);
  }
}

TEST_F(ObjectReadRangeIntegrationTest, ReadFromOffsetSmall) {
  auto client = MakeIntegrationTestClient();

  auto const contents = LoremIpsum();
  auto const object_name = MakeRandomObjectName();

  auto insert = client.InsertObject(bucket_name(), object_name, contents,
                                    IfGenerationMatch(0));
  ASSERT_THAT(insert, IsOk());
  ScheduleForDelete(*insert);
  EXPECT_THAT(contents.size(), insert->size());

  auto const size = static_cast<std::int64_t>(insert->size());

  // Read several small portions of the object, expecting specific results.
  struct Test {
    std::int64_t offset;
    std::string expected;
  } cases[] = {
      {0, contents.substr(0)},
      {4, contents.substr(4)},
      {size - 1, contents.substr(contents.size() - 1)},
  };

  for (auto const& test : cases) {
    SCOPED_TRACE("Testing range [" + std::to_string(test.offset) + ",end)");
    auto reader = client.ReadObject(bucket_name(), object_name,
                                    ReadFromOffset(test.offset));
    auto actual = std::string{std::istreambuf_iterator<char>(reader), {}};
    EXPECT_THAT(reader.status(), IsOk());
    EXPECT_EQ(test.expected, actual);
  }
}

TEST_F(ObjectReadRangeIntegrationTest, ReadLastSmall) {
  auto client = MakeIntegrationTestClient();

  auto const contents = LoremIpsum();
  auto const object_name = MakeRandomObjectName();

  auto insert = client.InsertObject(bucket_name(), object_name, contents,
                                    IfGenerationMatch(0));
  ASSERT_THAT(insert, IsOk());
  ScheduleForDelete(*insert);
  EXPECT_THAT(contents.size(), insert->size());

  auto const size = static_cast<std::int64_t>(insert->size());

  // Read several small portions of the object, expecting specific results.
  struct Test {
    std::int64_t offset;
    std::string expected;
  } cases[] = {
      {1, contents.substr(contents.size() - 1)},
      {4, contents.substr(contents.size() - 4)},
      {size, contents},
  };

  for (auto const& test : cases) {
    SCOPED_TRACE("Testing range [-" + std::to_string(test.offset) + ",end)");
    auto reader =
        client.ReadObject(bucket_name(), object_name, ReadLast(test.offset));
    auto actual = std::string{std::istreambuf_iterator<char>(reader), {}};
    EXPECT_THAT(reader.status(), IsOk());
    EXPECT_EQ(test.expected, actual);
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
