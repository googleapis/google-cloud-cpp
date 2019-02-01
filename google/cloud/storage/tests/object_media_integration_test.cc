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

#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/log.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/testing_util/capture_log_lines_backend.h"
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
using google::cloud::storage::testing::CountMatchingEntities;
using google::cloud::storage::testing::TestPermanentFailure;
using ::testing::HasSubstr;

/// Store the project and instance captured from the command-line arguments.
class ObjectMediaTestEnvironment : public ::testing::Environment {
 public:
  ObjectMediaTestEnvironment(std::string project, std::string instance) {
    project_id_ = std::move(project);
    bucket_name_ = std::move(instance);
  }

  static std::string const& project_id() { return project_id_; }
  static std::string const& bucket_name() { return bucket_name_; }

 private:
  static std::string project_id_;
  static std::string bucket_name_;
};

std::string ObjectMediaTestEnvironment::project_id_;
std::string ObjectMediaTestEnvironment::bucket_name_;

class ObjectMediaIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {};

bool UsingTestbench() {
  return google::cloud::internal::GetEnv("CLOUD_STORAGE_TESTBENCH_ENDPOINT")
      .has_value();
}

TEST_F(ObjectMediaIntegrationTest, XmlDownloadFile) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_TRUE(client.ok()) << "status=" << client.status();

  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
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
  ASSERT_TRUE(status.ok()) << "status=" << status;
  // Create a iostream to read the object back.
  std::ifstream stream(file_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  auto expected_str = expected.str();
  ASSERT_EQ(expected_str.size(), actual.size()) << " meta=" << meta;
  EXPECT_EQ(expected_str, actual);

  status = client->DeleteObject(bucket_name, object_name);
  EXPECT_TRUE(status.ok()) << "status=" << status;
  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectMediaIntegrationTest, JsonDownloadFile) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_TRUE(client.ok()) << "status=" << client.status();

  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
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
  ASSERT_TRUE(status.ok()) << "status=" << status;
  // Create a iostream to read the object back.
  std::ifstream stream(file_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  auto expected_str = expected.str();
  ASSERT_EQ(expected_str.size(), actual.size()) << " meta=" << meta;
  EXPECT_EQ(expected_str, actual);

  status = client->DeleteObject(bucket_name, object_name);
  EXPECT_TRUE(status.ok()) << "status=" << status;
  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectMediaIntegrationTest, DownloadFileFailure) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_TRUE(client.ok()) << "status=" << client.status();

  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();
  auto file_name = MakeRandomObjectName();

  auto status = client->DownloadToFile(bucket_name, object_name, file_name);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.message(), HasSubstr(object_name));
}

TEST_F(ObjectMediaIntegrationTest, DownloadFileCannotOpenFile) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_TRUE(client.ok()) << "status=" << client.status();

  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();
  StatusOr<ObjectMetadata> meta =
      client->InsertObject(bucket_name, object_name, LoremIpsum(),
                           IfGenerationMatch(0), Projection::Full());
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();

  // Create an invalid path for the destination object.
  auto file_name = MakeRandomObjectName() + "/" + MakeRandomObjectName();

  auto status = client->DownloadToFile(bucket_name, object_name, file_name);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.message(), HasSubstr(object_name));

  status = client->DeleteObject(bucket_name, object_name);
  EXPECT_TRUE(status.ok()) << "status=" << status;
}

TEST_F(ObjectMediaIntegrationTest, DownloadFileCannotWriteToFile) {
#if GTEST_OS_LINUX
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_TRUE(client.ok()) << "status=" << client.status();

  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();
  StatusOr<ObjectMetadata> meta =
      client->InsertObject(bucket_name, object_name, LoremIpsum(),
                           IfGenerationMatch(0), Projection::Full());
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();

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
  EXPECT_TRUE(status.ok()) << "status=" << status;
#endif  // GTEST_OS_LINUX
}

