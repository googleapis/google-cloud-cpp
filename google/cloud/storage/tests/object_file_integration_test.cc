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

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/internal/filesystem.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <cstdio>
#include <fstream>
#include <thread>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AnyOf;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Not;

class ObjectFileIntegrationTest
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

// Create a client configured to always use resumable uploads for files.
Client ClientWithSimpleUploadDisabled() {
  return Client(Options{}.set<MaximumSimpleUploadSizeOption>(0));
}

TEST_F(ObjectFileIntegrationTest, XmlDownloadFile) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();
  auto file_name = MakeRandomFilename();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;
  // Create an object with the contents to download.
  auto upload =
      client->WriteObject(bucket_name_, object_name, IfGenerationMatch(0));
  upload.exceptions(std::ios_base::failbit);
  WriteRandomLines(upload, expected);
  upload.Close();
  ASSERT_STATUS_OK(upload.metadata());
  auto meta = upload.metadata().value();
  ScheduleForDelete(meta);

  auto status = client->DownloadToFile(bucket_name_, object_name, file_name);
  ASSERT_STATUS_OK(status);
  // Create an iostream to read the object back.
  std::ifstream stream(file_name, std::ios::binary);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  auto expected_str = expected.str();
  ASSERT_EQ(expected_str.size(), actual.size()) << " meta=" << meta;
  EXPECT_EQ(expected_str, actual);

  // On Windows one must close the stream before removing the file.
  stream.close();
  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectFileIntegrationTest, JsonDownloadFile) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();
  auto file_name = MakeRandomFilename();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;
  // Create an object with the contents to download.
  auto upload =
      client->WriteObject(bucket_name_, object_name, IfGenerationMatch(0));
  WriteRandomLines(upload, expected);
  upload.Close();
  ASSERT_STATUS_OK(upload.metadata());
  auto meta = upload.metadata().value();
  ScheduleForDelete(meta);

  auto status = client->DownloadToFile(bucket_name_, object_name, file_name,
                                       IfMetagenerationNotMatch(0));
  ASSERT_STATUS_OK(status);
  // Create an iostream to read the object back.
  std::ifstream stream(file_name, std::ios::binary);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  auto expected_str = expected.str();
  ASSERT_EQ(expected_str.size(), actual.size()) << " meta=" << meta;
  EXPECT_EQ(expected_str, actual);

  // On Windows one must close the stream before removing the file.
  stream.close();
  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectFileIntegrationTest, DownloadFileFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();
  auto file_name = MakeRandomFilename();

  auto status = client->DownloadToFile(bucket_name_, object_name, file_name);
  EXPECT_THAT(status, StatusIs(Not(StatusCode::kOk), HasSubstr(object_name)));
}

TEST_F(ObjectFileIntegrationTest, DownloadFileCannotOpenFile) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();
  StatusOr<ObjectMetadata> meta =
      client->InsertObject(bucket_name_, object_name, LoremIpsum(),
                           IfGenerationMatch(0), Projection::Full());
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  // Create an invalid path for the destination object.
  auto file_name = MakeRandomFilename() + "/" + MakeRandomFilename();

  auto status = client->DownloadToFile(bucket_name_, object_name, file_name);
  EXPECT_THAT(status, StatusIs(Not(StatusCode::kOk), HasSubstr(object_name)));
}

TEST_F(ObjectFileIntegrationTest, DownloadFileCannotWriteToFile) {
#if GTEST_OS_LINUX
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();
  StatusOr<ObjectMetadata> meta =
      client->InsertObject(bucket_name_, object_name, LoremIpsum(),
                           IfGenerationMatch(0), Projection::Full());
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  // We want to test that the code handles write errors *after* the file is
  // successfully opened for writing. Such errors are hard to get, typically
  // they indicate that the filesystem is full (or maybe some rare condition
  // with remote filesystem such as NFS).
  // On Linux we are fortunate that `/dev/full` meets those requirements
  // exactly:
  //   http://man7.org/linux/man-pages/man4/full.4.html
  // I (coryan@) did not know about it, so I thought a longer comment may be in
  // order.
  auto constexpr kFileName = "/dev/full";

  auto status = client->DownloadToFile(bucket_name_, object_name, kFileName);
  EXPECT_THAT(status, StatusIs(Not(StatusCode::kOk), HasSubstr(object_name)));
#endif  // GTEST_OS_LINUX
}

