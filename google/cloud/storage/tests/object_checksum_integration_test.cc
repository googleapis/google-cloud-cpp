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
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/capture_log_lines_backend.h"
#include "google/cloud/testing_util/init_google_mock.h"
#include <gmock/gmock.h>
#include <regex>

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

class ObjectChecksumIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {};

TEST_F(ObjectChecksumIntegrationTest, InsertWithCrc32c) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name, object_name, expected, IfGenerationMatch(0),
      Crc32cChecksumValue("6Y46Mg=="));
  ASSERT_STATUS_OK(meta);

  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name, meta->bucket());

  // Create a iostream to read the object back.
  auto stream = client->ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);

  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
}

TEST_F(ObjectChecksumIntegrationTest, XmlInsertWithCrc32c) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name, object_name, expected, IfGenerationMatch(0), Fields(""),
      Crc32cChecksumValue("6Y46Mg=="));
  ASSERT_STATUS_OK(meta);

  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name, meta->bucket());

  // Create a iostream to read the object back.
  auto stream = client->ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);

  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
}

TEST_F(ObjectChecksumIntegrationTest, InsertWithCrc32cFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // This should fail because the CRC32C value is incorrect.
  StatusOr<ObjectMetadata> failure = client->InsertObject(
      bucket_name, object_name, expected, IfGenerationMatch(0),
      Crc32cChecksumValue("4UedKg=="));
  ASSERT_FALSE(failure.ok()) << "status=" << failure.status();
}

TEST_F(ObjectChecksumIntegrationTest, XmlInsertWithCrc32cFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // This should fail because the CRC32C value is incorrect.
  StatusOr<ObjectMetadata> failure = client->InsertObject(
      bucket_name, object_name, expected, IfGenerationMatch(0), Fields(""),
      Crc32cChecksumValue("4UedKg=="));
  ASSERT_FALSE(failure.ok()) << "status=" << failure.status();
}

TEST_F(ObjectChecksumIntegrationTest, InsertWithComputedCrc32c) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name, object_name, expected, IfGenerationMatch(0),
      Crc32cChecksumValue(ComputeCrc32cChecksum(expected)));
  ASSERT_STATUS_OK(meta);

  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name, meta->bucket());

  // Create a iostream to read the object back.
  auto stream = client->ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);

  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
}

/// @test Verify that CRC32C checksums are computed by default.
TEST_F(ObjectChecksumIntegrationTest, DefaultCrc32cInsertXML) {
  auto client_options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(client_options);
  Client client((*client_options)
                    .set_enable_raw_client_tracing(true)
                    .set_enable_http_tracing(true));
  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  auto backend = std::make_shared<testing_util::CaptureLogLinesBackend>();
  auto id = LogSink::Instance().AddBackend(backend);
  StatusOr<ObjectMetadata> insert_meta = client.InsertObject(
      bucket_name, object_name, LoremIpsum(), IfGenerationMatch(0), Fields(""));
  ASSERT_STATUS_OK(insert_meta);

  LogSink::Instance().RemoveBackend(id);

  auto count =
      std::count_if(backend->log_lines.begin(), backend->log_lines.end(),
                    [](std::string const& line) {
                      return line.rfind("x-goog-hash: crc32c=", 0) == 0;
                    });
  EXPECT_EQ(1, count);

  auto status = client.DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
}

/// @test Verify that CRC32C checksums are computed by default.
TEST_F(ObjectChecksumIntegrationTest, DefaultCrc32cInsertJSON) {
  auto client_options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(client_options);
  Client client((*client_options)
                    .set_enable_raw_client_tracing(true)
                    .set_enable_http_tracing(true));
  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  auto backend = std::make_shared<testing_util::CaptureLogLinesBackend>();
  auto id = LogSink::Instance().AddBackend(backend);
  StatusOr<ObjectMetadata> insert_meta = client.InsertObject(
      bucket_name, object_name, LoremIpsum(), IfGenerationMatch(0));
  ASSERT_STATUS_OK(insert_meta);

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

  auto status = client.DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
}

