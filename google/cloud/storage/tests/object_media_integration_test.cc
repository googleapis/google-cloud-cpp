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

#include "google/cloud/internal/random.h"
#include "google/cloud/internal/setenv.h"
#include "google/cloud/log.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/capture_log_lines_backend.h"
#include "google/cloud/testing_util/environment_variable_restore.h"
#include "google/cloud/testing_util/init_google_mock.h"
#include <gmock/gmock.h>
#include <cstdio>
#include <fstream>
#include <regex>
#include <thread>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::google::cloud::storage::testing::CountMatchingEntities;
using ::google::cloud::storage::testing::TestPermanentFailure;
using ::testing::HasSubstr;

// Initialized in main() below.
char const* flag_project_id;
char const* flag_bucket_name;

class ObjectMediaIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {
 public:
  ObjectMediaIntegrationTest()
      : endpoint_("CLOUD_STORAGE_TESTBENCH_ENDPOINT") {}

 protected:
  ::google::cloud::testing_util::EnvironmentVariableRestore endpoint_;
};

TEST_F(ObjectMediaIntegrationTest, XmlDownloadFile) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();
  auto file_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;
  // Create an object with the contents to download.
  auto upload =
      client->WriteObject(bucket_name, object_name, IfGenerationMatch(0));
  upload.exceptions(std::ios_base::failbit);
  WriteRandomLines(upload, expected);
  upload.Close();
  ObjectMetadata meta = upload.metadata().value();

  auto status = client->DownloadToFile(bucket_name, object_name, file_name);
  ASSERT_STATUS_OK(status);
  // Create a iostream to read the object back.
  std::ifstream stream(file_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  auto expected_str = expected.str();
  ASSERT_EQ(expected_str.size(), actual.size()) << " meta=" << meta;
  EXPECT_EQ(expected_str, actual);

  status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectMediaIntegrationTest, JsonDownloadFile) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();
  auto file_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;
  // Create an object with the contents to download.
  auto upload =
      client->WriteObject(bucket_name, object_name, IfGenerationMatch(0));
  WriteRandomLines(upload, expected);
  upload.Close();
  ObjectMetadata meta = upload.metadata().value();

  auto status = client->DownloadToFile(bucket_name, object_name, file_name,
                                       IfMetagenerationNotMatch(0));
  ASSERT_STATUS_OK(status);
  // Create a iostream to read the object back.
  std::ifstream stream(file_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  auto expected_str = expected.str();
  ASSERT_EQ(expected_str.size(), actual.size()) << " meta=" << meta;
  EXPECT_EQ(expected_str, actual);

  status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectMediaIntegrationTest, DownloadFileFailure) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();
  auto file_name = MakeRandomObjectName();

  auto status = client->DownloadToFile(bucket_name, object_name, file_name);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.message(), HasSubstr(object_name));
}

TEST_F(ObjectMediaIntegrationTest, DownloadFileCannotOpenFile) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();
  StatusOr<ObjectMetadata> meta =
      client->InsertObject(bucket_name, object_name, LoremIpsum(),
                           IfGenerationMatch(0), Projection::Full());
  ASSERT_STATUS_OK(meta);

  // Create an invalid path for the destination object.
  auto file_name = MakeRandomObjectName() + "/" + MakeRandomObjectName();

  auto status = client->DownloadToFile(bucket_name, object_name, file_name);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.message(), HasSubstr(object_name));

  status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
}

