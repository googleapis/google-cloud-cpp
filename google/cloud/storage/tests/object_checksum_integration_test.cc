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
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::HasSubstr;
using ::testing::Not;

class ObjectChecksumIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest,
      public ::testing::WithParamInterface<std::string> {
 protected:
  void SetUp() override {
    bucket_name_ = google::cloud::internal::GetEnv(
                       "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME")
                       .value_or("");
    ASSERT_FALSE(bucket_name_.empty());
  }

  std::string bucket_name_;
};

/// @test Verify that CRC32C checksums are enabled by default.
TEST_P(ObjectChecksumIntegrationTest, InsertObjectDefault) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);
  auto object_name = MakeRandomObjectName();
  auto meta = client->InsertObject(bucket_name_, object_name, LoremIpsum(),
                                   RestApiFlags(GetParam()).for_insert,
                                   DisableMD5Hash(true), IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  if (meta->has_metadata("x_emulator_upload")) {
    ASSERT_TRUE(meta->has_metadata("x_emulator_crc32c"));
    ASSERT_FALSE(meta->has_metadata("x_emulator_md5"));
  }
}

/// @test Verify that `DisableCrc32cChecksum(true)` works as expected.
TEST_P(ObjectChecksumIntegrationTest, InsertObjectExplicitDisable) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);
  auto object_name = MakeRandomObjectName();
  auto meta = client->InsertObject(
      bucket_name_, object_name, LoremIpsum(), DisableCrc32cChecksum(true),
      DisableMD5Hash(true), RestApiFlags(GetParam()).for_insert,
      IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  if (meta->has_metadata("x_emulator_upload")) {
    ASSERT_FALSE(meta->has_metadata("x_emulator_crc32c"));
    ASSERT_FALSE(meta->has_metadata("x_emulator_md5"));
  }
}

/// @test Verify that `DisableCrc32cChecksum(false)` works as expected.
TEST_P(ObjectChecksumIntegrationTest, InsertObjectExplicitEnable) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);
  auto object_name = MakeRandomObjectName();
  auto meta = client->InsertObject(
      bucket_name_, object_name, LoremIpsum(), DisableCrc32cChecksum(false),
      DisableMD5Hash(true), RestApiFlags(GetParam()).for_insert,
      IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  if (meta->has_metadata("x_emulator_upload")) {
    ASSERT_TRUE(meta->has_metadata("x_emulator_crc32c"));
    ASSERT_FALSE(meta->has_metadata("x_emulator_md5"));
  }
}

TEST_P(ObjectChecksumIntegrationTest, InsertObjectWithValueSuccess) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);
  auto object_name = MakeRandomObjectName();
  auto meta = client->InsertObject(
      bucket_name_, object_name, LoremIpsum(),
      Crc32cChecksumValue(ComputeCrc32cChecksum(LoremIpsum())),
      RestApiFlags(GetParam()).for_insert, DisableMD5Hash(true),
      IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  if (meta->has_metadata("x_emulator_upload")) {
    ASSERT_TRUE(meta->has_metadata("x_emulator_crc32c"));
    ASSERT_FALSE(meta->has_metadata("x_emulator_md5"));
  }
}

TEST_P(ObjectChecksumIntegrationTest, InsertObjectWithValueFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);
  auto object_name = MakeRandomObjectName();
  auto failure = client->InsertObject(
      bucket_name_, object_name, LoremIpsum(),
      RestApiFlags(GetParam()).for_insert, DisableMD5Hash(true),
      IfGenerationMatch(0), Crc32cChecksumValue(ComputeCrc32cChecksum("")));
  EXPECT_THAT(failure, Not(IsOk()));
}

/// @test Verify that CRC32C checksums are computed by default in WriteObject().
TEST_F(ObjectChecksumIntegrationTest, WriteObjectDefault) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);
  auto object_name = MakeRandomObjectName();
  auto os = client->WriteObject(bucket_name_, object_name, DisableMD5Hash(true),
                                IfGenerationMatch(0));
  os << LoremIpsum();
  os.Close();
  auto meta = os.metadata();
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);
  EXPECT_EQ(os.received_hash(), os.computed_hash());

  EXPECT_THAT(os.received_hash(),
              HasSubstr(ComputeCrc32cChecksum(LoremIpsum())));
  EXPECT_THAT(os.computed_hash(),
              HasSubstr(ComputeCrc32cChecksum(LoremIpsum())));
  if (meta->has_metadata("x_emulator_upload")) {
    // Streaming uploads over REST cannot include checksums
    EXPECT_TRUE(meta->has_metadata("x_emulator_no_crc32c")) << *meta;
    EXPECT_TRUE(meta->has_metadata("x_emulator_no_md5")) << *meta;
  }
}

