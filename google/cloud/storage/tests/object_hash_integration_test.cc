// Copyright 2019 Google LLC
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
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/expect_exception.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <regex>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::Pair;

class ObjectHashIntegrationTest
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

/// @test Verify that MD5 hashes are disabled by default in InsertObject().
TEST_P(ObjectHashIntegrationTest, InsertObjectDefault) {
  auto client = MakeIntegrationTestClient();
  auto object_name = MakeRandomObjectName();
  auto meta =
      client.InsertObject(bucket_name_, object_name, LoremIpsum(),
                          DisableCrc32cChecksum(true), IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  if (meta->has_metadata("x_emulator_upload")) {
    ASSERT_FALSE(meta->has_metadata("x_emulator_crc32c"));
    ASSERT_FALSE(meta->has_metadata("x_emulator_md5"));
  }
}

/// @test Verify that MD5 hashes can be explicitly disabled in InsertObject().
TEST_P(ObjectHashIntegrationTest, InsertObjectExplicitDisable) {
  auto client = MakeIntegrationTestClient();
  auto object_name = MakeRandomObjectName();

  auto meta = client.InsertObject(
      bucket_name_, object_name, LoremIpsum(), DisableMD5Hash(true),
      DisableCrc32cChecksum(true), IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  if (meta->has_metadata("x_emulator_upload")) {
    ASSERT_FALSE(meta->has_metadata("x_emulator_crc32c"));
    ASSERT_FALSE(meta->has_metadata("x_emulator_md5"));
  }
}

/// @test Verify that MD5 hashes can be explicitly enabled in InsertObject().
TEST_P(ObjectHashIntegrationTest, InsertObjectExplicitEnable) {
  auto client = MakeIntegrationTestClient();
  auto object_name = MakeRandomObjectName();

  auto meta = client.InsertObject(
      bucket_name_, object_name, LoremIpsum(), DisableMD5Hash(false),
      DisableCrc32cChecksum(true), IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  if (meta->has_metadata("x_emulator_upload")) {
    ASSERT_FALSE(meta->has_metadata("x_emulator_crc32c"));
    ASSERT_TRUE(meta->has_metadata("x_emulator_md5"));
  }
}

/// @test Verify that valid MD5 hash values work in InsertObject().
TEST_P(ObjectHashIntegrationTest, InsertObjectWithValueSuccess) {
  auto client = MakeIntegrationTestClient();
  auto object_name = MakeRandomObjectName();
  auto meta =
      client.InsertObject(bucket_name_, object_name, LoremIpsum(),
                          MD5HashValue(ComputeMD5Hash(LoremIpsum())),
                          DisableCrc32cChecksum(true), IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  if (meta->has_metadata("x_emulator_upload")) {
    ASSERT_FALSE(meta->has_metadata("x_emulator_crc32c"));
    ASSERT_TRUE(meta->has_metadata("x_emulator_md5"));
  }
}

/// @test Verify that incorrect MD5 hash values work in InsertObject().
TEST_P(ObjectHashIntegrationTest, InsertObjectWithValueFailure) {
  // TODO(#14385) - the emulator does not support this feature for gRPC.
  if (UsingEmulator() && UsingGrpc()) GTEST_SKIP();

  auto client = MakeIntegrationTestClient();
  auto object_name = MakeRandomObjectName();

  // This should fail because the MD5 hash value is incorrect.
  auto failure = client.InsertObject(
      bucket_name_, object_name, LoremIpsum(), MD5HashValue(ComputeMD5Hash("")),
      DisableCrc32cChecksum(false), IfGenerationMatch(0));
  EXPECT_THAT(failure, Not(IsOk()));
}

/// @test Verify that MD5 hashes are disabled by default in WriteObject().
TEST_F(ObjectHashIntegrationTest, WriteObjectDefault) {
  auto client = MakeIntegrationTestClient();
  auto object_name = MakeRandomObjectName();
  auto os =
      client.WriteObject(bucket_name_, object_name, DisableCrc32cChecksum(true),
                         IfGenerationMatch(0));
  os << LoremIpsum();
  os.Close();
  auto meta = os.metadata();
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  EXPECT_THAT(os.received_hash(), Not(HasSubstr(ComputeMD5Hash(LoremIpsum()))));
  EXPECT_THAT(os.computed_hash(), Not(HasSubstr(ComputeMD5Hash(LoremIpsum()))));
  if (meta->has_metadata("x_emulator_upload")) {
    ASSERT_TRUE(meta->has_metadata("x_emulator_no_crc32c"));
    ASSERT_TRUE(meta->has_metadata("x_emulator_no_md5"));
  }
}

/// @test Verify that MD5 hashes can be explicitly disabled in WriteObject().
TEST_F(ObjectHashIntegrationTest, WriteObjectExplicitDisable) {
  auto client = MakeIntegrationTestClient();
  auto object_name = MakeRandomObjectName();
  auto os =
      client.WriteObject(bucket_name_, object_name, DisableMD5Hash(true),
                         DisableCrc32cChecksum(true), IfGenerationMatch(0));
  os << LoremIpsum();
  os.Close();
  auto meta = os.metadata();
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  EXPECT_THAT(os.received_hash(), Not(HasSubstr(ComputeMD5Hash(LoremIpsum()))));
  EXPECT_THAT(os.computed_hash(), Not(HasSubstr(ComputeMD5Hash(LoremIpsum()))));
  if (meta->has_metadata("x_emulator_upload")) {
    ASSERT_TRUE(meta->has_metadata("x_emulator_no_crc32c"));
    ASSERT_TRUE(meta->has_metadata("x_emulator_no_md5"));
  }
}

/// @test Verify that MD5 hashes can be explicitly enabled in WriteObject().
TEST_F(ObjectHashIntegrationTest, WriteObjectExplicitEnable) {
  auto client = MakeIntegrationTestClient();
  auto object_name = MakeRandomObjectName();
  auto os =
      client.WriteObject(bucket_name_, object_name, DisableMD5Hash(false),
                         DisableCrc32cChecksum(true), IfGenerationMatch(0));
  os << LoremIpsum();
  os.Close();
  auto meta = os.metadata();
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  EXPECT_THAT(os.computed_hash(), HasSubstr(ComputeMD5Hash(LoremIpsum())));
  EXPECT_THAT(os.received_hash(), HasSubstr(ComputeMD5Hash(LoremIpsum())));
  if (meta->has_metadata("x_emulator_upload")) {
    if (UsingGrpc()) {
      EXPECT_THAT(meta->metadata(), Contains(Pair("x_emulator_no_crc32c", _)));
      EXPECT_THAT(meta->metadata(), Contains(Pair("x_emulator_md5", _)));
    } else {
      // REST cannot send the checksums computed at the end of the upload.
      EXPECT_THAT(meta->metadata(), Contains(Pair("x_emulator_no_crc32c", _)));
      EXPECT_THAT(meta->metadata(), Contains(Pair("x_emulator_no_md5", _)));
    }
  }
}

/// @test Verify that valid MD5 hash values work in WriteObject().
TEST_F(ObjectHashIntegrationTest, WriteObjectWithValueSuccess) {
  auto client = MakeIntegrationTestClient();
  auto object_name = MakeRandomObjectName();
  auto os = client.WriteObject(
      bucket_name_, object_name, MD5HashValue(ComputeMD5Hash(LoremIpsum())),
      DisableCrc32cChecksum(true), IfGenerationMatch(0));
  os << LoremIpsum();
  os.Close();
  auto meta = os.metadata();
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  if (meta->has_metadata("x_emulator_upload")) {
    EXPECT_THAT(meta->metadata(), Contains(Pair("x_emulator_no_crc32c", _)));
    // TODO(#14385) - the emulator does not support this feature for gRPC.
    if (!(UsingEmulator() && UsingGrpc())) {
      EXPECT_THAT(meta->metadata(), Contains(Pair("x_emulator_md5", _)));
    }
  }
}

/// @test Verify that incorrect MD5 hash values work in WriteObject().
TEST_F(ObjectHashIntegrationTest, WriteObjectWithValueFailure) {
  // TODO(#14385) - the emulator does not support this feature for gRPC.
  if (UsingEmulator() && UsingGrpc()) GTEST_SKIP();

  auto client = MakeIntegrationTestClient();
  auto object_name = MakeRandomObjectName();
  auto os = client.WriteObject(
      bucket_name_, object_name, MD5HashValue(ComputeMD5Hash("")),
      DisableCrc32cChecksum(true), IfGenerationMatch(0));
  os << LoremIpsum();
  os.Close();
  auto meta = os.metadata();
  EXPECT_THAT(meta, Not(IsOk()));
}

/// @test Verify that MD5 hash mismatches are reported when the server
/// receives bad data.
TEST_F(ObjectHashIntegrationTest, WriteObjectReceiveBadChecksum) {
  // This test is disabled when not using the emulator as it relies on the
  // emulator to inject faults. The emulator does not support this type of fault
  // injection for gRPC either.
  if (!UsingEmulator() || UsingGrpc()) GTEST_SKIP();
  auto client = MakeIntegrationTestClient();
  auto object_name = MakeRandomObjectName();

  // Create a stream to upload an object.
  ObjectWriteStream stream = client.WriteObject(
      bucket_name_, object_name, DisableMD5Hash(false),
      DisableCrc32cChecksum(true),
      CustomHeader("x-goog-emulator-instructions", "inject-upload-data-error"),
      IfGenerationMatch(0));
  stream << LoremIpsum() << "\n";
  stream.Close();
  EXPECT_TRUE(stream.bad());
  ASSERT_STATUS_OK(stream.metadata());
  ScheduleForDelete(*stream.metadata());
  EXPECT_NE(stream.received_hash(), stream.computed_hash());
}

/// @test Verify that MD5 hash mismatches are reported by default.
TEST_F(ObjectHashIntegrationTest, WriteObjectUploadBadChecksum) {
  // TODO(#14385) - the emulator does not support this feature for gRPC.
  if (UsingEmulator() && UsingGrpc()) GTEST_SKIP();
  auto client = MakeIntegrationTestClient();
  auto object_name = MakeRandomObjectName();

  // Create a stream to upload an object.
  ObjectWriteStream stream = client.WriteObject(
      bucket_name_, object_name, MD5HashValue(ComputeMD5Hash("")),
      DisableCrc32cChecksum(true), IfGenerationMatch(0));
  stream << LoremIpsum() << "\n";
  stream.Close();
  EXPECT_TRUE(stream.bad());
  ASSERT_THAT(stream.metadata(), Not(IsOk()));
}

/// @test Verify that MD5 hashes are disabled by default on downloads.
TEST_P(ObjectHashIntegrationTest, ReadObjectDefault) {
  // TODO(#14385) - the emulator does not support this feature for gRPC.
  if (UsingEmulator() && UsingGrpc()) GTEST_SKIP();

  auto client = MakeIntegrationTestClient();
  auto object_name = MakeRandomObjectName();
  auto meta = client.InsertObject(bucket_name_, object_name, LoremIpsum(),
                                  IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  auto stream = client.ReadObject(bucket_name_, object_name);
  auto const actual = std::string{std::istreambuf_iterator<char>{stream}, {}};
  ASSERT_FALSE(stream.IsOpen());

  EXPECT_EQ(stream.received_hash(), stream.computed_hash());
  EXPECT_THAT(stream.received_hash(), HasSubstr(meta->crc32c()));
}

/// @test Verify that MD5 hashes mismatches are reported (if enabled) on
/// downloads.
TEST_P(ObjectHashIntegrationTest, ReadObjectCorruptedByServerGetc) {
  // This test is disabled when not using the emulator as it relies on the
  // emulator to inject faults. The emulator does not support this type of fault
  // injection for gRPC either.
  if (!UsingEmulator() || UsingGrpc()) GTEST_SKIP();

  auto client = MakeIntegrationTestClient();
  auto object_name = MakeRandomObjectName();
  StatusOr<ObjectMetadata> meta = client.InsertObject(
      bucket_name_, object_name, LoremIpsum(), IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  auto stream = client.ReadObject(
      bucket_name_, object_name, DisableMD5Hash(false),
      DisableCrc32cChecksum(true),
      CustomHeader("x-goog-emulator-instructions", "return-corrupted-data"));

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      try {
        std::string actual(std::istreambuf_iterator<char>{stream},
                           std::istreambuf_iterator<char>{});
      } catch (HashMismatchError const& ex) {
        EXPECT_NE(ex.received_hash(), ex.computed_hash());
        EXPECT_THAT(ex.received_hash(), HasSubstr(meta->md5_hash()));
        EXPECT_THAT(ex.what(), HasSubstr("mismatched hashes"));
        throw;
      },
      HashMismatchError);
#else
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_NE(stream.received_hash(), stream.computed_hash());
  EXPECT_THAT(stream.status(), Not(IsOk()));
}

/// @test Verify that MD5 hashes mismatches are reported (if enabled) on
/// downloads.
TEST_P(ObjectHashIntegrationTest, ReadObjectCorruptedByServerRead) {
  // This test is disabled when not using the emulator as it relies on the
  // emulator to inject faults. The emulator does not support this type of fault
  // injection for gRPC either.
  if (!UsingEmulator() || UsingGrpc()) GTEST_SKIP();

  auto client = MakeIntegrationTestClient();
  auto object_name = MakeRandomObjectName();
  StatusOr<ObjectMetadata> meta =
      client.InsertObject(bucket_name_, object_name, LoremIpsum(),
                          IfGenerationMatch(0), Projection::Full());
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  auto stream = client.ReadObject(
      bucket_name_, object_name, DisableMD5Hash(false),
      DisableCrc32cChecksum(true),
      CustomHeader("x-goog-emulator-instructions", "return-corrupted-data"));

  // Create a buffer large enough to read the full contents.
  std::vector<char> buffer(2 * LoremIpsum().size());
  stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_TRUE(stream.bad());
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THAT(stream.status(), StatusIs(StatusCode::kDataLoss));
  EXPECT_NE(stream.received_hash(), stream.computed_hash());
  EXPECT_EQ(stream.received_hash(), meta->md5_hash());
}

INSTANTIATE_TEST_SUITE_P(ObjectHashIntegrationTestJson,
                         ObjectHashIntegrationTest, ::testing::Values("JSON"));

}  // anonymous namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