TEST_F(ObjectMediaIntegrationTest, DownloadFileCannotWriteToFile) {
#if GTEST_OS_LINUX
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();
  StatusOr<ObjectMetadata> meta =
      client->InsertObject(bucket_name, object_name, LoremIpsum(),
                           IfGenerationMatch(0), Projection::Full());
  ASSERT_STATUS_OK(meta);

  // We want to test that the code handles write errors *after* the file is
  // successfully opened for writing. Such errors are hard to get, typically
  // they indicate that the filesystem is full (or maybe some rare condition
  // with remote filesystem such as NFS).
  // On Linux we are fortunate that `/dev/full` meets those requirements
  // exactly:
  //   http://man7.org/linux/man-pages/man4/full.4.html
  // I (coryan@) did not know about it, so I thought a longer comment may be in
  // order.
  auto file_name = "/dev/full";

  auto status = client->DownloadToFile(bucket_name, object_name, file_name);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.message(), HasSubstr(object_name));

  status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
#endif  // GTEST_OS_LINUX
}

TEST_F(ObjectMediaIntegrationTest, UploadFile) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);

  auto file_name = ::testing::TempDir() + MakeRandomObjectName();
  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;
  // Create a file with the contents to upload.
  std::ofstream os(file_name);
  WriteRandomLines(os, expected);
  os.close();

  StatusOr<ObjectMetadata> meta = client->UploadFile(
      file_name, bucket_name, object_name, IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);
  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name, meta->bucket());
  auto expected_str = expected.str();
  ASSERT_EQ(expected_str.size(), meta->size());

  // Create a iostream to read the object back.
  auto stream = client->ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  EXPECT_EQ(expected_str.size(), actual.size()) << " meta=" << *meta;
  EXPECT_EQ(expected_str, actual);

  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectMediaIntegrationTest, UploadFileEmpty) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);

  auto file_name = ::testing::TempDir() + MakeRandomObjectName();
  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // Create a file with the contents to upload.
  std::ofstream(file_name).close();

  StatusOr<ObjectMetadata> meta = client->UploadFile(
      file_name, bucket_name, object_name, IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);
  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name, meta->bucket());
  EXPECT_EQ(0U, meta->size());

  // Create a iostream to read the object back.
  auto stream = client->ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_TRUE(actual.empty());
  EXPECT_EQ(0U, actual.size());
  EXPECT_EQ("", actual);

  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectMediaIntegrationTest, UploadFileMissingFileFailure) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);

  auto file_name = MakeRandomObjectName();
  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  StatusOr<ObjectMetadata> meta = client->UploadFile(
      file_name, bucket_name, object_name, IfGenerationMatch(0));
  EXPECT_FALSE(meta.ok());
  EXPECT_EQ(StatusCode::kNotFound, meta.status().code());
  EXPECT_THAT(meta.status().message(), HasSubstr(file_name));
}

TEST_F(ObjectMediaIntegrationTest, UploadFileUploadFailure) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);

  auto file_name = ::testing::TempDir() + MakeRandomObjectName();
  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // Create the file.
  std::ofstream(file_name) << LoremIpsum();

  // Create the object.
  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name, object_name, LoremIpsum(), IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);

  // Trying to upload the file to the same object with the IfGenerationMatch(0)
  // condition should fail because the object already exists.
  StatusOr<ObjectMetadata> upload = client->UploadFile(
      file_name, bucket_name, object_name, IfGenerationMatch(0));
  EXPECT_FALSE(upload.ok());
  EXPECT_EQ(StatusCode::kFailedPrecondition, upload.status().code());

  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectMediaIntegrationTest, UploadFileNonRegularWarning) {
  // We need to create a non-regular file that is also readable, this is easy to
  // do on Linux, and hard to do on the other platforms we support, so just run
  // the test there.
#if GTEST_OS_LINUX || GTEST_OS_MAC
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);

  auto file_name = ::testing::TempDir() + MakeRandomObjectName();
  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  ASSERT_NE(-1, mkfifo(file_name.c_str(), 0777));

  std::string expected = LoremIpsum();
  std::thread t([file_name, expected] {
    std::ofstream os(file_name);
    os << expected;
    os.close();
  });
  auto backend = std::make_shared<testing_util::CaptureLogLinesBackend>();
  auto id = LogSink::Instance().AddBackend(backend);

  StatusOr<ObjectMetadata> meta =
      client->UploadFile(file_name, bucket_name, object_name,
                         IfGenerationMatch(0), DisableMD5Hash(true));
  ASSERT_STATUS_OK(meta);
  LogSink::Instance().RemoveBackend(id);

  auto count = std::count_if(
      backend->log_lines.begin(), backend->log_lines.end(),
      [file_name](std::string const& line) {
        return line.find(file_name) != std::string::npos &&
               line.find("not a regular file") != std::string::npos;
      });
  EXPECT_NE(0U, count);

  t.join();
  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
  EXPECT_EQ(0, std::remove(file_name.c_str()));