/// @test Verify that CRC32C checksums can be explicitly disabled in
/// WriteObject().
TEST_F(ObjectChecksumIntegrationTest, WriteObjectExplicitDisable) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);
  auto object_name = MakeRandomObjectName();
  auto os = client->WriteObject(bucket_name_, object_name,
                                DisableCrc32cChecksum(true),
                                DisableMD5Hash(true), IfGenerationMatch(0));
  os << LoremIpsum();
  os.Close();
  auto meta = os.metadata();
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  EXPECT_THAT(os.received_hash(),
              Not(HasSubstr(ComputeCrc32cChecksum(LoremIpsum()))));
  EXPECT_THAT(os.computed_hash(),
              Not(HasSubstr(ComputeCrc32cChecksum(LoremIpsum()))));
  if (meta->has_metadata("x_emulator_upload")) {
    // Streaming uploads over REST cannot include checksums
    EXPECT_TRUE(meta->has_metadata("x_emulator_no_crc32c")) << *meta;
    EXPECT_TRUE(meta->has_metadata("x_emulator_no_md5")) << *meta;
  }
}

/// @test Verify that CRC32C checksums can be explicitly enabled in
/// WriteObject().
TEST_F(ObjectChecksumIntegrationTest, WriteObjectExplicitEnable) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);
  auto object_name = MakeRandomObjectName();
  auto os = client->WriteObject(bucket_name_, object_name,
                                DisableCrc32cChecksum(false),
                                DisableMD5Hash(true), IfGenerationMatch(0));
  os << LoremIpsum();
  os.Close();
  auto meta = os.metadata();
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  EXPECT_THAT(os.received_hash(),
              HasSubstr(ComputeCrc32cChecksum(LoremIpsum())));
  EXPECT_THAT(os.computed_hash(),
              HasSubstr(ComputeCrc32cChecksum(LoremIpsum())));
  if (meta->has_metadata("x_emulator_upload")) {
    // Streaming uploads over REST cannot include checksums
    EXPECT_TRUE(meta->has_metadata("x_emulator_no_crc32c")) << *meta;
    EXPECT_TRUE(meta->has_metadata("x_emulator_no_md5")) << *meta;
  }
}

/// @test Verify that valid CRC32C checksums values work in WriteObject().
TEST_F(ObjectChecksumIntegrationTest, WriteObjectWithValueSuccess) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);
  auto object_name = MakeRandomObjectName();
  auto os = client->WriteObject(
      bucket_name_, object_name,
      Crc32cChecksumValue(ComputeCrc32cChecksum(LoremIpsum())),
      DisableMD5Hash(true), IfGenerationMatch(0));
  os << LoremIpsum();
  os.Close();
  auto meta = os.metadata();
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  if (meta->has_metadata("x_emulator_upload")) {
    // Streaming uploads over REST cannot include checksums unless provided by
    // the application when the upload begins.
    EXPECT_TRUE(meta->has_metadata("x_emulator_crc32c")) << *meta;
    EXPECT_TRUE(meta->has_metadata("x_emulator_no_md5")) << *meta;
  }
}

/// @test Verify that incorrect CRC32C checksums values work in WriteObject().
TEST_F(ObjectChecksumIntegrationTest, WriteObjectWithValueFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);
  auto object_name = MakeRandomObjectName();
  auto os = client->WriteObject(
      bucket_name_, object_name, MD5HashValue(ComputeMD5Hash("")),
      DisableCrc32cChecksum(true), IfGenerationMatch(0));
  os << LoremIpsum();
  os.Close();
  auto meta = os.metadata();
  EXPECT_THAT(meta, Not(IsOk()));
}

