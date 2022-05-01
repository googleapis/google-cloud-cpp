// Copyright 2022 Google LLC
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
#include <fstream>
#include <thread>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsOk;

class DecompressiveTranscodingIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {
 protected:
  void SetUp() override {
    google::cloud::storage::testing::StorageIntegrationTest::SetUp();
    bucket_name_ = google::cloud::internal::GetEnv(
                       "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME")
                       .value_or("");
    ASSERT_FALSE(bucket_name_.empty());
  }

  std::string const& bucket_name() const { return bucket_name_; }

 private:
  std::string bucket_name_;
};

TEST_F(DecompressiveTranscodingIntegrationTest, WriteAndReadJson) {
  // TODO(storage-testbench#321) - fix transcoding support in the emulator
  if (UsingEmulator()) GTEST_SKIP();

  auto const gzip_filename = google::cloud::internal::GetEnv(
                                 "GOOGLE_CLOUD_CPP_STORAGE_TEST_GZIP_FILENAME")
                                 .value_or("");
  ASSERT_FALSE(gzip_filename.empty());
  std::ifstream gz(gzip_filename, std::ios::binary);
  auto const contents = std::string{std::istreambuf_iterator<char>(gz), {}};
  ASSERT_TRUE(gz.good());

  auto client = Client(
      Options{}
          .set<TransferStallTimeoutOption>(std::chrono::seconds(3))
          .set<RetryPolicyOption>(LimitedErrorCountRetryPolicy(5).clone()));

  auto object_name = MakeRandomObjectName();
  auto insert = client.InsertObject(
      bucket_name(), object_name, contents, IfGenerationMatch(0),
      WithObjectMetadata(
          ObjectMetadata().set_content_encoding("gzip").set_content_type(
              "text/plain")));
  ASSERT_STATUS_OK(insert);
  ScheduleForDelete(*insert);
  EXPECT_EQ(insert->content_encoding(), "gzip");
  EXPECT_EQ(insert->content_type(), "text/plain");

  // TODO(#8829) - decompressive transcoding does not work with gRPC
  if (UsingGrpc()) return;

  auto reader =
      client.ReadObject(bucket_name(), object_name, IfGenerationNotMatch(0));
  ASSERT_STATUS_OK(reader.status());
  auto decompressed = std::string{std::istreambuf_iterator<char>(reader), {}};
  ASSERT_STATUS_OK(reader.status());

  // The whole point is to decompress the data and return something different.
  ASSERT_NE(decompressed.substr(0, 32), contents.substr(0, 32));
}

TEST_F(DecompressiveTranscodingIntegrationTest, WriteAndReadXml) {
  // TODO(storage-testbench#321) - fix transcoding support in the emulator
  if (UsingEmulator()) GTEST_SKIP();

  auto const gzip_filename = google::cloud::internal::GetEnv(
                                 "GOOGLE_CLOUD_CPP_STORAGE_TEST_GZIP_FILENAME")
                                 .value_or("");
  ASSERT_FALSE(gzip_filename.empty());
  std::ifstream gz(gzip_filename, std::ios::binary);
  auto const contents = std::string{std::istreambuf_iterator<char>(gz), {}};
  ASSERT_TRUE(gz.good());

  auto client = Client(
      Options{}
          .set<TransferStallTimeoutOption>(std::chrono::seconds(3))
          .set<RetryPolicyOption>(LimitedErrorCountRetryPolicy(5).clone()));

  auto object_name = MakeRandomObjectName();
  auto insert = client.InsertObject(
      bucket_name(), object_name, contents, IfGenerationMatch(0),
      WithObjectMetadata(
          ObjectMetadata().set_content_encoding("gzip").set_content_type(
              "text/plain")));
  ASSERT_STATUS_OK(insert);
  ScheduleForDelete(*insert);
  EXPECT_EQ(insert->content_encoding(), "gzip");
  EXPECT_EQ(insert->content_type(), "text/plain");

  // TODO(#8829) - decompressive transcoding does not work with gRPC
  if (UsingGrpc()) return;

  auto reader = client.ReadObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(reader.status());
  auto decompressed = std::string{std::istreambuf_iterator<char>(reader), {}};
  ASSERT_STATUS_OK(reader.status());

  // The whole point is to decompress the data and return something different.
  ASSERT_NE(decompressed.substr(0, 32), contents.substr(0, 32));
}

