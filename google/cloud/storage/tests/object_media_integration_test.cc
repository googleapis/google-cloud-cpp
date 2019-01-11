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
  Client client;
  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();
  auto file_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;
  // Create an object with the contents to download.
  auto upload =
      client.WriteObject(bucket_name, object_name, IfGenerationMatch(0));
  upload.exceptions(std::ios_base::failbit);
  WriteRandomLines(upload, expected);
  upload.Close();
  ObjectMetadata meta = upload.metadata().value();

  client.DownloadToFile(bucket_name, object_name, file_name);
  // Create a iostream to read the object back.
  std::ifstream stream(file_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  auto expected_str = expected.str();
  ASSERT_EQ(expected_str.size(), actual.size()) << " meta=" << meta;
  EXPECT_EQ(expected_str, actual);

  client.DeleteObject(bucket_name, object_name);
  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectMediaIntegrationTest, JsonDownloadFile) {
  Client client;
  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();
  auto file_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;
  // Create an object with the contents to download.
  auto upload =
      client.WriteObject(bucket_name, object_name, IfGenerationMatch(0));
  WriteRandomLines(upload, expected);
  upload.Close();
  ObjectMetadata meta = upload.metadata().value();

  client.DownloadToFile(bucket_name, object_name, file_name,
                        IfMetagenerationNotMatch(0));
  // Create a iostream to read the object back.
  std::ifstream stream(file_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  auto expected_str = expected.str();
  ASSERT_EQ(expected_str.size(), actual.size()) << " meta=" << meta;
  EXPECT_EQ(expected_str, actual);

  client.DeleteObject(bucket_name, object_name);
  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectMediaIntegrationTest, DownloadFileFailure) {
  Client client;
  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();
  auto file_name = MakeRandomObjectName();

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      try {
        client.DownloadToFile(bucket_name, object_name, file_name);
      } catch (std::runtime_error const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr(object_name));
        throw;
      },
      std::runtime_error);
#else
  EXPECT_DEATH_IF_SUPPORTED(
      client.DownloadToFile(bucket_name, object_name, file_name),
      "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST_F(ObjectMediaIntegrationTest, DownloadFileCannotOpenFile) {
  Client client;
  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();
  StatusOr<ObjectMetadata> meta =
      client.InsertObject(bucket_name, object_name, LoremIpsum(),
                          IfGenerationMatch(0), Projection::Full());
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();

  // Create an invalid path for the destination object.
  auto file_name = MakeRandomObjectName() + "/" + MakeRandomObjectName();

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      try {
        client.DownloadToFile(bucket_name, object_name, file_name);
      } catch (std::runtime_error const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr(file_name));
        throw;
      },
      std::runtime_error);
#else
  EXPECT_DEATH_IF_SUPPORTED(
      client.DownloadToFile(bucket_name, object_name, file_name),
      "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  client.DeleteObject(bucket_name, object_name);
}

TEST_F(ObjectMediaIntegrationTest, DownloadFileCannotWriteToFile) {
#if GTEST_OS_LINUX
  Client client;
  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();
  StatusOr<ObjectMetadata> meta =
      client.InsertObject(bucket_name, object_name, LoremIpsum(),
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

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      try {
        client.DownloadToFile(bucket_name, object_name, file_name);
      } catch (std::runtime_error const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr(file_name));
        throw;
      },
      std::runtime_error);
#else
  EXPECT_DEATH_IF_SUPPORTED(
      client.DownloadToFile(bucket_name, object_name, file_name),
      "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  client.DeleteObject(bucket_name, object_name);
#endif  // GTEST_OS_LINUX
}

