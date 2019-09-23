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

#include "google/cloud/internal/big_endian.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/openssl_util.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/init_google_mock.h"
#include <crc32c/crc32c.h>
#include <algorithm>
#include <future>
#include <thread>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::testing::HasSubstr;

// Initialized in main() below.
char const* flag_dst_bucket_name;

class ParallelUploadTest
    : public google::cloud::storage::testing::StorageIntegrationTest {
 protected:
  static constexpr auto max_object_size = 128 * 1024 * 1024L;
  static constexpr auto object_count_per_thread = 100;

  void SetUp() override {
    StorageIntegrationTest::SetUp();
    std::cout << "Creating upload data " << std::flush;
    upload_contents_ = MakeRandomData(max_object_size);
    std::cout << "DONE\n";
    auto crc32c_value = crc32c::Crc32c(upload_contents_);
    expected_crc32c_ = internal::Base64Encode(
        google::cloud::internal::EncodeBigEndian(crc32c_value));
  }

  void UploadStreamByChunk(Client client) {
    for (int i = 0; i != object_count_per_thread; ++i) {
      auto name = MakeRandomObjectName();

      std::size_t chunk_size = 1024 * 1024L;
      auto writer = client.WriteObject(flag_dst_bucket_name, name);
      for (std::size_t offset = 0; offset < upload_contents_.size();
           offset += chunk_size) {
        auto count = (std::min)(chunk_size, upload_contents_.size() - offset);
        writer.write(upload_contents_.data() + offset, count);
        EXPECT_FALSE(writer.bad());
        EXPECT_STATUS_OK(writer.last_status());
      }
      writer.Close();
      std::cout << '.' << std::flush;
      auto metadata = writer.metadata();
      EXPECT_STATUS_OK(metadata);
      if (!metadata) continue;
      EXPECT_EQ(upload_contents_.size(), metadata->size())
          << "ERROR: mismatched size, metadata=" << *metadata;
      EXPECT_EQ(expected_crc32c_, metadata->crc32c())
          << "ERROR: mismatched size, metadata=" << *metadata;
    }
  }

  void UploadStreamAll(Client client) {
    for (int i = 0; i != object_count_per_thread; ++i) {
      auto name = MakeRandomObjectName();

      auto writer = client.WriteObject(flag_dst_bucket_name, name);
      EXPECT_FALSE(writer.bad());
      writer << upload_contents_;
      writer.Close();
      std::cout << '.' << std::flush;
      auto metadata = writer.metadata();
      EXPECT_STATUS_OK(metadata);
      if (!metadata) continue;
      EXPECT_EQ(upload_contents_.size(), metadata->size())
          << "ERROR: mismatched size, metadata=" << *metadata;
      EXPECT_EQ(expected_crc32c_, metadata->crc32c())
          << "ERROR: mismatched size, metadata=" << *metadata;
    }
  }

  void UploadInsert(Client client) {
    for (int i = 0; i != object_count_per_thread; ++i) {
      auto name = MakeRandomObjectName();

      auto metadata =
          client.InsertObject(flag_dst_bucket_name, name, upload_contents_);
      std::cout << '.' << std::flush;
      EXPECT_STATUS_OK(metadata);
      if (!metadata) continue;
      EXPECT_EQ(upload_contents_.size(), metadata->size())
          << "ERROR: mismatched size, metadata=" << *metadata;
      EXPECT_EQ(expected_crc32c_, metadata->crc32c())
          << "ERROR: mismatched size, metadata=" << *metadata;
    }
  }

  static int ThreadCount() {
    auto count = std::thread::hardware_concurrency();
    if (count == 0) {
      return 2;
    }
    return 2 * static_cast<int>(count);
  }

 private:
  std::string upload_contents_;
  std::string expected_crc32c_;
};

TEST_F(ParallelUploadTest, StreamingByChunk) {
  auto options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(options)
      << "ERROR: Aborting test, cannot create client options";

  Client client(options->set_maximum_socket_recv_size(128 * 1024)
                    .set_maximum_socket_send_size(128 * 1024)
                    .set_download_stall_timeout(std::chrono::seconds(30)));

  std::cout << "Uploading, using WriteObject and multiple .write() calls "
            << std::flush;
  std::vector<std::future<void>> tasks(ThreadCount());
  for (auto& t : tasks) {
    t = std::async(std::launch::async,
                   [this, client] { UploadStreamByChunk(client); });
  }
  for (auto& t : tasks) {
    t.get();
  }
  std::cout << " DONE\n";
}

TEST_F(ParallelUploadTest, StreamingAll) {
  auto options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(options)
      << "ERROR: Aborting test, cannot create client options";

  Client client(options->set_maximum_socket_recv_size(128 * 1024)
                    .set_maximum_socket_send_size(128 * 1024)
                    .set_download_stall_timeout(std::chrono::seconds(30)));

  std::cout << "Uploading, using WriteObject and a single operator<<() call "
            << std::flush;
  std::vector<std::future<void>> tasks(ThreadCount());
  for (auto& t : tasks) {
    t = std::async(std::launch::async,
                   [this, client] { UploadStreamAll(client); });
  }
  for (auto& t : tasks) {
    t.get();
  }
  std::cout << " DONE\n";
}

TEST_F(ParallelUploadTest, Insert) {
  auto options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(options)
      << "ERROR: Aborting test, cannot create client options";

  Client client(options->set_maximum_socket_recv_size(128 * 1024)
                    .set_maximum_socket_send_size(128 * 1024)
                    .set_download_stall_timeout(std::chrono::seconds(30)));

  std::cout << "Uploading using a single InsertObject call " << std::flush;
  std::vector<std::future<void>> tasks(ThreadCount());
  for (auto& t : tasks) {
    t = std::async(std::launch::async,
                   [this, client] { UploadInsert(client); });
  }
  for (auto& t : tasks) {
    t.get();
  }
  std::cout << " DONE\n";
}

}  // anonymous namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

int main(int argc, char* argv[]) {
  google::cloud::testing_util::InitGoogleMock(argc, argv);

  // Make sure the arguments are valid.
  if (argc != 2) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(argv[0]).find_last_of('/');
    std::cerr << "Usage: " << cmd.substr(last_slash + 1)
              << " <destination-bucket-name>\n";
    return 1;
  }

  google::cloud::storage::flag_dst_bucket_name = argv[1];

  return RUN_ALL_TESTS();
}