#endif  // GTEST_OS_LINUX
}

TEST_F(ObjectMediaIntegrationTest, XmlUploadFile) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);

  auto file_name = ::testing::TempDir() + MakeRandomObjectName();
  std::string bucket_name = flag_bucket_name;
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
  std::ofstream os(file_name);
  for (int line = 0; line != 1000; ++line) {
    std::string random = generate_random_line() + "\n";
    os << line << ": " << random;
    expected << line << ": " << random;
  }
  os.close();

  StatusOr<ObjectMetadata> meta = client->UploadFile(
      file_name, bucket_name, object_name, IfGenerationMatch(0), Fields(""));
  ASSERT_STATUS_OK(meta);
  auto expected_str = expected.str();

  // Create a iostream to read the object back.
  auto stream = client->ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  EXPECT_EQ(expected_str.size(), actual.size()) << " meta=" << *meta;
  EXPECT_EQ(expected_str, actual);

  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectMediaIntegrationTest, UploadFileResumableBySize) {
  // Create a client that always uses resumable uploads.
  auto client_options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(client_options);
  Client client(client_options->set_maximum_simple_upload_size(0));
  auto file_name = ::testing::TempDir() + MakeRandomObjectName();
  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;
  // Create a file with the contents to upload.
  std::ofstream os(file_name);
  WriteRandomLines(os, expected);
  os.close();

  StatusOr<ObjectMetadata> meta = client.UploadFile(
      file_name, bucket_name, object_name, IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);
  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name, meta->bucket());
  auto expected_str = expected.str();
  ASSERT_EQ(expected_str.size(), meta->size());

  if (UsingTestbench()) {
    ASSERT_TRUE(meta->has_metadata("x_testbench_upload"));
    EXPECT_EQ("resumable", meta->metadata("x_testbench_upload"));
  }

  // Create a iostream to read the object back.
  auto stream = client.ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  EXPECT_EQ(expected_str.size(), actual.size()) << " meta=" << *meta;
  EXPECT_EQ(expected_str, actual);

  auto status = client.DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectMediaIntegrationTest, UploadFileResumableByOption) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);

  auto file_name = ::testing::TempDir() + MakeRandomObjectName();
  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;
  // Create a file with the contents to upload.
  std::ofstream os(file_name);
  WriteRandomLines(os, expected);
  os.close();

  StatusOr<ObjectMetadata> meta =
      client->UploadFile(file_name, bucket_name, object_name,
                         IfGenerationMatch(0), NewResumableUploadSession());
  ASSERT_STATUS_OK(meta);
  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name, meta->bucket());
  auto expected_str = expected.str();
  ASSERT_EQ(expected_str.size(), meta->size());

  if (UsingTestbench()) {
    ASSERT_TRUE(meta->has_metadata("x_testbench_upload"));
    EXPECT_EQ("resumable", meta->metadata("x_testbench_upload"));
  }

  // Create a iostream to read the object back.
  auto stream = client->ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  EXPECT_EQ(expected_str.size(), actual.size()) << " meta=" << *meta;
  EXPECT_EQ(expected_str, actual);

  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectMediaIntegrationTest, UploadFileResumableQuantum) {
  // Create a client that always uses resumable uploads.
  auto client_options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(client_options);
  Client client(client_options->set_maximum_simple_upload_size(0));
  auto file_name = ::testing::TempDir() + MakeRandomObjectName();
  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;
  // Create a file with the contents to upload.
  std::ofstream os(file_name);
  static_assert(internal::UploadChunkRequest::kChunkSizeQuantum % 128 == 0,
                "This test assumes the chunk quantum is a multiple of 128; it "
                "needs fixing");
  WriteRandomLines(os, expected,
                   3 * internal::UploadChunkRequest::kChunkSizeQuantum / 128,
                   128);
  os.close();

  StatusOr<ObjectMetadata> meta = client.UploadFile(
      file_name, bucket_name, object_name, IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);
  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name, meta->bucket());
  auto expected_str = expected.str();
  ASSERT_EQ(expected_str.size(), meta->size());

  // Create a iostream to read the object back.
  auto stream = client.ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  EXPECT_EQ(expected_str.size(), actual.size()) << " meta=" << *meta;
  EXPECT_EQ(expected_str, actual);

  auto status = client.DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectMediaIntegrationTest, UploadFileResumableNonQuantum) {
  // Create a client that always uses resumable uploads.
  auto client_options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(client_options);
  Client client(client_options->set_maximum_simple_upload_size(0));
  auto file_name = ::testing::TempDir() + MakeRandomObjectName();
  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;
  // Create a file with the contents to upload.
  std::ofstream os(file_name);
  static_assert(internal::UploadChunkRequest::kChunkSizeQuantum % 256 == 0,
                "This test assumes the chunk quantum is a multiple of 256; it "
                "needs fixing");
  auto desired_size = (5 * internal::UploadChunkRequest::kChunkSizeQuantum / 2);
  WriteRandomLines(os, expected, static_cast<int>(desired_size / 128), 128);
  os.close();

  StatusOr<ObjectMetadata> meta = client.UploadFile(
      file_name, bucket_name, object_name, IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);
  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name, meta->bucket());
  auto expected_str = expected.str();
  ASSERT_EQ(expected_str.size(), meta->size());

  // Create a iostream to read the object back.
  auto stream = client.ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  EXPECT_EQ(expected_str.size(), actual.size()) << " meta=" << *meta;
  EXPECT_EQ(expected_str, actual);

  auto status = client.DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectMediaIntegrationTest, UploadFileResumableUploadFailure) {
  // Create a client that always uses resumable uploads.
  auto client_options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(client_options);
  Client client(client_options->set_maximum_simple_upload_size(0));
  auto file_name = ::testing::TempDir() + MakeRandomObjectName();
  auto bucket_name = MakeRandomBucketName();
  auto object_name = MakeRandomObjectName();

  // Create the file.
  std::ofstream(file_name) << LoremIpsum();

  // Trying to upload the file to a non-existing bucket should fail.
  StatusOr<ObjectMetadata> meta = client.UploadFile(
      file_name, bucket_name, object_name, IfGenerationMatch(0));
  EXPECT_FALSE(meta.ok()) << "value=" << meta.value();

  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectMediaIntegrationTest, StreamingReadClose) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();
  auto file_name = MakeRandomObjectName();

  // Construct a large object, or at least large enough that it is not
  // downloaded in the first chunk.
  long const lines = 4 * 1024 * 1024 / 128;
  std::string large_text;
  for (long i = 0; i != lines; ++i) {
    auto line = google::cloud::internal::Sample(generator_, 127,
                                                "abcdefghijklmnopqrstuvwxyz"
                                                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                                "012456789");
    large_text += line + "\n";
  }
  // Create an object with the contents to download.
  StatusOr<ObjectMetadata> source_meta = client->InsertObject(
      bucket_name, object_name, large_text, IfGenerationMatch(0));
  ASSERT_STATUS_OK(source_meta);

  // Create a iostream to read the object back.
  auto stream = client->ReadObject(bucket_name, object_name);
  std::string actual;
  std::copy_n(std::istreambuf_iterator<char>{stream}, 1024,
              std::back_inserter(actual));

  EXPECT_EQ(large_text.substr(0, 1024), actual);
  stream.Close();
  EXPECT_STATUS_OK(stream.status());

  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
}

