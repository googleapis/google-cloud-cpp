// Copyright 2019 Google LLC
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

#include "google/cloud/internal/random.h"
#include "google/cloud/log.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/init_google_mock.h"
#include <gmock/gmock.h>
#include <thread>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::testing::HasSubstr;

// Initialized in main() below.
char const* flag_project_id;
char const* flag_bucket_name;

class SlowReaderIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {};

TEST_F(SlowReaderIntegrationTest, StreamingRead) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string const bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();
  auto file_name = MakeRandomObjectName();

  // Construct an object large enough to not be downloaded in the first chunk.
  auto large_text = MakeRandomData(4 * 1024 * 1024);

  // Create an object with the contents to download.
  StatusOr<ObjectMetadata> source_meta = client->InsertObject(
      bucket_name, object_name, large_text, IfGenerationMatch(0));
  ASSERT_STATUS_OK(source_meta);

  // Create an iostream to read the object back. When running against the
  // testbench we can fail quickly by asking the testbench to break the stream
  // in the middle.
  ObjectReadStream stream;
  auto slow_reader_period = std::chrono::seconds(400);
  if (UsingTestbench()) {
    stream = client->ReadObject(
        bucket_name, object_name,
        CustomHeader("x-goog-testbench-instructions", "return-broken-stream"));
    slow_reader_period = std::chrono::seconds(1);
  } else {
    stream = client->ReadObject(bucket_name, object_name);
  }

  auto const max_slow_reader_period = std::chrono::minutes(10);
  std::vector<char> buffer;
  long const size = 1024 * 1024;
  std::int64_t read_count = 0;
  buffer.resize(size);
  stream.read(buffer.data(), size);
  read_count += stream.gcount();
  EXPECT_STATUS_OK(stream.status());

  std::cout << "Reading " << std::flush;
  while (!stream.eof()) {
    std::cout << ' ' << slow_reader_period.count() << "s (" << read_count << ")"
              << std::flush;
    std::this_thread::sleep_for(slow_reader_period);
    stream.read(buffer.data(), size);
    read_count += stream.gcount();
    EXPECT_STATUS_OK(stream.status());
    if (slow_reader_period < max_slow_reader_period) {
      slow_reader_period += std::chrono::minutes(1);
    }
  }
  std::cout << " DONE\n";
  EXPECT_STATUS_OK(stream.status());

  stream.Close();
  EXPECT_STATUS_OK(stream.status());

  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
}

TEST_F(SlowReaderIntegrationTest, StreamingReadRestart) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string const bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();
  auto file_name = MakeRandomObjectName();

  // Construct an object large enough to not be downloaded in the first chunk.
  auto const object_size = 4 * 1024 * 1024L;
  auto large_text = MakeRandomData(object_size);

  // Create an object with the contents to download.
  StatusOr<ObjectMetadata> source_meta = client->InsertObject(
      bucket_name, object_name, large_text, IfGenerationMatch(0));
  ASSERT_STATUS_OK(source_meta);

  // Create an iostream to read the object back. When running against the
  // testbench we can fail quickly by asking the testbench to break the stream
  // in the middle.
  auto slow_reader_period = std::chrono::seconds(UsingTestbench() ? 1 : 400);

  auto make_reader = [this, bucket_name, object_name, &client](int64_t offset) {
    if (UsingTestbench()) {
      return client->ReadObject(
          bucket_name, object_name,
          CustomHeader("x-goog-testbench-instructions", "return-broken-stream"),
          ReadFromOffset(offset));
    }
    return client->ReadObject(bucket_name, object_name, ReadFromOffset(offset));
  };

  ObjectReadStream stream = make_reader(0);

  auto const max_slow_reader_period = std::chrono::minutes(10);
  std::vector<char> buffer;
  long const size = 1024 * 1024;
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
      slow_reader_period += std::chrono::minutes(1);
    }
  }
  std::cout << " DONE\n";
  EXPECT_STATUS_OK(stream.status());

  stream.Close();
  EXPECT_STATUS_OK(stream.status());

  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
}

}  // anonymous namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

int main(int argc, char* argv[]) {
  google::cloud::testing_util::InitGoogleMock(argc, argv);

  // Make sure the arguments are valid.
  if (argc != 3) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(argv[0]).find_last_of('/');
    std::cerr << "Usage: " << cmd.substr(last_slash + 1)
              << " <project-id> <bucket-name>\n";
    return 1;
  }

  google::cloud::storage::flag_project_id = argv[1];
  google::cloud::storage::flag_bucket_name = argv[2];

  return RUN_ALL_TESTS();
}