TEST_F(ObjectMediaIntegrationTest, UploadFile) {
  Client client;
  auto file_name = ::testing::TempDir() + MakeRandomObjectName();
  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;
  // Create a file with the contents to upload.
  std::ofstream os(file_name);
  WriteRandomLines(os, expected);
  os.close();

  ObjectMetadata meta = client.UploadFile(file_name, bucket_name, object_name,
                                          IfGenerationMatch(0));
  EXPECT_EQ(object_name, meta.name());
  EXPECT_EQ(bucket_name, meta.bucket());
  auto expected_str = expected.str();
  ASSERT_EQ(expected_str.size(), meta.size());

  // Create a iostream to read the object back.
  auto stream = client.ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  EXPECT_EQ(expected_str.size(), actual.size()) << " meta=" << meta;
  EXPECT_EQ(expected_str, actual);

  client.DeleteObject(bucket_name, object_name);
  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectMediaIntegrationTest, UploadFileEmpty) {
  Client client;
  auto file_name = ::testing::TempDir() + MakeRandomObjectName();
  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // Create a file with the contents to upload.
  std::ofstream(file_name).close();

  ObjectMetadata meta = client.UploadFile(file_name, bucket_name, object_name,
                                          IfGenerationMatch(0));
  EXPECT_EQ(object_name, meta.name());
  EXPECT_EQ(bucket_name, meta.bucket());
  EXPECT_EQ(0U, meta.size());

  // Create a iostream to read the object back.
  auto stream = client.ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_TRUE(actual.empty());
  EXPECT_EQ(0U, actual.size());
  EXPECT_EQ("", actual);

  client.DeleteObject(bucket_name, object_name);
  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectMediaIntegrationTest, UploadFileMissingFileFailure) {
  Client client;
  auto file_name = MakeRandomObjectName();
  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      try {
        client.UploadFile(file_name, bucket_name, object_name,
                          IfGenerationMatch(0));
      } catch (std::runtime_error const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr(file_name));
        throw;
      },
      std::runtime_error);
#else
  EXPECT_DEATH_IF_SUPPORTED(
      client.UploadFile(file_name, bucket_name, object_name,
                        IfGenerationMatch(0)),
      "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST_F(ObjectMediaIntegrationTest, UploadFileUploadFailure) {
  Client client;
  auto file_name = ::testing::TempDir() + MakeRandomObjectName();
  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // Create the file.
  std::ofstream(file_name) << LoremIpsum();

  // Create the object.
  StatusOr<ObjectMetadata> meta = client.InsertObject(
      bucket_name, object_name, LoremIpsum(), IfGenerationMatch(0));
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();

  // Trying to upload the file to the same object with the IfGenerationMatch(0)
  // condition should fail because the object already exists.
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      try {
        client.UploadFile(file_name, bucket_name, object_name,
                          IfGenerationMatch(0));
      } catch (std::runtime_error const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("[FAILED_PRECONDITION]"));
        throw;
      },
      std::runtime_error);
#else
  EXPECT_DEATH_IF_SUPPORTED(
      client.UploadFile(file_name, bucket_name, object_name,
                        IfGenerationMatch(0)),
      "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS

  client.DeleteObject(bucket_name, object_name);
  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectMediaIntegrationTest, UploadFileNonRegularWarning) {
  // We need to create a non-regular file that is also readable, this is easy to
  // do on Linux, and hard to do on the other platforms we support, so just run
  // the test there.
#if GTEST_OS_LINUX
  Client client;
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

  ObjectMetadata meta =
      client.UploadFile(file_name, bucket_name, object_name,
                        IfGenerationMatch(0), DisableMD5Hash(true));

  LogSink::Instance().RemoveBackend(id);

  auto count = std::count_if(
      backend->log_lines.begin(), backend->log_lines.end(),
      [file_name](std::string const& line) {
        return line.find(file_name) != std::string::npos and
               line.find("not a regular file") != std::string::npos;
      });
  EXPECT_NE(0U, count);

  t.join();
  client.DeleteObject(bucket_name, object_name);
  EXPECT_EQ(0, std::remove(file_name.c_str()));
#endif  // GTEST_OS_LINUX
}