TEST_F(ObjectFileIntegrationTest, UploadFile) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto file_name = google::cloud::internal::PathAppend(::testing::TempDir(),
                                                       MakeRandomFilename());
  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;
  // Create a file with the contents to upload.
  std::ofstream os(file_name, std::ios::binary);
  WriteRandomLines(os, expected);
  os.close();

  StatusOr<ObjectMetadata> meta = client->UploadFile(
      file_name, bucket_name_, object_name, IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);
  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name_, meta->bucket());
  auto expected_str = expected.str();
  ASSERT_EQ(expected_str.size(), meta->size());

  // Create an iostream to read the object back.
  auto stream = client->ReadObject(bucket_name_, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  EXPECT_EQ(expected_str.size(), actual.size()) << " meta=" << *meta;
  EXPECT_EQ(expected_str, actual);

  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectFileIntegrationTest, UploadFileBinary) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto file_name = google::cloud::internal::PathAppend(::testing::TempDir(),
                                                       MakeRandomFilename());
  auto object_name = MakeRandomObjectName();

  // Create a file with the contents to upload.
  std::size_t const payload_size = 1024;  //* 1024;
  std::vector<char> expected(payload_size, 0);
  std::uniform_int_distribution<int> random_bytes(
      std::numeric_limits<char>::min(), std::numeric_limits<char>::max());
  std::generate_n(expected.begin(), payload_size, [this, &random_bytes] {
    return static_cast<char>(random_bytes(generator_));
  });

  // Explicitly add a 0x1A, it is the EOF character on Windows and causes some
  // interesting failures.
  expected[payload_size / 4] = 0x1A;
  std::ofstream os(file_name, std::ios::binary);
  os.write(expected.data(), expected.size());
  os.close();

  StatusOr<ObjectMetadata> meta = client->UploadFile(
      file_name, bucket_name_, object_name, IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);
  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name_, meta->bucket());
  ASSERT_EQ(expected.size(), meta->size());

  // Create a iostream to read the object back.
  auto stream = client->ReadObject(bucket_name_, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  EXPECT_EQ(expected.size(), actual.size()) << " meta=" << *meta;
  EXPECT_THAT(actual, ::testing::ElementsAreArray(expected));

  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectFileIntegrationTest, UploadFileEmpty) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto file_name = google::cloud::internal::PathAppend(::testing::TempDir(),
                                                       MakeRandomFilename());
  auto object_name = MakeRandomObjectName();

  // Create a file with the contents to upload.
  std::ofstream(file_name, std::ios::binary).close();

  StatusOr<ObjectMetadata> meta = client->UploadFile(
      file_name, bucket_name_, object_name, IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);
  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name_, meta->bucket());
  EXPECT_EQ(0, meta->size());

  // Create an iostream to read the object back.
  auto stream = client->ReadObject(bucket_name_, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_TRUE(actual.empty());
  EXPECT_EQ(0, actual.size());
  EXPECT_EQ("", actual);

  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectFileIntegrationTest, UploadFileMissingFileFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto file_name = MakeRandomFilename();
  auto object_name = MakeRandomObjectName();

  StatusOr<ObjectMetadata> meta = client->UploadFile(
      file_name, bucket_name_, object_name, IfGenerationMatch(0));
  EXPECT_THAT(meta, StatusIs(StatusCode::kNotFound, HasSubstr(file_name)));
}

TEST_F(ObjectFileIntegrationTest, UploadFileUploadFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto file_name = google::cloud::internal::PathAppend(::testing::TempDir(),
                                                       MakeRandomFilename());
  auto object_name = MakeRandomObjectName();

  // Create the file.
  std::ofstream(file_name, std::ios::binary) << LoremIpsum();

  // Create the object.
  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name_, object_name, LoremIpsum(), IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  // Trying to upload the file to the same object with the IfGenerationMatch(0)
  // condition should fail because the object already exists.
  StatusOr<ObjectMetadata> upload = client->UploadFile(
      file_name, bucket_name_, object_name, IfGenerationMatch(0));
  // The GCS server returns a different error code depending on the protocol
  // (REST vs. gRPC) used
  EXPECT_THAT(upload.status().code(), AnyOf(Eq(StatusCode::kFailedPrecondition),
                                            Eq(StatusCode::kAborted)))
      << " upload.status=" << upload.status();

  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectFileIntegrationTest, UploadFileNonRegularWarning) {
  // We need to create a non-regular file that is also readable, this is easy to
  // do on Linux, and hard to do on the other platforms we support, so just run
  // the test there.
#if GTEST_OS_LINUX || GTEST_OS_MAC
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto file_name = google::cloud::internal::PathAppend(::testing::TempDir(),
                                                       MakeRandomFilename());
  auto object_name = MakeRandomObjectName();

  ASSERT_NE(-1, mkfifo(file_name.c_str(), 0777));

  std::string expected = LoremIpsum();
  std::thread t([file_name, expected] {
    std::ofstream os(file_name);
    os << expected;
    os.close();
  });
  testing_util::ScopedLog log;
  StatusOr<ObjectMetadata> meta =
      client->UploadFile(file_name, bucket_name_, object_name,
                         IfGenerationMatch(0), DisableMD5Hash(true));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  EXPECT_THAT(log.ExtractLines(), Contains(HasSubstr("not a regular file")));

  t.join();
  EXPECT_EQ(0, std::remove(file_name.c_str()));
#endif  // GTEST_OS_LINUX
}

