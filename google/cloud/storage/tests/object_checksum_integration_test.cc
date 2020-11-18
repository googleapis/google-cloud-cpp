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
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/capture_log_lines_backend.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <regex>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::StartsWith;

class ObjectChecksumIntegrationTest
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

TEST_F(ObjectChecksumIntegrationTest, InsertWithCrc32c) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name_, object_name, expected, IfGenerationMatch(0),
      Crc32cChecksumValue("6Y46Mg=="));
  ASSERT_STATUS_OK(meta);

  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name_, meta->bucket());

  // Create a iostream to read the object back.
  auto stream = client->ReadObject(bucket_name_, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);

  auto status = client->DeleteObject(bucket_name_, object_name);
  EXPECT_STATUS_OK(status);
}

TEST_F(ObjectChecksumIntegrationTest, XmlInsertWithCrc32c) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name_, object_name, expected, IfGenerationMatch(0), Fields(""),
      Crc32cChecksumValue("6Y46Mg=="));
  ASSERT_STATUS_OK(meta);

  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name_, meta->bucket());

  // Create a iostream to read the object back.
  auto stream = client->ReadObject(bucket_name_, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);

  auto status = client->DeleteObject(bucket_name_, object_name);
  EXPECT_STATUS_OK(status);
}

TEST_F(ObjectChecksumIntegrationTest, InsertWithCrc32cFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // This should fail because the CRC32C value is incorrect.
  StatusOr<ObjectMetadata> failure = client->InsertObject(
      bucket_name_, object_name, expected, IfGenerationMatch(0),
      Crc32cChecksumValue("4UedKg=="));
  EXPECT_THAT(failure, StatusIs(Not(StatusCode::kOk)));
}

TEST_F(ObjectChecksumIntegrationTest, XmlInsertWithCrc32cFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // This should fail because the CRC32C value is incorrect.
  StatusOr<ObjectMetadata> failure = client->InsertObject(
      bucket_name_, object_name, expected, IfGenerationMatch(0), Fields(""),
      Crc32cChecksumValue("4UedKg=="));
  EXPECT_THAT(failure, StatusIs(Not(StatusCode::kOk)));
}

TEST_F(ObjectChecksumIntegrationTest, InsertWithComputedCrc32c) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name_, object_name, expected, IfGenerationMatch(0),
      Crc32cChecksumValue(ComputeCrc32cChecksum(expected)));
  ASSERT_STATUS_OK(meta);

  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name_, meta->bucket());

  // Create a iostream to read the object back.
  auto stream = client->ReadObject(bucket_name_, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);

  auto status = client->DeleteObject(bucket_name_, object_name);
  EXPECT_STATUS_OK(status);
}

/// @test Verify that CRC32C checksums are computed by default.
TEST_F(ObjectChecksumIntegrationTest, DefaultCrc32cInsertXML) {
  auto client_options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(client_options);
  Client client((*client_options)
                    .set_enable_raw_client_tracing(true)
                    .set_enable_http_tracing(true));
  auto object_name = MakeRandomObjectName();

  auto backend = std::make_shared<testing_util::CaptureLogLinesBackend>();
  auto id = LogSink::Instance().AddBackend(backend);
  StatusOr<ObjectMetadata> insert_meta =
      client.InsertObject(bucket_name_, object_name, LoremIpsum(),
                          IfGenerationMatch(0), Fields(""));
  ASSERT_STATUS_OK(insert_meta);

  LogSink::Instance().RemoveBackend(id);

  EXPECT_THAT(backend->ClearLogLines(),
              Contains(StartsWith("x-goog-hash: crc32c=")));

  auto status = client.DeleteObject(bucket_name_, object_name);
  EXPECT_STATUS_OK(status);
}

/// @test Verify that CRC32C checksums are computed by default.
TEST_F(ObjectChecksumIntegrationTest, DefaultCrc32cInsertJSON) {
  auto client_options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(client_options);
  Client client((*client_options)
                    .set_enable_raw_client_tracing(true)
                    .set_enable_http_tracing(true));
  auto object_name = MakeRandomObjectName();

  auto backend = std::make_shared<testing_util::CaptureLogLinesBackend>();
  auto id = LogSink::Instance().AddBackend(backend);
  StatusOr<ObjectMetadata> insert_meta = client.InsertObject(
      bucket_name_, object_name, LoremIpsum(), IfGenerationMatch(0));
  ASSERT_STATUS_OK(insert_meta);

  LogSink::Instance().RemoveBackend(id);

  // This is a big indirect, we detect if the upload changed to
  // multipart/related, and if so, we assume the hash value is being used.
  // Unfortunately I (@coryan) cannot think of a way to examine the upload
  // contents.
  EXPECT_THAT(
      backend->ClearLogLines(),
      Contains(StartsWith("content-type: multipart/related; boundary=")));

  if (insert_meta->has_metadata("x_emulator_upload")) {
    // When running against the emulator, we have some more information to
    // verify the right upload type and contents were sent.
    EXPECT_EQ("multipart", insert_meta->metadata("x_emulator_upload"));
    ASSERT_TRUE(insert_meta->has_metadata("x_emulator_crc32c"));
    auto expected_crc32c = ComputeCrc32cChecksum(LoremIpsum());
    EXPECT_EQ(expected_crc32c, insert_meta->metadata("x_emulator_crc32c"));
  }

  auto status = client.DeleteObject(bucket_name_, object_name);
  EXPECT_STATUS_OK(status);
}