TEST_F(ObjectMediaIntegrationTest, XmlUploadFile) {
  Client client;
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

  ObjectMetadata meta = client.UploadFile(file_name, bucket_name, object_name,
                                          IfGenerationMatch(0), Fields(""));
  auto expected_str = expected.str();

  // Create a iostream to read the object back.
  auto stream = client.ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  EXPECT_EQ(expected_str.size(), actual.size()) << " meta=" << meta;
  EXPECT_EQ(expected_str, actual);

  client.DeleteObject(bucket_name, object_name);
  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectMediaIntegrationTest, UploadFileResumableBySize) {
  // Create a client that always uses resumable uploads.
  Client client(ClientOptions().set_maximum_simple_upload_size(0));
  auto file_name = ::testing::TempDir() + MakeRandomObjectName();
  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;
  // Create a file with the contents to upload.
  std::ofstream os(file_name);
  WriteRandomLines(os, expected);
  os.close();

  ObjectMetadata meta = client.UploadFile(file_name, bucket_name, object_name,
                                          IfGenerationMatch(0));
  EXPECT_EQ(object_name, meta.name());
  EXPECT_EQ(bucket_name, meta.bucket());
  auto expected_str = expected.str();
  ASSERT_EQ(expected_str.size(), meta.size());

  if (UsingTestbench()) {
    ASSERT_TRUE(meta.has_metadata("x_testbench_upload"));
    EXPECT_EQ("resumable", meta.metadata("x_testbench_upload"));
  }

  // Create a iostream to read the object back.
  auto stream = client.ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  EXPECT_EQ(expected_str.size(), actual.size()) << " meta=" << meta;
  EXPECT_EQ(expected_str, actual);

  client.DeleteObject(bucket_name, object_name);
  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectMediaIntegrationTest, UploadFileResumableByOption) {
  Client client;
  auto file_name = ::testing::TempDir() + MakeRandomObjectName();
  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;
  // Create a file with the contents to upload.
  std::ofstream os(file_name);
  WriteRandomLines(os, expected);
  os.close();

  ObjectMetadata meta =
      client.UploadFile(file_name, bucket_name, object_name,
                        IfGenerationMatch(0), NewResumableUploadSession());
  EXPECT_EQ(object_name, meta.name());
  EXPECT_EQ(bucket_name, meta.bucket());
  auto expected_str = expected.str();
  ASSERT_EQ(expected_str.size(), meta.size());

  if (UsingTestbench()) {
    ASSERT_TRUE(meta.has_metadata("x_testbench_upload"));
    EXPECT_EQ("resumable", meta.metadata("x_testbench_upload"));
  }

  // Create a iostream to read the object back.
  auto stream = client.ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  EXPECT_EQ(expected_str.size(), actual.size()) << " meta=" << meta;
  EXPECT_EQ(expected_str, actual);

  client.DeleteObject(bucket_name, object_name);
  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectMediaIntegrationTest, UploadFileResumableQuantum) {
  // Create a client that always uses resumable uploads.
  Client client(ClientOptions().set_maximum_simple_upload_size(0));
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

  ObjectMetadata meta = client.UploadFile(file_name, bucket_name, object_name,
                                          IfGenerationMatch(0));
  EXPECT_EQ(object_name, meta.name());
  EXPECT_EQ(bucket_name, meta.bucket());
  auto expected_str = expected.str();
  ASSERT_EQ(expected_str.size(), meta.size());

  // Create a iostream to read the object back.
  auto stream = client.ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  EXPECT_EQ(expected_str.size(), actual.size()) << " meta=" << meta;
  EXPECT_EQ(expected_str, actual);

  client.DeleteObject(bucket_name, object_name);
  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectMediaIntegrationTest, UploadFileResumableNonQuantum) {
  // Create a client that always uses resumable uploads.
  Client client(ClientOptions().set_maximum_simple_upload_size(0));
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

  ObjectMetadata meta = client.UploadFile(file_name, bucket_name, object_name,
                                          IfGenerationMatch(0));
  EXPECT_EQ(object_name, meta.name());
  EXPECT_EQ(bucket_name, meta.bucket());
  auto expected_str = expected.str();
  ASSERT_EQ(expected_str.size(), meta.size());

  // Create a iostream to read the object back.
  auto stream = client.ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  EXPECT_EQ(expected_str.size(), actual.size()) << " meta=" << meta;
  EXPECT_EQ(expected_str, actual);

  client.DeleteObject(bucket_name, object_name);
  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectMediaIntegrationTest, UploadFileResumableUploadFailure) {
  // Create a client that always uses resumable uploads.
  Client client(ClientOptions().set_maximum_simple_upload_size(0));
  auto file_name = ::testing::TempDir() + MakeRandomObjectName();
  auto bucket_name = MakeRandomBucketName();
  auto object_name = MakeRandomObjectName();

  // Create the file.
  std::ofstream(file_name) << LoremIpsum();

  // Trying to upload the file to a non-existing bucket should fail.
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      try {
        client.UploadFile(file_name, bucket_name, object_name,
                          IfGenerationMatch(0));
      } catch (std::runtime_error const& ex) { throw; },
      std::runtime_error);