/// @test Read a portion of a relatively large object using the JSON API.
TEST_F(ObjectMediaIntegrationTest, ReadRangeJSON) {
  // The testbench always requires multiple iterations to copy this object.
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // This produces a 64 KiB text object, normally applications should download
  // much larger chunks from GCS, but it is really hard to figure out what is
  // broken when the error messages are in the MiB ranges.
  long const chunk = 16 * 1024L;
  long const lines = 4 * chunk / 128;
  std::string large_text;
  for (long i = 0; i != lines; ++i) {
    auto line = google::cloud::internal::Sample(generator_, 127,
                                                "abcdefghijklmnopqrstuvwxyz"
                                                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                                "012456789");
    large_text += line + "\n";
  }

  StatusOr<ObjectMetadata> source_meta = client->InsertObject(
      bucket_name, object_name, large_text, IfGenerationMatch(0));
  ASSERT_STATUS_OK(source_meta);

  EXPECT_EQ(object_name, source_meta->name());
  EXPECT_EQ(bucket_name, source_meta->bucket());

  // Create a iostream to read the object back.
  auto stream = client->ReadObject(bucket_name, object_name,
                                   ReadRange(1 * chunk, 2 * chunk),
                                   IfGenerationNotMatch(0));
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(1 * chunk, actual.size());
  EXPECT_EQ(large_text.substr(1 * chunk, 1 * chunk), actual);

  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
}