/// @test Verify that CRC32C checksums are computed by default on downloads.
TEST_F(ObjectChecksumIntegrationTest, DefaultCrc32cStreamingReadXML) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  // Create an object and a stream to read it back.
  StatusOr<ObjectMetadata> meta =
      client->InsertObject(bucket_name_, object_name, LoremIpsum(),
                           IfGenerationMatch(0), Projection::Full());
  ASSERT_STATUS_OK(meta);

  auto stream = client->ReadObject(bucket_name_, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(stream.IsOpen());
  ASSERT_FALSE(actual.empty());

  EXPECT_EQ(stream.received_hash(), stream.computed_hash());
  EXPECT_THAT(stream.received_hash(), HasSubstr(meta->crc32c()));

  auto status = client->DeleteObject(bucket_name_, object_name);
  EXPECT_STATUS_OK(status);
}

/// @test Verify that CRC32C checksums are computed by default on downloads.
TEST_F(ObjectChecksumIntegrationTest, DefaultCrc32cStreamingReadJSON) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  // Create an object and a stream to read it back.
  StatusOr<ObjectMetadata> meta =
      client->InsertObject(bucket_name_, object_name, LoremIpsum(),
                           IfGenerationMatch(0), Projection::Full());
  ASSERT_STATUS_OK(meta);

  auto stream = client->ReadObject(bucket_name_, object_name,
                                   IfMetagenerationNotMatch(0));
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(stream.IsOpen());
  ASSERT_FALSE(actual.empty());

  EXPECT_EQ(stream.received_hash(), stream.computed_hash());
  EXPECT_THAT(stream.received_hash(), HasSubstr(meta->crc32c()));

  auto status = client->DeleteObject(bucket_name_, object_name);
  EXPECT_STATUS_OK(status);
}

/// @test Verify that CRC32C checksums are computed by default on uploads.
TEST_F(ObjectChecksumIntegrationTest, DefaultCrc32cStreamingWriteJSON) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  // Create the object, but only if it does not exist already.
  auto os =
      client->WriteObject(bucket_name_, object_name, IfGenerationMatch(0));
  os.exceptions(std::ios_base::failbit);
  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;
  WriteRandomLines(os, expected);

  auto expected_crc32c = ComputeCrc32cChecksum(expected.str());

  os.Close();
  ObjectMetadata meta = os.metadata().value();
  EXPECT_EQ(os.received_hash(), os.computed_hash());
  EXPECT_THAT(os.received_hash(), HasSubstr(expected_crc32c));

  auto status = client->DeleteObject(bucket_name_, object_name);
  EXPECT_STATUS_OK(status);
}

/// @test Verify that CRC32C checksum mismatches are reported by default on
/// downloads.
TEST_F(ObjectChecksumIntegrationTest, MismatchedCrc32cStreamingReadXML) {
  // This test is disabled when not using the emulator as it relies on the
  // emulator to inject faults.
  if (!UsingEmulator()) GTEST_SKIP();
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  // Create an object and a stream to read it back.
  StatusOr<ObjectMetadata> meta =
      client->InsertObject(bucket_name_, object_name, LoremIpsum(),
                           IfGenerationMatch(0), Projection::Full());
  ASSERT_STATUS_OK(meta);

  auto stream = client->ReadObject(
      bucket_name_, object_name,
      CustomHeader("x-goog-emulator-instructions", "return-corrupted-data"));

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
  EXPECT_THAT(stream.status(), StatusIs(Not(StatusCode::kOk)));
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS

  auto status = client->DeleteObject(bucket_name_, object_name);
  EXPECT_STATUS_OK(status);
}