/// @test Verify that CRC32C checksums are computed by default on downloads.
TEST_F(ObjectChecksumIntegrationTest, DefaultCrc32cStreamingReadXML) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // Create an object and a stream to read it back.
  StatusOr<ObjectMetadata> meta =
      client->InsertObject(bucket_name, object_name, LoremIpsum(),
                           IfGenerationMatch(0), Projection::Full());
  ASSERT_STATUS_OK(meta);

  auto stream = client->ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(stream.IsOpen());
  ASSERT_FALSE(actual.empty());

  EXPECT_EQ(stream.received_hash(), stream.computed_hash());
  EXPECT_THAT(stream.received_hash(), HasSubstr(meta->crc32c()));

  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
}

/// @test Verify that CRC32C checksums are computed by default on downloads.
TEST_F(ObjectChecksumIntegrationTest, DefaultCrc32cStreamingReadJSON) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // Create an object and a stream to read it back.
  StatusOr<ObjectMetadata> meta =
      client->InsertObject(bucket_name, object_name, LoremIpsum(),
                           IfGenerationMatch(0), Projection::Full());
  ASSERT_STATUS_OK(meta);

  auto stream =
      client->ReadObject(bucket_name, object_name, IfMetagenerationNotMatch(0));
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(stream.IsOpen());
  ASSERT_FALSE(actual.empty());

  EXPECT_EQ(stream.received_hash(), stream.computed_hash());
  EXPECT_THAT(stream.received_hash(), HasSubstr(meta->crc32c()));

  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
}

/// @test Verify that CRC32C checksums are computed by default on uploads.
TEST_F(ObjectChecksumIntegrationTest, DefaultCrc32cStreamingWriteJSON) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // Create the object, but only if it does not exist already.
  auto os = client->WriteObject(bucket_name, object_name, IfGenerationMatch(0));
  os.exceptions(std::ios_base::failbit);
  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;
  WriteRandomLines(os, expected);

  auto expected_crc32c = ComputeCrc32cChecksum(expected.str());

  os.Close();
  ObjectMetadata meta = os.metadata().value();
  EXPECT_EQ(os.received_hash(), os.computed_hash());
  EXPECT_THAT(os.received_hash(), HasSubstr(expected_crc32c));

  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
}

/// @test Verify that CRC32C checksum mismatches are reported by default on
/// downloads.
TEST_F(ObjectChecksumIntegrationTest, MismatchedCrc32cStreamingReadXML) {
  if (!UsingTestbench()) {
    // This test is disabled when not using the testbench as it relies on the
    // testbench to inject faults.
    return;
  }
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // Create an object and a stream to read it back.
  StatusOr<ObjectMetadata> meta =
      client->InsertObject(bucket_name, object_name, LoremIpsum(),
                           IfGenerationMatch(0), Projection::Full());
  ASSERT_STATUS_OK(meta);

  auto stream = client->ReadObject(
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

  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
}

/// @test Verify that CRC32C checksum mismatches are reported by default on
/// downloads.
TEST_F(ObjectChecksumIntegrationTest, MismatchedCrc32cStreamingReadJSON) {
  if (!UsingTestbench()) {
    // This test is disabled when not using the testbench as it relies on the
    // testbench to inject faults.
    return;
  }
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // Create an object and a stream to read it back.
  StatusOr<ObjectMetadata> meta =
      client->InsertObject(bucket_name, object_name, LoremIpsum(),
                           IfGenerationMatch(0), Projection::Full());
  ASSERT_STATUS_OK(meta);

  auto stream = client->ReadObject(
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

  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
}

/// @test Verify that CRC32C checksum mismatches are reported by default on
/// downloads.
TEST_F(ObjectChecksumIntegrationTest, MismatchedCrc32cStreamingWriteJSON) {
  if (!UsingTestbench()) {
    // This test is disabled when not using the testbench as it relies on the
    // testbench to inject faults.
    return;
  }
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // Create a stream to upload an object.
  ObjectWriteStream stream = client->WriteObject(
      bucket_name, object_name, DisableMD5Hash(true), IfGenerationMatch(0),
      CustomHeader("x-goog-testbench-instructions",
                   "inject-upload-data-error"));
  stream << LoremIpsum() << "\n";
  stream << LoremIpsum();

  stream.Close();
  EXPECT_TRUE(stream.bad());
  EXPECT_STATUS_OK(stream.metadata());
  EXPECT_NE(stream.received_hash(), stream.computed_hash());

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