TEST_F(ObjectMediaIntegrationTest, UploadFile) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_TRUE(client.ok()) << "status=" << client.status();

  auto file_name = ::testing::TempDir() + MakeRandomObjectName();
  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;
  // Create a file with the contents to upload.
  std::ofstream os(file_name);
  WriteRandomLines(os, expected);
  os.close();

  StatusOr<ObjectMetadata> meta = client->UploadFile(
      file_name, bucket_name, object_name, IfGenerationMatch(0));
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();
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
  EXPECT_TRUE(status.ok()) << "status=" << status;
  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectMediaIntegrationTest, UploadFileEmpty) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_TRUE(client.ok()) << "status=" << client.status();

  auto file_name = ::testing::TempDir() + MakeRandomObjectName();
  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // Create a file with the contents to upload.
  std::ofstream(file_name).close();

  StatusOr<ObjectMetadata> meta = client->UploadFile(
      file_name, bucket_name, object_name, IfGenerationMatch(0));
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();
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
  EXPECT_TRUE(status.ok()) << "status=" << status;
  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectMediaIntegrationTest, UploadFileMissingFileFailure) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_TRUE(client.ok()) << "status=" << client.status();

  auto file_name = MakeRandomObjectName();
  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  StatusOr<ObjectMetadata> meta = client->UploadFile(
      file_name, bucket_name, object_name, IfGenerationMatch(0));
  EXPECT_FALSE(meta.ok());
  EXPECT_EQ(StatusCode::kNotFound, meta.status().code());
  EXPECT_THAT(meta.status().message(), HasSubstr(file_name));
}

TEST_F(ObjectMediaIntegrationTest, UploadFileUploadFailure) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_TRUE(client.ok()) << "status=" << client.status();

  auto file_name = ::testing::TempDir() + MakeRandomObjectName();
  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // Create the file.
  std::ofstream(file_name) << LoremIpsum();

  // Create the object.
  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name, object_name, LoremIpsum(), IfGenerationMatch(0));
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();

  // Trying to upload the file to the same object with the IfGenerationMatch(0)
  // condition should fail because the object already exists.
  StatusOr<ObjectMetadata> upload = client->UploadFile(
      file_name, bucket_name, object_name, IfGenerationMatch(0));
  EXPECT_FALSE(upload.ok());
  EXPECT_EQ(StatusCode::kFailedPrecondition, upload.status().code());

  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_TRUE(status.ok()) << "status=" << status;
  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectMediaIntegrationTest, UploadFileNonRegularWarning) {
  // We need to create a non-regular file that is also readable, this is easy to
  // do on Linux, and hard to do on the other platforms we support, so just run
  // the test there.
#if GTEST_OS_LINUX || GTEST_OS_MAC
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_TRUE(client.ok()) << "status=" << client.status();

  auto file_name = ::testing::TempDir() + MakeRandomObjectName();
  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
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
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();
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
  EXPECT_TRUE(status.ok()) << "status=" << status;
  EXPECT_EQ(0, std::remove(file_name.c_str()));
#endif  // GTEST_OS_LINUX
}