/// @test Verify that CRC32C checksum mismatches are reported by default on
/// downloads.
TEST_F(ObjectChecksumIntegrationTest, MismatchedCrc32cStreamingReadJSON) {
  // This test is disabled when not using the emulator as it relies on the
  // emulator to inject faults.
  if (!UsingEmulator()) GTEST_SKIP();
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  // Create an object and a stream to read it back.
  StatusOr<ObjectMetadata> meta =
      client->InsertObject(bucket_name_, object_name, LoremIpsum(),
                           IfGenerationMatch(0), Projection::Full());
  ASSERT_STATUS_OK(meta);

  auto stream = client->ReadObject(
      bucket_name_, object_name, DisableMD5Hash(true),
      IfMetagenerationNotMatch(0),
      CustomHeader("x-goog-emulator-instructions", "return-corrupted-data"));

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

  auto status = client->DeleteObject(bucket_name_, object_name);
  EXPECT_STATUS_OK(status);
}

/// @test Verify that CRC32C checksum mismatches are reported when using
/// .read().
TEST_F(ObjectChecksumIntegrationTest, MismatchedMD5StreamingReadXMLRead) {
  // This test is disabled when not using the emulator as it relies on the
  // emulator to inject faults.
  if (!UsingEmulator()) GTEST_SKIP();
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();
  auto contents = MakeRandomData(1024 * 1024);

  // Create an object and a stream to read it back.
  StatusOr<ObjectMetadata> meta =
      client->InsertObject(bucket_name_, object_name, contents,
                           IfGenerationMatch(0), Projection::Full());
  ASSERT_STATUS_OK(meta);

  auto stream = client->ReadObject(
      bucket_name_, object_name, DisableMD5Hash(true),
      CustomHeader("x-goog-emulator-instructions", "return-corrupted-data"));

  // Create a buffer large enough to hold the results and read pas EOF.
  std::vector<char> buffer(2 * contents.size());
  stream.read(buffer.data(), buffer.size());
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_TRUE(stream.bad());
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THAT(stream.status(), StatusIs(StatusCode::kDataLoss));
  EXPECT_NE(stream.received_hash(), stream.computed_hash());
  EXPECT_EQ(stream.received_hash(), meta->crc32c());

  auto status = client->DeleteObject(bucket_name_, object_name);
  EXPECT_STATUS_OK(status);
}

/// @test Verify that CRC32C checksum mismatches are reported when using
/// .read().
TEST_F(ObjectChecksumIntegrationTest, MismatchedMD5StreamingReadJSONRead) {
  // This test is disabled when not using the emulator as it relies on the
  // emulator to inject faults.
  if (!UsingEmulator()) GTEST_SKIP();
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();
  auto contents = MakeRandomData(1024 * 1024);

  // Create an object and a stream to read it back.
  StatusOr<ObjectMetadata> meta =
      client->InsertObject(bucket_name_, object_name, contents,
                           IfGenerationMatch(0), Projection::Full());
  ASSERT_STATUS_OK(meta);

  auto stream = client->ReadObject(
      bucket_name_, object_name, DisableMD5Hash(true),
      IfMetagenerationNotMatch(0),
      CustomHeader("x-goog-emulator-instructions", "return-corrupted-data"));

  // Create a buffer large enough to hold the results and read pas EOF.
  std::vector<char> buffer(2 * contents.size());
  stream.read(buffer.data(), buffer.size());
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_TRUE(stream.bad());
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THAT(stream.status(), StatusIs(StatusCode::kDataLoss));
  EXPECT_NE(stream.received_hash(), stream.computed_hash());
  EXPECT_EQ(stream.received_hash(), meta->crc32c());

  auto status = client->DeleteObject(bucket_name_, object_name);
  EXPECT_STATUS_OK(status);
}

/// @test Verify that CRC32C checksum mismatches are reported by default on
/// downloads.
TEST_F(ObjectChecksumIntegrationTest, MismatchedCrc32cStreamingWriteJSON) {
  // This test is disabled when not using the emulator as it relies on the
  // emulator to inject faults.
  if (!UsingEmulator()) GTEST_SKIP();
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  // Create a stream to upload an object.
  ObjectWriteStream stream = client->WriteObject(
      bucket_name_, object_name, DisableMD5Hash(true), IfGenerationMatch(0),
      CustomHeader("x-goog-emulator-instructions", "inject-upload-data-error"));
  stream << LoremIpsum() << "\n";
  stream << LoremIpsum();

  stream.Close();
  EXPECT_TRUE(stream.bad());
  EXPECT_STATUS_OK(stream.metadata());
  EXPECT_NE(stream.received_hash(), stream.computed_hash());

  auto status = client->DeleteObject(bucket_name_, object_name);
  EXPECT_STATUS_OK(status);
}

}  // anonymous namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