/// @test Read a portion of a relatively large object using the XML API.
TEST_F(ObjectMediaIntegrationTest, ReadRangeXml) {
  // The testbench always requires multiple iterations to copy this object.
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // This produces a 64 KiB text object, normally applications should download
  // much larger chunks from GCS, but it is really hard to figure out what is
  // broken when the error messages are in the MiB ranges.
  long const chunk = 16 * 1024L;
  long const lines = 4 * chunk / 128;
  std::string large_text;
  for (long i = 0; i != lines; ++i) {
    auto line = google::cloud::internal::Sample(generator_, 127,
                                                "abcdefghijklmnopqrstuvwxyz"
                                                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                                "012456789");
    large_text += line + "\n";
  }

  StatusOr<ObjectMetadata> source_meta = client->InsertObject(
      bucket_name, object_name, large_text, IfGenerationMatch(0));
  ASSERT_STATUS_OK(source_meta);

  EXPECT_EQ(object_name, source_meta->name());
  EXPECT_EQ(bucket_name, source_meta->bucket());

  // Create a iostream to read the object back.
  auto stream = client->ReadObject(bucket_name, object_name,
                                   ReadRange(1 * chunk, 2 * chunk));
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(1 * chunk, actual.size());
  EXPECT_EQ(large_text.substr(1 * chunk, 1 * chunk), actual);

  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
}

TEST_F(ObjectMediaIntegrationTest, ConnectionFailureReadJSON) {
  Client client{ClientOptions(oauth2::CreateAnonymousCredentials())
                    .set_endpoint("http://localhost:0"),
                LimitedErrorCountRetryPolicy(2)};

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // We force the library to use the JSON API by adding the
  // `IfGenerationNotMatch()` parameter, both JSON and XML use the same code to
  // download, but controlling the endpoint for JSON is easier.
  auto stream =
      client.ReadObject(bucket_name, object_name, IfGenerationNotMatch(0));
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_TRUE(actual.empty());
  EXPECT_TRUE(stream.bad());
  EXPECT_FALSE(stream.status().ok());
  EXPECT_EQ(StatusCode::kUnavailable, stream.status().code())
      << ", status=" << stream.status();
}