TEST_F(ObjectFileIntegrationTest, XmlUploadFile) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto file_name = google::cloud::internal::PathAppend(::testing::TempDir(),
                                                       MakeRandomFilename());
  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;

  auto generate_random_line = [this] {
    std::string const characters =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789"
        ".,/;:'[{]}=+-_}]`~!@#$%^&*()";
    return google::cloud::internal::Sample(generator_, 200, characters);
  };

  // Create a file with the contents to upload.
  std::ofstream os(file_name, std::ios::binary);
  for (int line = 0; line != 1000; ++line) {
    std::string random = generate_random_line() + "\n";
    os << line << ": " << random;
    expected << line << ": " << random;
  }
  os.close();

  StatusOr<ObjectMetadata> meta = client->UploadFile(
      file_name, bucket_name_, object_name, IfGenerationMatch(0), Fields(""));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);
  auto expected_str = expected.str();

  // Create an iostream to read the object back.
  auto stream = client->ReadObject(bucket_name_, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  EXPECT_EQ(expected_str.size(), actual.size()) << " meta=" << *meta;
  EXPECT_EQ(expected_str, actual);

  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectFileIntegrationTest, UploadFileResumableBySize) {
  auto client = ClientWithSimpleUploadDisabled();
  auto file_name = google::cloud::internal::PathAppend(::testing::TempDir(),
                                                       MakeRandomFilename());
  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;
  // Create a file with the contents to upload.
  std::ofstream os(file_name, std::ios::binary);
  WriteRandomLines(os, expected);
  os.close();

  StatusOr<ObjectMetadata> meta = client.UploadFile(
      file_name, bucket_name_, object_name, IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);
  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name_, meta->bucket());
  auto expected_str = expected.str();
  ASSERT_EQ(expected_str.size(), meta->size());

  if (UsingEmulator()) {
    ASSERT_TRUE(meta->has_metadata("x_emulator_upload"));
    EXPECT_EQ("resumable", meta->metadata("x_emulator_upload"));
  }

  // Create an iostream to read the object back.
  auto stream = client.ReadObject(bucket_name_, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  EXPECT_EQ(expected_str.size(), actual.size()) << " meta=" << *meta;
  EXPECT_EQ(expected_str, actual);

  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectFileIntegrationTest, UploadFileResumableByOption) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto file_name = google::cloud::internal::PathAppend(::testing::TempDir(),
                                                       MakeRandomFilename());
  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;
  // Create a file with the contents to upload.
  std::ofstream os(file_name, std::ios::binary);
  WriteRandomLines(os, expected);
  os.close();

  StatusOr<ObjectMetadata> meta =
      client->UploadFile(file_name, bucket_name_, object_name,
                         IfGenerationMatch(0), NewResumableUploadSession());
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);
  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name_, meta->bucket());
  auto expected_str = expected.str();
  ASSERT_EQ(expected_str.size(), meta->size());

  if (UsingEmulator()) {
    ASSERT_TRUE(meta->has_metadata("x_emulator_upload"));
    EXPECT_EQ("resumable", meta->metadata("x_emulator_upload"));
  }

  // Create an iostream to read the object back.
  auto stream = client->ReadObject(bucket_name_, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  EXPECT_EQ(expected_str.size(), actual.size()) << " meta=" << *meta;
  EXPECT_EQ(expected_str, actual);

  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectFileIntegrationTest, UploadFileResumableQuantum) {
  auto client = ClientWithSimpleUploadDisabled();
  auto file_name = google::cloud::internal::PathAppend(::testing::TempDir(),
                                                       MakeRandomFilename());
  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;
  // Create a file with the contents to upload.
  std::ofstream os(file_name, std::ios::binary);
  static_assert(internal::UploadChunkRequest::kChunkSizeQuantum % 128 == 0,
                "This test assumes the chunk quantum is a multiple of 128; it "
                "needs fixing");
  WriteRandomLines(os, expected,
                   3 * internal::UploadChunkRequest::kChunkSizeQuantum / 128,
                   128);
  os.close();

  StatusOr<ObjectMetadata> meta = client.UploadFile(
      file_name, bucket_name_, object_name, IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);
  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name_, meta->bucket());
  auto expected_str = expected.str();
  ASSERT_EQ(expected_str.size(), meta->size());

  // Create an iostream to read the object back.
  auto stream = client.ReadObject(bucket_name_, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  EXPECT_EQ(expected_str.size(), actual.size()) << " meta=" << *meta;
  EXPECT_EQ(expected_str, actual);

  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectFileIntegrationTest, UploadFileResumableNonQuantum) {
  auto client = ClientWithSimpleUploadDisabled();
  auto file_name = google::cloud::internal::PathAppend(::testing::TempDir(),
                                                       MakeRandomFilename());
  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;
  // Create a file with the contents to upload.
  std::ofstream os(file_name, std::ios::binary);
  static_assert(internal::UploadChunkRequest::kChunkSizeQuantum % 256 == 0,
                "This test assumes the chunk quantum is a multiple of 256; it "
                "needs fixing");
  auto desired_size = (5 * internal::UploadChunkRequest::kChunkSizeQuantum / 2);
  WriteRandomLines(os, expected, static_cast<int>(desired_size / 128), 128);
  os.close();

  StatusOr<ObjectMetadata> meta = client.UploadFile(
      file_name, bucket_name_, object_name, IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);
  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name_, meta->bucket());
  auto expected_str = expected.str();
  ASSERT_EQ(expected_str.size(), meta->size());

  // Create an iostream to read the object back.
  auto stream = client.ReadObject(bucket_name_, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  EXPECT_EQ(expected_str.size(), actual.size()) << " meta=" << *meta;
  EXPECT_EQ(expected_str, actual);

  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectFileIntegrationTest, UploadFileResumableUploadFailure) {
  auto client = ClientWithSimpleUploadDisabled();
  auto file_name = google::cloud::internal::PathAppend(::testing::TempDir(),
                                                       MakeRandomFilename());
  auto bucket_name = MakeRandomBucketName();
  auto object_name = MakeRandomObjectName();

  // Create the file.
  std::ofstream(file_name, std::ios::binary) << LoremIpsum();

  // Trying to upload the file to a non-existing bucket should fail.
  StatusOr<ObjectMetadata> meta = client.UploadFile(
      file_name, bucket_name, object_name, IfGenerationMatch(0));
  EXPECT_THAT(meta, Not(IsOk())) << "value=" << meta.value();

  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectFileIntegrationTest, UploadPortionRegularFile) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto file_name = google::cloud::internal::PathAppend(::testing::TempDir(),
                                                       MakeRandomFilename());
  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;
  // Create a file with the contents to upload.
  std::ofstream os(file_name, std::ios::binary);
  WriteRandomLines(os, expected);
  os.close();

  StatusOr<ObjectMetadata> meta = client->UploadFile(
      file_name, bucket_name_, object_name, UploadFromOffset(10),
      UploadLimit(10000), IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);
  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name_, meta->bucket());
  auto expected_str = expected.str().substr(10, 10000);
  ASSERT_EQ(expected_str.size(), meta->size());

  // Create an iostream to read the object back.
  auto stream = client->ReadObject(bucket_name_, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  EXPECT_EQ(expected_str.size(), actual.size()) << " meta=" << *meta;
  EXPECT_EQ(expected_str, actual);

  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectFileIntegrationTest, ResumableUploadFileCustomHeader) {
  // Test relies on emulator for capturing custom header
  if (!UsingEmulator()) GTEST_SKIP();

  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto file_name = google::cloud::internal::PathAppend(::testing::TempDir(),
                                                       MakeRandomFilename());
  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;
  // Create a file with the contents to upload.
  std::ofstream os(file_name, std::ios::binary);
  WriteRandomLines(os, expected);
  os.close();

  // Create a CustomHeader object to send in request.
  CustomHeader custom_header("x-goog-emulator-custom-header",
                             "custom_header_value");

  StatusOr<ObjectMetadata> meta =
      client->UploadFile(file_name, bucket_name_, object_name, custom_header,
                         IfGenerationMatch(0), NewResumableUploadSession());
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name_, meta->bucket());
  auto expected_str = expected.str();
  ASSERT_EQ(expected_str.size(), meta->size());

  ASSERT_TRUE(meta->has_metadata("x_emulator_custom_header"));
  EXPECT_EQ(custom_header.value(), meta->metadata("x_emulator_custom_header"));

  // Create an iostream to read the object back.
  auto stream = client->ReadObject(bucket_name_, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  EXPECT_EQ(expected_str.size(), actual.size()) << " meta=" << *meta;
  EXPECT_EQ(expected_str, actual);

  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

}  // anonymous namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
