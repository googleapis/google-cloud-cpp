// Copyright 2020 Google LLC
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

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <thread>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

class SlowReaderChunkIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {
 protected:
  void SetUp() override {
    // Too slow to run against production.
    if (!UsingEmulator()) GTEST_SKIP();
    bucket_name_ = google::cloud::internal::GetEnv(
                       "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME")
                       .value_or("");
    ASSERT_FALSE(bucket_name_.empty());
  }

  std::string bucket_name_;
};

TEST_F(SlowReaderChunkIntegrationTest, LongPauses) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  // Construct an object too large to fit in the first chunk.
  auto const read_size = 1024 * 1024L;
  auto const large_text = MakeRandomData(4 * read_size);
  StatusOr<ObjectMetadata> source_meta = client->InsertObject(
      bucket_name_, object_name, large_text, IfGenerationMatch(0));
  ASSERT_STATUS_OK(source_meta);
  ScheduleForDelete(*source_meta);

  // Create an iostream to read the object back. When running against the
  // emulator we can fail quickly by asking the emulator to break the stream
  // in the middle.
  auto make_reader = [this, object_name, &client](int64_t offset) {
    if (UsingEmulator()) {
      return client->ReadObject(
          bucket_name_, object_name,
          CustomHeader("x-goog-emulator-instructions", "return-broken-stream"),
          ReadFromOffset(offset));
    }
    return client->ReadObject(bucket_name_, object_name,
                              ReadFromOffset(offset));
  };

  ObjectReadStream stream = make_reader(0);

  auto slow_reader_period = std::chrono::seconds(UsingEmulator() ? 1 : 400);
  auto const period_increment = std::chrono::seconds(UsingEmulator() ? 5 : 60);
  auto const max_slow_reader_period = std::chrono::minutes(10);
  std::vector<char> buffer;
  std::size_t const size = 1024 * 1024;
  buffer.resize(size);
  stream.read(buffer.data(), size);
  EXPECT_STATUS_OK(stream.status());

  std::cout << "Reading " << std::flush;
  std::int64_t offset = 0;
  while (!stream.eof()) {
    std::cout << ' ' << slow_reader_period.count() << "s (" << offset << ")"
              << std::flush;
    std::this_thread::sleep_for(slow_reader_period);
    stream.read(buffer.data(), size);
    if (!stream.status().ok()) {
      std::cout << " restart after (" << stream.status() << ")" << std::flush;
      stream = make_reader(offset);
      continue;
    }
    offset += stream.gcount();
    EXPECT_STATUS_OK(stream.status());
    if (slow_reader_period < max_slow_reader_period) {
      slow_reader_period += period_increment;
    }
  }
  std::cout << " DONE\n";
  EXPECT_STATUS_OK(stream.status());

  stream.Close();
  EXPECT_STATUS_OK(stream.status());
}

}  // anonymous namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