#else
  EXPECT_DEATH_IF_SUPPORTED(
      client.UploadFile(file_name, bucket_name, object_name,
                        IfGenerationMatch(0)),
      "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS

  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectMediaIntegrationTest, StreamingReadClose) {
  Client client;
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
  StatusOr<ObjectMetadata> source_meta = client.InsertObject(
      bucket_name, object_name, large_text, IfGenerationMatch(0));
  ASSERT_TRUE(source_meta.ok()) << "status=" << source_meta.status();

  // Create a iostream to read the object back.
  auto stream = client.ReadObject(bucket_name, object_name);
  std::string actual;
  std::copy_n(std::istreambuf_iterator<char>{stream}, 1024,
              std::back_inserter(actual));

  EXPECT_EQ(large_text.substr(0, 1024), actual);
  stream.Close();
  EXPECT_TRUE(stream.status().ok()) << "status=" << stream.status();

  client.DeleteObject(bucket_name, object_name);
}

/// @test Verify that MD5 hash mismatches are reported by default on downloads.
TEST_F(ObjectMediaIntegrationTest, MismatchedMD5StreamingReadXML) {
  if (not UsingTestbench()) {
    // This test is disabled when not using the testbench as it relies on the
    // testbench to inject faults.
    return;
  }
  Client client;
  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // Create an object and a stream to read it back.
  StatusOr<ObjectMetadata> meta =
      client.InsertObject(bucket_name, object_name, LoremIpsum(),
                          IfGenerationMatch(0), Projection::Full());
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();

  auto stream = client.ReadObject(
      bucket_name, object_name, DisableCrc32cChecksum(true),
      CustomHeader("x-goog-testbench-instructions", "return-corrupted-data"));

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      try {
        std::string actual(std::istreambuf_iterator<char>{stream},
                           std::istreambuf_iterator<char>{});
      } catch (HashMismatchError const& ex) {
        EXPECT_NE(ex.received_hash(), ex.computed_hash());
        EXPECT_THAT(ex.what(), HasSubstr("mismatched hashes"));
        throw;
      },
      HashMismatchError);
#else
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_NE(stream.received_hash(), stream.computed_hash());
  EXPECT_EQ(stream.received_hash(), meta->md5_hash());
  EXPECT_FALSE(stream.status().ok());
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS

  client.DeleteObject(bucket_name, object_name);
}

/// @test Verify that MD5 hash mismatches are reported by default on downloads.
TEST_F(ObjectMediaIntegrationTest, MismatchedMD5StreamingReadJSON) {
  if (not UsingTestbench()) {
    // This test is disabled when not using the testbench as it relies on the
    // testbench to inject faults.
    return;
  }
  Client client;
  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // Create an object and a stream to read it back.
  StatusOr<ObjectMetadata> meta =
      client.InsertObject(bucket_name, object_name, LoremIpsum(),
                          IfGenerationMatch(0), Projection::Full());
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();

  auto stream = client.ReadObject(
      bucket_name, object_name, DisableCrc32cChecksum(true),
      IfMetagenerationNotMatch(0),
      CustomHeader("x-goog-testbench-instructions", "return-corrupted-data"));

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      try {
        std::string actual(std::istreambuf_iterator<char>{stream},
                           std::istreambuf_iterator<char>{});
      } catch (HashMismatchError const& ex) {
        EXPECT_NE(ex.received_hash(), ex.computed_hash());
        EXPECT_THAT(ex.what(), HasSubstr("mismatched hashes"));
        throw;
      },
      HashMismatchError);
#else
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_FALSE(stream.received_hash().empty());
  EXPECT_FALSE(stream.computed_hash().empty());
  EXPECT_NE(stream.received_hash(), stream.computed_hash());
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS

  client.DeleteObject(bucket_name, object_name);
}