TEST_F(ObjectMediaIntegrationTest, XmlUploadFile) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_TRUE(client.ok()) << "status=" << client.status();

  auto file_name = ::testing::TempDir() + MakeRandomObjectName();
  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
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
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();
  auto expected_str = expected.str();

  // Create a iostream to read the object back.
  auto stream = client->ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  EXPECT_EQ(expected_str.size(), actual.size()) << " meta=" << *meta;
  EXPECT_EQ(expected_str, actual);

  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_TRUE(status.ok()) << "status=" << status;
  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectMediaIntegrationTest, UploadFileResumableBySize) {
  // Create a client that always uses resumable uploads.
  auto client_options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_TRUE(client_options.ok()) << "status=" << client_options.status();
  Client client(client_options->set_maximum_simple_upload_size(0));
  auto file_name = ::testing::TempDir() + MakeRandomObjectName();
  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;
  // Create a file with the contents to upload.
  std::ofstream os(file_name);
  WriteRandomLines(os, expected);
  os.close();

  StatusOr<ObjectMetadata> meta = client.UploadFile(
      file_name, bucket_name, object_name, IfGenerationMatch(0));
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();
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
  EXPECT_TRUE(status.ok()) << "status=" << status;
  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectMediaIntegrationTest, UploadFileResumableByOption) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_TRUE(client.ok()) << "status=" << client.status();

  auto file_name = ::testing::TempDir() + MakeRandomObjectName();
  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
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
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();
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
  EXPECT_TRUE(status.ok()) << "status=" << status;
  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectMediaIntegrationTest, UploadFileResumableQuantum) {
  // Create a client that always uses resumable uploads.
  auto client_options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_TRUE(client_options.ok()) << "status=" << client_options.status();
  Client client(client_options->set_maximum_simple_upload_size(0));
  auto file_name = ::testing::TempDir() + MakeRandomObjectName();
  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
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
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();
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
  EXPECT_TRUE(status.ok()) << "status=" << status;
  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectMediaIntegrationTest, UploadFileResumableNonQuantum) {
  // Create a client that always uses resumable uploads.
  auto client_options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_TRUE(client_options.ok()) << "status=" << client_options.status();
  Client client(client_options->set_maximum_simple_upload_size(0));
  auto file_name = ::testing::TempDir() + MakeRandomObjectName();
  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;
  // Create a file with the contents to upload.
  std::ofstream os(file_name);
  static_assert(internal::UploadChunkRequest::kChunkSizeQuantum % 256 == 0,
                "This test assumes the chunk quantum is a multiple of 256; it "
                "needs fixing");
  auto desired_size = (5 * internal::UploadChunkRequest::kChunkSizeQuantum / 2);
  WriteRandomLines(os, expected, desired_size / 128, 128);
  os.close();

  StatusOr<ObjectMetadata> meta = client.UploadFile(
      file_name, bucket_name, object_name, IfGenerationMatch(0));
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();
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
  EXPECT_TRUE(status.ok()) << "status=" << status;
  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectMediaIntegrationTest, UploadFileResumableUploadFailure) {
  // Create a client that always uses resumable uploads.
  auto client_options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_TRUE(client_options.ok()) << "status=" << client_options.status();
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
  ASSERT_TRUE(client.ok()) << "status=" << client.status();

  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
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
  ASSERT_TRUE(source_meta.ok()) << "status=" << source_meta.status();

  // Create a iostream to read the object back.
  auto stream = client->ReadObject(bucket_name, object_name);
  std::string actual;
  std::copy_n(std::istreambuf_iterator<char>{stream}, 1024,
              std::back_inserter(actual));

  EXPECT_EQ(large_text.substr(0, 1024), actual);
  stream.Close();
  EXPECT_TRUE(stream.status().ok()) << "status=" << stream.status();

  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_TRUE(status.ok()) << "status=" << status;
}

/// @test Read a portion of a relatively large object using the JSON API.
TEST_F(ObjectMediaIntegrationTest, ReadRangeJSON) {
  // The testbench always requires multiple iterations to copy this object.
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_TRUE(client.ok()) << "status=" << client.status();

  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
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
  ASSERT_TRUE(source_meta.ok()) << "status=" << source_meta.status();

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
  EXPECT_TRUE(status.ok()) << "status=" << status;
}

/// @test Read a portion of a relatively large object using the XML API.
TEST_F(ObjectMediaIntegrationTest, ReadRangeXml) {
  // The testbench always requires multiple iterations to copy this object.
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_TRUE(client.ok()) << "status=" << client.status();

  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
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
  ASSERT_TRUE(source_meta.ok()) << "status=" << source_meta.status();

  EXPECT_EQ(object_name, source_meta->name());
  EXPECT_EQ(bucket_name, source_meta->bucket());

  // Create a iostream to read the object back.
  auto stream = client->ReadObject(bucket_name, object_name,
                                   ReadRange(1 * chunk, 2 * chunk));
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(1 * chunk, actual.size());
  EXPECT_EQ(large_text.substr(1 * chunk, 1 * chunk), actual);

  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_TRUE(status.ok()) << "status=" << status;
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
              << " <project-id> <bucket-name>" << std::endl;
    return 1;
  }

  std::string const project_id = argv[1];
  std::string const bucket_name = argv[2];
  (void)::testing::AddGlobalTestEnvironment(
      new google::cloud::storage::ObjectMediaTestEnvironment(project_id,
                                                             bucket_name));

  return RUN_ALL_TESTS();
}
