// Copyright 2020 Google LLC
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
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <thread>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

class SmallReadsIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {
 protected:
  void SetUp() override {
    bucket_name_ = google::cloud::internal::GetEnv(
                       "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME")
                       .value_or("");
    ASSERT_FALSE(bucket_name_.empty());
  }

  std::string bucket_name_;
};

/// @test This is a repro for #5096, the download should not stall
TEST_F(SmallReadsIntegrationTest, Repro5096) {
  auto client = MakeIntegrationTestClient();
  auto object_name = MakeRandomObjectName();

  // Create an object with enough data to repro the problem.
  auto constexpr kRandomDataBlock = 128 * 1024;
  auto constexpr kBlockCount = 32;
  auto constexpr kReadSize = 4096;

  auto const block = MakeRandomData(kRandomDataBlock);
  auto writer =
      client.WriteObject(bucket_name_, object_name, IfGenerationMatch(0));
  for (int i = 0; i != kBlockCount; ++i) {
    writer.write(block.data(), block.size());
  }
  writer.Close();
  ASSERT_STATUS_OK(writer.metadata());
  ScheduleForDelete(*writer.metadata());

  auto reader = client.ReadObject(bucket_name_, object_name);

  using std::chrono::duration_cast;
  using std::chrono::milliseconds;
  using std::chrono::steady_clock;
  // When #5096 was triggered some of the read calls take 120 seconds, normally
  // they would take a few milliseconds. I (coryan) think that 10 seconds is
  // (a) large enough to avoid flakiness due to weird scheduling, and (b) small
  // enough to detect a regression of #5096.
  auto const tolerance = std::chrono::seconds(10);
  auto last = steady_clock::now();
  std::streamoff offset = 0;
  while (reader) {
    std::vector<char> buffer(kReadSize);
    reader.read(buffer.data(), buffer.size());
    auto const ts = steady_clock::now();
    auto const elapsed = duration_cast<milliseconds>(ts - last);
    EXPECT_LE(elapsed, tolerance) << "offset=" << offset;
    last = ts;
    offset += reader.gcount();
  }
}

TEST_F(SmallReadsIntegrationTest, ReadFullSingleRead) {
  auto client = MakeIntegrationTestClient();
  auto object_name = MakeRandomObjectName();
  auto const contents = LoremIpsum();

  // Create a small object, read it all in the first .read() call
  auto meta = client.InsertObject(bucket_name_, object_name, contents,
                                  IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  std::vector<char> buffer(2 * contents.size());
  auto reader = client.ReadObject(bucket_name_, object_name);
  reader.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
  ASSERT_GT(reader.gcount(), 0);
  auto const size = static_cast<std::size_t>(reader.gcount());
  EXPECT_TRUE(reader.eof());
  EXPECT_TRUE(reader.fail());
  EXPECT_FALSE(reader.bad());
  ASSERT_STATUS_OK(reader.status());
  EXPECT_EQ(contents, std::string(buffer.data(), size));
}

TEST_F(SmallReadsIntegrationTest, ReadFullByChar) {
  auto client = MakeIntegrationTestClient();
  auto object_name = MakeRandomObjectName();

  // Create a small object, read it all in the first .read() call
  auto meta = client.InsertObject(bucket_name_, object_name, LoremIpsum(),
                                  IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  auto reader = client.ReadObject(bucket_name_, object_name);
  auto actual = std::string{std::istreambuf_iterator<char>(reader), {}};
  ASSERT_STATUS_OK(reader.status());
  EXPECT_FALSE(reader.eof());
  EXPECT_FALSE(reader.fail());
  EXPECT_TRUE(reader.good());
  EXPECT_EQ(LoremIpsum(), actual);
}

}  // anonymous namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
