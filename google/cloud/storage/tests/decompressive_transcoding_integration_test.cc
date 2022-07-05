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
#include "absl/strings/str_split.h"
#include <gmock/gmock.h>
#include <fstream>
#include <thread>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::internal::GetEnv;
using ::testing::ElementsAreArray;

class DecompressiveTranscodingIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {
 protected:
  void SetUp() override {
    google::cloud::storage::testing::StorageIntegrationTest::SetUp();
    bucket_name_ =
        GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME").value_or("");
    ASSERT_FALSE(bucket_name_.empty());
    auto const gzip_filename =
        GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_GZIP_FILENAME").value_or("");
    ASSERT_FALSE(gzip_filename.empty());
    std::ifstream gz(gzip_filename, std::ios::binary);
    gzipped_contents_ = std::string{std::istreambuf_iterator<char>(gz), {}};
    ASSERT_TRUE(gz.good());
  }

  std::string const& bucket_name() const { return bucket_name_; }
  std::string const& gzipped_contents() const { return gzipped_contents_; }

 private:
  std::string bucket_name_;
  std::string gzipped_contents_;
};

TEST_F(DecompressiveTranscodingIntegrationTest, WriteAndReadJson) {
  auto client = Client(
      Options{}
          .set<TransferStallTimeoutOption>(std::chrono::seconds(3))
          .set<RetryPolicyOption>(LimitedErrorCountRetryPolicy(5).clone()));

  auto object_name = MakeRandomObjectName();
  auto insert = client.InsertObject(
      bucket_name(), object_name, gzipped_contents(), IfGenerationMatch(0),
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
  ASSERT_NE(decompressed.substr(0, 32), gzipped_contents().substr(0, 32));
}

TEST_F(DecompressiveTranscodingIntegrationTest, WriteAndReadXml) {
  auto client = Client(
      Options{}
          .set<TransferStallTimeoutOption>(std::chrono::seconds(3))
          .set<RetryPolicyOption>(LimitedErrorCountRetryPolicy(5).clone()));

  auto object_name = MakeRandomObjectName();
  auto insert = client.InsertObject(
      bucket_name(), object_name, gzipped_contents(), IfGenerationMatch(0),
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
  ASSERT_NE(decompressed.substr(0, 32), gzipped_contents().substr(0, 32));
}

TEST_F(DecompressiveTranscodingIntegrationTest, WriteAndReadCompressedJson) {
  auto client = Client(
      Options{}
          .set<TransferStallTimeoutOption>(std::chrono::seconds(3))
          .set<RetryPolicyOption>(LimitedErrorCountRetryPolicy(5).clone()));

  auto object_name = MakeRandomObjectName();
  auto insert = client.InsertObject(
      bucket_name(), object_name, gzipped_contents(), IfGenerationMatch(0),
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

  ASSERT_EQ(compressed.substr(0, 32), gzipped_contents().substr(0, 32));
}

TEST_F(DecompressiveTranscodingIntegrationTest, WriteAndReadCompressedXml) {
  auto client = Client(
      Options{}
          .set<TransferStallTimeoutOption>(std::chrono::seconds(3))
          .set<RetryPolicyOption>(LimitedErrorCountRetryPolicy(5).clone()));

  auto object_name = MakeRandomObjectName();
  auto insert = client.InsertObject(
      bucket_name(), object_name, gzipped_contents(), IfGenerationMatch(0),
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

  ASSERT_EQ(compressed.substr(0, 32), gzipped_contents().substr(0, 32));
}

TEST_F(DecompressiveTranscodingIntegrationTest, ResumeGunzippedDownloadJson) {
  // This test requires the emulator to force specific download failures.
  // TODO(#8829) - decompressive transcoding does not work with gRPC
  if (!UsingEmulator() || UsingGrpc()) GTEST_SKIP();

  auto client = Client(
      Options{}
          .set<MaximumCurlSocketRecvSizeOption>(16 * 1024)
          .set<DownloadBufferSizeOption>(1024)
          .set<TransferStallTimeoutOption>(std::chrono::seconds(3))
          .set<RetryPolicyOption>(LimitedErrorCountRetryPolicy(5).clone()));

  auto object_name = MakeRandomObjectName();
  auto insert = client.InsertObject(
      bucket_name(), object_name, gzipped_contents(), IfGenerationMatch(0),
      WithObjectMetadata(
          ObjectMetadata().set_content_encoding("gzip").set_content_type(
              "text/plain")));
  ASSERT_STATUS_OK(insert);
  ScheduleForDelete(*insert);
  EXPECT_EQ(insert->content_encoding(), "gzip");
  EXPECT_EQ(insert->content_type(), "text/plain");

  // Read the (decompressed) contents of the object.
  auto reader =
      client.ReadObject(bucket_name(), object_name, IfGenerationNotMatch(0));
  ASSERT_STATUS_OK(reader.status());
  auto const decompressed =
      std::string{std::istreambuf_iterator<char>(reader), {}};
  ASSERT_STATUS_OK(reader.status());

  // The test assumes the object is at least 512 KiB.
  ASSERT_GT(decompressed.size(), 512 * 1024);

  auto retry_response = InsertRetryTest(RetryTestRequest{{
      RetryTestConfiguration{"storage.objects.get",
                             {
                                 "return-broken-stream-after-128K",
                                 "return-broken-stream-after-256K",
                                 "return-broken-stream-after-512K",
                             }},
  }});
  ASSERT_STATUS_OK(retry_response);

  reader =
      client.ReadObject(bucket_name(), object_name, IfGenerationNotMatch(0),
                        CustomHeader("x-retry-test-id", retry_response->id));
  std::vector<char> buffer(128 * 1024);
  std::string actual;
  while (!reader.read(buffer.data(), buffer.size()).bad()) {
    actual +=
        std::string{buffer.data(), static_cast<std::size_t>(reader.gcount())};
    if (reader.eof()) break;
  }
  ASSERT_STATUS_OK(reader.status());

  EXPECT_EQ(actual.size(), decompressed.size());
  std::vector<std::string> actual_lines = absl::StrSplit(actual, '\n');
  std::vector<std::string> decompressed_lines =
      absl::StrSplit(decompressed, '\n');
  EXPECT_THAT(actual_lines, ElementsAreArray(decompressed_lines));
}

}  // anonymous namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