/// @test Verify that CRC32C checksum mismatches are reported when the server
/// receives bad data.
TEST_F(ObjectChecksumIntegrationTest, WriteObjectReceiveBadChecksum) {
  // This test is disabled when not using the emulator as it relies on the
  // emulator to inject faults.
  if (!UsingEmulator()) GTEST_SKIP();
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  // Create a stream to upload an object.
  ObjectWriteStream stream = client->WriteObject(
      bucket_name_, object_name, DisableMD5Hash(true),
      CustomHeader("x-goog-emulator-instructions", "inject-upload-data-error"),
      IfGenerationMatch(0));
  stream << LoremIpsum() << "\n";
  stream.Close();
  EXPECT_TRUE(stream.bad());
  ASSERT_STATUS_OK(stream.metadata());
  ScheduleForDelete(*stream.metadata());
  EXPECT_NE(stream.received_hash(), stream.computed_hash());
}

/// @test Verify that CRC32C checksum mismatches are reported by default.
TEST_F(ObjectChecksumIntegrationTest, WriteObjectUploadBadChecksum) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  // Create a stream to upload an object.
  ObjectWriteStream stream = client->WriteObject(
      bucket_name_, object_name, Crc32cChecksumValue(ComputeCrc32cChecksum("")),
      DisableMD5Hash(true), IfGenerationMatch(0));
  stream << LoremIpsum() << "\n";
  stream.Close();
  EXPECT_TRUE(stream.bad());
  ASSERT_THAT(stream.metadata(), Not(IsOk()));
}

/// @test Verify that CRC32C checksums are computed by default on downloads.
TEST_P(ObjectChecksumIntegrationTest, ReadObjectDefault) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);
  auto object_name = MakeRandomObjectName();
  auto meta = client->InsertObject(bucket_name_, object_name, LoremIpsum(),
                                   IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  auto stream = client->ReadObject(bucket_name_, object_name,
                                   RestApiFlags(GetParam()).for_streaming_read);
  auto const actual = std::string{std::istreambuf_iterator<char>{stream}, {}};
  ASSERT_FALSE(stream.IsOpen());

  EXPECT_EQ(stream.received_hash(), stream.computed_hash());
  EXPECT_THAT(stream.received_hash(), HasSubstr(meta->crc32c()));
}

/// @test Verify that CRC32C checksum mismatches are reported by default on
/// downloads.
TEST_P(ObjectChecksumIntegrationTest, ReadObjectCorruptedByServerGetc) {
  // This test is disabled when not using the emulator as it relies on the
  // emulator to inject faults.
  if (!UsingEmulator()) GTEST_SKIP();
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);
  auto object_name = MakeRandomObjectName();
  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name_, object_name, LoremIpsum(), IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  auto stream = client->ReadObject(
      bucket_name_, object_name, DisableMD5Hash(true),
      CustomHeader("x-goog-emulator-instructions", "return-corrupted-data"),
      RestApiFlags(GetParam()).for_streaming_read);

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
  EXPECT_THAT(stream.status(), Not(IsOk()));
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify that CRC32C checksum mismatches are reported by default on
/// downloads.
TEST_P(ObjectChecksumIntegrationTest, ReadObjectCorruptedByServerRead) {
  // This test is disabled when not using the emulator as it relies on the
  // emulator to inject faults.
  if (!UsingEmulator()) GTEST_SKIP();
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);
  auto object_name = MakeRandomObjectName();
  StatusOr<ObjectMetadata> meta =
      client->InsertObject(bucket_name_, object_name, LoremIpsum(),
                           IfGenerationMatch(0), Projection::Full());
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  auto stream = client->ReadObject(
      bucket_name_, object_name, DisableMD5Hash(true),
      CustomHeader("x-goog-emulator-instructions", "return-corrupted-data"),
      RestApiFlags(GetParam()).for_streaming_read);

  // Create a buffer large enough to read the full contents.
  std::vector<char> buffer(2 * LoremIpsum().size());
  stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_TRUE(stream.bad());
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THAT(stream.status(), StatusIs(StatusCode::kDataLoss));
  EXPECT_NE(stream.received_hash(), stream.computed_hash());
  EXPECT_EQ(stream.received_hash(), meta->crc32c());
}

INSTANTIATE_TEST_SUITE_P(ObjectChecksumIntegrationTestJson,
                         ObjectChecksumIntegrationTest,
                         ::testing::Values("JSON"));

INSTANTIATE_TEST_SUITE_P(ObjectChecksumIntegrationTestXml,
                         ObjectChecksumIntegrationTest,
                         ::testing::Values("XML"));

}  // anonymous namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