/// @test Verify that MD5 hash mismatches are reported by default on downloads.
TEST_F(ObjectMediaIntegrationTest, MismatchedMD5StreamingWriteXML) {
  if (not UsingTestbench()) {
    // This test is disabled when not using the testbench as it relies on the
    // testbench to inject faults.
    return;
  }
  Client client;
  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // Create a stream to upload an object.
  ObjectWriteStream stream =
      client.WriteObject(bucket_name, object_name, DisableCrc32cChecksum(true),
                         IfGenerationMatch(0), Fields(""),
                         CustomHeader("x-goog-testbench-instructions",
                                      "inject-upload-data-error"));
  stream << LoremIpsum() << "\n";
  stream << LoremIpsum();

  stream.Close();
  EXPECT_TRUE(stream.bad());
  EXPECT_TRUE(stream.metadata().ok());
  EXPECT_NE(stream.received_hash(), stream.computed_hash());

  client.DeleteObject(bucket_name, object_name);
}

/// @test Verify that MD5 hash mismatches are reported by default on downloads.
TEST_F(ObjectMediaIntegrationTest, MismatchedMD5StreamingWriteJSON) {
  if (not UsingTestbench()) {
    // This test is disabled when not using the testbench as it relies on the
    // testbench to inject faults.
    return;
  }
  Client client;
  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // Create a stream to upload an object.
  ObjectWriteStream stream =
      client.WriteObject(bucket_name, object_name, DisableCrc32cChecksum(true),
                         IfGenerationMatch(0),
                         CustomHeader("x-goog-testbench-instructions",
                                      "inject-upload-data-error"));
  stream << LoremIpsum() << "\n";
  stream << LoremIpsum();

  stream.Close();
  EXPECT_TRUE(stream.bad());
  EXPECT_TRUE(stream.metadata().ok());
  EXPECT_NE(stream.received_hash(), stream.computed_hash());

  client.DeleteObject(bucket_name, object_name);
}

TEST_F(ObjectMediaIntegrationTest, InsertWithCrc32c) {
  Client client;
  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client.InsertObject(
      bucket_name, object_name, expected, IfGenerationMatch(0),
      Crc32cChecksumValue("6Y46Mg=="));
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();

  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name, meta->bucket());

  // Create a iostream to read the object back.
  auto stream = client.ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);

  client.DeleteObject(bucket_name, object_name);
}

TEST_F(ObjectMediaIntegrationTest, XmlInsertWithCrc32c) {
  Client client;
  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client.InsertObject(
      bucket_name, object_name, expected, IfGenerationMatch(0), Fields(""),
      Crc32cChecksumValue("6Y46Mg=="));
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();

  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name, meta->bucket());

  // Create a iostream to read the object back.
  auto stream = client.ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);

  client.DeleteObject(bucket_name, object_name);
}

TEST_F(ObjectMediaIntegrationTest, InsertWithCrc32cFailure) {
  Client client;
  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // This should fail because the CRC32C value is incorrect.
  StatusOr<ObjectMetadata> failure = client.InsertObject(
      bucket_name, object_name, expected, IfGenerationMatch(0),
      Crc32cChecksumValue("4UedKg=="));
  ASSERT_FALSE(failure.ok()) << "status=" << failure.status();
}

TEST_F(ObjectMediaIntegrationTest, XmlInsertWithCrc32cFailure) {
  Client client;
  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // This should fail because the CRC32C value is incorrect.
  StatusOr<ObjectMetadata> failure = client.InsertObject(
      bucket_name, object_name, expected, IfGenerationMatch(0), Fields(""),
      Crc32cChecksumValue("4UedKg=="));
  ASSERT_FALSE(failure.ok()) << "status=" << failure.status();
}

TEST_F(ObjectMediaIntegrationTest, InsertWithComputedCrc32c) {
  Client client;
  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client.InsertObject(
      bucket_name, object_name, expected, IfGenerationMatch(0),
      Crc32cChecksumValue(ComputeCrc32cChecksum(expected)));
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();

  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name, meta->bucket());

  // Create a iostream to read the object back.
  auto stream = client.ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);

  client.DeleteObject(bucket_name, object_name);
}