TEST_F(ObjectMediaIntegrationTest, ConnectionFailureReadXML) {
  google::cloud::internal::SetEnv("CLOUD_STORAGE_TESTBENCH_ENDPOINT",
                                  "http://localhost:0");
  Client client{ClientOptions(oauth2::CreateAnonymousCredentials())
                    .set_endpoint("http://localhost:0"),
                LimitedErrorCountRetryPolicy(2)};

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  auto stream = client.ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_TRUE(actual.empty());
  EXPECT_TRUE(stream.bad());
  EXPECT_FALSE(stream.status().ok());
  EXPECT_EQ(StatusCode::kUnavailable, stream.status().code())
      << ", status=" << stream.status();
}

TEST_F(ObjectMediaIntegrationTest, ConnectionFailureWriteJSON) {
  Client client{ClientOptions(oauth2::CreateAnonymousCredentials())
                    .set_endpoint("http://localhost:0"),
                LimitedErrorCountRetryPolicy(2)};

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // We force the library to use the JSON API by adding the
  // `IfGenerationNotMatch()` parameter, both JSON and XML use the same code to
  // download, but controlling the endpoint for JSON is easier.
  auto stream = client.WriteObject(
      bucket_name, object_name, IfGenerationMatch(0), IfGenerationNotMatch(7));
  EXPECT_TRUE(stream.bad());
  EXPECT_FALSE(stream.metadata().status().ok());
  EXPECT_EQ(StatusCode::kUnavailable, stream.metadata().status().code())
      << ", status=" << stream.metadata().status();
}

TEST_F(ObjectMediaIntegrationTest, ConnectionFailureWriteXML) {
  google::cloud::internal::SetEnv("CLOUD_STORAGE_TESTBENCH_ENDPOINT",
                                  "http://localhost:0");
  Client client{ClientOptions(oauth2::CreateAnonymousCredentials())
                    .set_endpoint("http://localhost:0"),
                LimitedErrorCountRetryPolicy(2)};

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  auto stream = client.WriteObject(
      bucket_name, object_name, IfGenerationMatch(0), IfGenerationNotMatch(7));
  EXPECT_TRUE(stream.bad());
  EXPECT_FALSE(stream.metadata().status().ok());
  EXPECT_EQ(StatusCode::kUnavailable, stream.metadata().status().code())
      << ", status=" << stream.metadata().status();
}

TEST_F(ObjectMediaIntegrationTest, ConnectionFailureDownloadFile) {
  google::cloud::internal::SetEnv("CLOUD_STORAGE_TESTBENCH_ENDPOINT",
                                  "http://localhost:0");
  Client client{ClientOptions(oauth2::CreateAnonymousCredentials())
                    .set_endpoint("http://localhost:0"),
                LimitedErrorCountRetryPolicy(2)};

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();
  auto file_name = MakeRandomObjectName();

  Status status = client.DownloadToFile(bucket_name, object_name, file_name);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(StatusCode::kUnavailable, status.code()) << ", status=" << status;
}

TEST_F(ObjectMediaIntegrationTest, ConnectionFailureUploadFile) {
  google::cloud::internal::SetEnv("CLOUD_STORAGE_TESTBENCH_ENDPOINT",
                                  "http://localhost:0");
  Client client{ClientOptions(oauth2::CreateAnonymousCredentials())
                    .set_endpoint("http://localhost:0"),
                LimitedErrorCountRetryPolicy(2)};

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();
  auto file_name = MakeRandomObjectName();

  std::ofstream(file_name) << LoremIpsum();

  StatusOr<ObjectMetadata> meta =
      client.UploadFile(file_name, bucket_name, object_name);
  EXPECT_FALSE(meta.ok()) << "value=" << meta.value();
  EXPECT_EQ(StatusCode::kUnavailable, meta.status().code())
      << ", status=" << meta.status();

  EXPECT_EQ(0, std::remove(file_name.c_str()));
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