TEST_F(DecompressiveTranscodingIntegrationTest, WriteAndReadCompressedJson) {
  // TODO(storage-testbench#321) - fix transcoding support in the emulator
  if (UsingEmulator()) GTEST_SKIP();

  auto const gzip_filename = google::cloud::internal::GetEnv(
                                 "GOOGLE_CLOUD_CPP_STORAGE_TEST_GZIP_FILENAME")
                                 .value_or("");
  ASSERT_FALSE(gzip_filename.empty());
  std::ifstream gz(gzip_filename, std::ios::binary);
  auto const contents = std::string{std::istreambuf_iterator<char>(gz), {}};
  ASSERT_TRUE(gz.good());

  auto client = Client(
      Options{}
          .set<TransferStallTimeoutOption>(std::chrono::seconds(3))
          .set<RetryPolicyOption>(LimitedErrorCountRetryPolicy(5).clone()));

  auto object_name = MakeRandomObjectName();
  auto insert = client.InsertObject(
      bucket_name(), object_name, contents, IfGenerationMatch(0),
      WithObjectMetadata(
          ObjectMetadata().set_content_encoding("gzip").set_content_type(
              "text/plain")));
  ASSERT_STATUS_OK(insert);
  ScheduleForDelete(*insert);
  EXPECT_EQ(insert->content_encoding(), "gzip");
  EXPECT_EQ(insert->content_type(), "text/plain");

  auto reader =
      client.ReadObject(bucket_name(), object_name, AcceptEncodingGzip(),
                        IfGenerationNotMatch(0));
  ASSERT_STATUS_OK(reader.status());
  auto compressed = std::string{std::istreambuf_iterator<char>(reader), {}};
  ASSERT_STATUS_OK(reader.status());

  ASSERT_EQ(compressed.substr(0, 32), contents.substr(0, 32));
}

TEST_F(DecompressiveTranscodingIntegrationTest, WriteAndReadCompressedXml) {
  // TODO(storage-testbench#321) - fix transcoding support in the emulator
  if (UsingEmulator()) GTEST_SKIP();

  auto const gzip_filename = google::cloud::internal::GetEnv(
                                 "GOOGLE_CLOUD_CPP_STORAGE_TEST_GZIP_FILENAME")
                                 .value_or("");
  ASSERT_FALSE(gzip_filename.empty());
  std::ifstream gz(gzip_filename, std::ios::binary);
  auto const contents = std::string{std::istreambuf_iterator<char>(gz), {}};
  ASSERT_TRUE(gz.good());

  auto client = Client(
      Options{}
          .set<TransferStallTimeoutOption>(std::chrono::seconds(3))
          .set<RetryPolicyOption>(LimitedErrorCountRetryPolicy(5).clone()));

  auto object_name = MakeRandomObjectName();
  auto insert = client.InsertObject(
      bucket_name(), object_name, contents, IfGenerationMatch(0),
      WithObjectMetadata(
          ObjectMetadata().set_content_encoding("gzip").set_content_type(
              "text/plain")));
  ASSERT_STATUS_OK(insert);
  ScheduleForDelete(*insert);
  EXPECT_EQ(insert->content_encoding(), "gzip");
  EXPECT_EQ(insert->content_type(), "text/plain");

  auto reader =
      client.ReadObject(bucket_name(), object_name, AcceptEncodingGzip());
  ASSERT_STATUS_OK(reader.status());
  auto compressed = std::string{std::istreambuf_iterator<char>(reader), {}};
  ASSERT_STATUS_OK(reader.status());

  ASSERT_EQ(compressed.substr(0, 32), contents.substr(0, 32));
}

}  // anonymous namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