/// @test Verify that CRC32C checksums are computed by default.
TEST_F(ObjectMediaIntegrationTest, DefaultCrc32cInsertXML) {
  Client client(ClientOptions()
                    .set_enable_raw_client_tracing(true)
                    .set_enable_http_tracing(true));
  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  auto backend = std::make_shared<testing_util::CaptureLogLinesBackend>();
  auto id = LogSink::Instance().AddBackend(backend);
  StatusOr<ObjectMetadata> insert_meta = client.InsertObject(
      bucket_name, object_name, LoremIpsum(), IfGenerationMatch(0), Fields(""));
  ASSERT_TRUE(insert_meta.ok()) << "status=" << insert_meta.status();

  LogSink::Instance().RemoveBackend(id);

  auto count =
      std::count_if(backend->log_lines.begin(), backend->log_lines.end(),
                    [](std::string const& line) {
                      return line.rfind("x-goog-hash: crc32c=", 0) == 0;
                    });
  EXPECT_EQ(1, count);

  client.DeleteObject(bucket_name, object_name);
}

/// @test Verify that CRC32C checksums are computed by default.
TEST_F(ObjectMediaIntegrationTest, DefaultCrc32cInsertJSON) {
  Client client(ClientOptions()
                    .set_enable_raw_client_tracing(true)
                    .set_enable_http_tracing(true));
  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  auto backend = std::make_shared<testing_util::CaptureLogLinesBackend>();
  auto id = LogSink::Instance().AddBackend(backend);
  StatusOr<ObjectMetadata> insert_meta = client.InsertObject(
      bucket_name, object_name, LoremIpsum(), IfGenerationMatch(0));
  ASSERT_TRUE(insert_meta.ok()) << "status=" << insert_meta.status();

  LogSink::Instance().RemoveBackend(id);

  auto count = std::count_if(
      backend->log_lines.begin(), backend->log_lines.end(),
      [](std::string const& line) {
        // This is a big indirect, we detect if the upload changed to
        // multipart/related, and if so, we assume the hash value is being used.
        // Unfortunately I (@coryan) cannot think of a way to examine the upload
        // contents.
        return line.rfind("content-type: multipart/related; boundary=", 0) == 0;
      });
  EXPECT_EQ(1, count);

  if (insert_meta->has_metadata("x_testbench_upload")) {
    // When running against the testbench, we have some more information to
    // verify the right upload type and contents were sent.
    EXPECT_EQ("multipart", insert_meta->metadata("x_testbench_upload"));
    ASSERT_TRUE(insert_meta->has_metadata("x_testbench_crc32c"));
    auto expected_crc32c = ComputeCrc32cChecksum(LoremIpsum());
    EXPECT_EQ(expected_crc32c, insert_meta->metadata("x_testbench_crc32c"));
  }

  client.DeleteObject(bucket_name, object_name);
}

/// @test Verify that CRC32C checksums are computed by default on downloads.
TEST_F(ObjectMediaIntegrationTest, DefaultCrc32cStreamingReadXML) {
  Client client;
  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // Create an object and a stream to read it back.
  StatusOr<ObjectMetadata> meta =
      client.InsertObject(bucket_name, object_name, LoremIpsum(),
                          IfGenerationMatch(0), Projection::Full());
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();

  auto stream = client.ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(stream.IsOpen());
  ASSERT_FALSE(actual.empty());

  EXPECT_EQ(stream.received_hash(), stream.computed_hash());
  EXPECT_THAT(stream.received_hash(), HasSubstr(meta->crc32c()));

  client.DeleteObject(bucket_name, object_name);
}

/// @test Verify that CRC32C checksums are computed by default on downloads.
TEST_F(ObjectMediaIntegrationTest, DefaultCrc32cStreamingReadJSON) {
  Client client;
  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // Create an object and a stream to read it back.
  StatusOr<ObjectMetadata> meta =
      client.InsertObject(bucket_name, object_name, LoremIpsum(),
                          IfGenerationMatch(0), Projection::Full());
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();

  auto stream =
      client.ReadObject(bucket_name, object_name, IfMetagenerationNotMatch(0));
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(stream.IsOpen());
  ASSERT_FALSE(actual.empty());

  EXPECT_EQ(stream.received_hash(), stream.computed_hash());
  EXPECT_THAT(stream.received_hash(), HasSubstr(meta->crc32c()));

  client.DeleteObject(bucket_name, object_name);
}

/// @test Verify that CRC32C checksums are computed by default on uploads.
TEST_F(ObjectMediaIntegrationTest, DefaultCrc32cStreamingWriteXML) {
  Client client;
  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // Create the object, but only if it does not exist already.
  auto os = client.WriteObject(bucket_name, object_name, IfGenerationMatch(0),
                               Fields(""));
  os.exceptions(std::ios_base::failbit);
  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;
  WriteRandomLines(os, expected);

  auto expected_crc32c = ComputeCrc32cChecksum(expected.str());

  os.Close();
  ObjectMetadata meta = os.metadata().value();
  EXPECT_EQ(os.received_hash(), os.computed_hash());
  EXPECT_THAT(os.received_hash(), HasSubstr(expected_crc32c));

  client.DeleteObject(bucket_name, object_name);
}

/// @test Verify that CRC32C checksums are computed by default on uploads.
TEST_F(ObjectMediaIntegrationTest, DefaultCrc32cStreamingWriteJSON) {
  Client client;
  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // Create the object, but only if it does not exist already.
  auto os = client.WriteObject(bucket_name, object_name, IfGenerationMatch(0));
  os.exceptions(std::ios_base::failbit);
  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;
  WriteRandomLines(os, expected);

  auto expected_crc32c = ComputeCrc32cChecksum(expected.str());

  os.Close();
  ObjectMetadata meta = os.metadata().value();
  EXPECT_EQ(os.received_hash(), os.computed_hash());
  EXPECT_THAT(os.received_hash(), HasSubstr(expected_crc32c));

  client.DeleteObject(bucket_name, object_name);
}

/// @test Verify that CRC32C checksum mismatches are reported by default on
/// downloads.
TEST_F(ObjectMediaIntegrationTest, MismatchedCrc32cStreamingReadXML) {
  if (not UsingTestbench()) {
    // This test is disabled when not using the testbench as it relies on the
    // testbench to inject faults.
    return;
  }
  Client client;
  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // Create an object and a stream to read it back.
  StatusOr<ObjectMetadata> meta =
      client.InsertObject(bucket_name, object_name, LoremIpsum(),
                          IfGenerationMatch(0), Projection::Full());
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();

  auto stream = client.ReadObject(
      bucket_name, object_name,
      CustomHeader("x-goog-testbench-instructions", "return-corrupted-data"));

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      try {
        std::string actual(std::istreambuf_iterator<char>{stream},
                           std::istreambuf_iterator<char>{});
      } catch (HashMismatchError const& ex) {
        EXPECT_NE(ex.received_hash(), ex.computed_hash());
        EXPECT_THAT(ex.received_hash(), HasSubstr(meta->crc32c()));
        EXPECT_THAT(ex.what(), HasSubstr("mismatched hashes"));
        throw;
      },
      HashMismatchError);
#else
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_NE(stream.received_hash(), stream.computed_hash());
  EXPECT_THAT(stream.received_hash(), HasSubstr(meta->crc32c()));
  EXPECT_FALSE(stream.status().ok());
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS

  client.DeleteObject(bucket_name, object_name);
}

/// @test Verify that CRC32C checksum mismatches are reported by default on
/// downloads.
TEST_F(ObjectMediaIntegrationTest, MismatchedCrc32cStreamingReadJSON) {
  if (not UsingTestbench()) {
    // This test is disabled when not using the testbench as it relies on the
    // testbench to inject faults.
    return;
  }
  Client client;
  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // Create an object and a stream to read it back.
  StatusOr<ObjectMetadata> meta =
      client.InsertObject(bucket_name, object_name, LoremIpsum(),
                          IfGenerationMatch(0), Projection::Full());
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();

  auto stream = client.ReadObject(
      bucket_name, object_name, DisableMD5Hash(true),
      IfMetagenerationNotMatch(0),
      CustomHeader("x-goog-testbench-instructions", "return-corrupted-data"));

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      try {
        std::string actual(std::istreambuf_iterator<char>{stream},
                           std::istreambuf_iterator<char>{});
      } catch (HashMismatchError const& ex) {
        EXPECT_NE(ex.received_hash(), ex.computed_hash());
        EXPECT_THAT(ex.what(), HasSubstr("mismatched hashes"));
        throw;
      },
      HashMismatchError);
#else
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_FALSE(stream.received_hash().empty());
  EXPECT_FALSE(stream.computed_hash().empty());
  EXPECT_NE(stream.received_hash(), stream.computed_hash());
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS

  client.DeleteObject(bucket_name, object_name);
}

/// @test Verify that CRC32C checksum mismatches are reported by default on
/// downloads.
TEST_F(ObjectMediaIntegrationTest, MismatchedCrc32cStreamingWriteXML) {
  if (not UsingTestbench()) {
    // This test is disabled when not using the testbench as it relies on the
    // testbench to inject faults.
    return;
  }
  Client client;
  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // Create a stream to upload an object.
  ObjectWriteStream stream =
      client.WriteObject(bucket_name, object_name, DisableMD5Hash(true),
                         IfGenerationMatch(0), Fields(""),
                         CustomHeader("x-goog-testbench-instructions",
                                      "inject-upload-data-error"));
  stream << LoremIpsum() << "\n";
  stream << LoremIpsum();

  stream.Close();
  EXPECT_TRUE(stream.bad());
  EXPECT_TRUE(stream.metadata().ok());
  EXPECT_NE(stream.received_hash(), stream.computed_hash());

  client.DeleteObject(bucket_name, object_name);
}

/// @test Verify that CRC32C checksum mismatches are reported by default on
/// downloads.
TEST_F(ObjectMediaIntegrationTest, MismatchedCrc32cStreamingWriteJSON) {
  if (not UsingTestbench()) {
    // This test is disabled when not using the testbench as it relies on the
    // testbench to inject faults.
    return;
  }
  Client client;
  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // Create a stream to upload an object.
  ObjectWriteStream stream = client.WriteObject(
      bucket_name, object_name, DisableMD5Hash(true), IfGenerationMatch(0),
      CustomHeader("x-goog-testbench-instructions",
                   "inject-upload-data-error"));
  stream << LoremIpsum() << "\n";
  stream << LoremIpsum();

  stream.Close();
  EXPECT_TRUE(stream.bad());
  EXPECT_TRUE(stream.metadata().ok());
  EXPECT_NE(stream.received_hash(), stream.computed_hash());

  client.DeleteObject(bucket_name, object_name);
}

/// @test Read a portion of a relatively large object using the JSON API.
TEST_F(ObjectMediaIntegrationTest, ReadRangeJSON) {
  // The testbench always requires multiple iterations to copy this object.
  Client client;
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

  StatusOr<ObjectMetadata> source_meta = client.InsertObject(
      bucket_name, object_name, large_text, IfGenerationMatch(0));
  ASSERT_TRUE(source_meta.ok()) << "status=" << source_meta.status();

  EXPECT_EQ(object_name, source_meta->name());
  EXPECT_EQ(bucket_name, source_meta->bucket());

  // Create a iostream to read the object back.
  auto stream = client.ReadObject(bucket_name, object_name,
                                  ReadRange(1 * chunk, 2 * chunk),
                                  IfGenerationNotMatch(0));
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(1 * chunk, actual.size());
  EXPECT_EQ(large_text.substr(1 * chunk, 1 * chunk), actual);

  client.DeleteObject(bucket_name, object_name);
}

/// @test Read a portion of a relatively large object using the XML API.
TEST_F(ObjectMediaIntegrationTest, ReadRangeXml) {
  // The testbench always requires multiple iterations to copy this object.
  Client client;
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

  StatusOr<ObjectMetadata> source_meta = client.InsertObject(
      bucket_name, object_name, large_text, IfGenerationMatch(0));
  ASSERT_TRUE(source_meta.ok()) << "status=" << source_meta.status();

  EXPECT_EQ(object_name, source_meta->name());
  EXPECT_EQ(bucket_name, source_meta->bucket());

  // Create a iostream to read the object back.
  auto stream = client.ReadObject(bucket_name, object_name,
                                  ReadRange(1 * chunk, 2 * chunk));
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(1 * chunk, actual.size());
  EXPECT_EQ(large_text.substr(1 * chunk, 1 * chunk), actual);

  client.DeleteObject(bucket_name, object_name);
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
