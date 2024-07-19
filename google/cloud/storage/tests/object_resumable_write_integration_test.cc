// Copyright 2018 Google LLC
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
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <thread>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::AnyOf;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;

class ObjectResumableWriteIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {
 protected:
  void SetUp() override {
    google::cloud::storage::testing::StorageIntegrationTest::SetUp();
    bucket_name_ = google::cloud::internal::GetEnv(
                       "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME")
                       .value_or("");
    ASSERT_FALSE(bucket_name_.empty());
  }

  std::string bucket_name_;
};

TEST_F(ObjectResumableWriteIntegrationTest, WriteWithContentType) {
  auto client = MakeIntegrationTestClient();
  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;

  // Create the object, but only if it does not exist already.
  auto os = client.WriteObject(
      bucket_name_, object_name, IfGenerationMatch(0),
      WithObjectMetadata(ObjectMetadata().set_content_type("text/plain")));
  os.exceptions(std::ios_base::failbit);
  os << LoremIpsum();
  EXPECT_FALSE(os.resumable_session_id().empty());
  os.Close();
  ASSERT_STATUS_OK(os.metadata());
  ObjectMetadata meta = os.metadata().value();
  ScheduleForDelete(meta);
  EXPECT_EQ(object_name, meta.name());
  EXPECT_EQ(bucket_name_, meta.bucket());
  EXPECT_EQ("text/plain", meta.content_type());
  if (UsingEmulator()) {
    EXPECT_TRUE(meta.has_metadata("x_emulator_upload"));
    EXPECT_EQ("resumable", meta.metadata("x_emulator_upload"));
  }
}

TEST_F(ObjectResumableWriteIntegrationTest, WriteWithContentTypeFailure) {
  auto client = MakeIntegrationTestClient();
  auto bucket_name = MakeRandomBucketName();
  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;

  // Create the object, but only if it does not exist already.
  auto os = client.WriteObject(
      bucket_name, object_name, IfGenerationMatch(0),
      WithObjectMetadata(ObjectMetadata().set_content_type("text/plain")));
  EXPECT_TRUE(os.bad());
  EXPECT_FALSE(os.metadata().status().ok())
      << ", status=" << os.metadata().status();
}

TEST_F(ObjectResumableWriteIntegrationTest, WriteWithUseResumable) {
  auto client = MakeIntegrationTestClient();
  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;

  // Create the object, but only if it does not exist already.
  auto os = client.WriteObject(bucket_name_, object_name, IfGenerationMatch(0),
                               NewResumableUploadSession());
  os.exceptions(std::ios_base::failbit);
  os << LoremIpsum();
  EXPECT_FALSE(os.resumable_session_id().empty());
  os.Close();
  ASSERT_STATUS_OK(os.metadata());
  ObjectMetadata meta = os.metadata().value();
  ScheduleForDelete(meta);
  EXPECT_EQ(object_name, meta.name());
  EXPECT_EQ(bucket_name_, meta.bucket());
  if (UsingEmulator()) {
    EXPECT_TRUE(meta.has_metadata("x_emulator_upload"));
    EXPECT_EQ("resumable", meta.metadata("x_emulator_upload"));
  }
}

TEST_F(ObjectResumableWriteIntegrationTest, WriteResume) {
  auto client = MakeIntegrationTestClient();
  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;

  // Create the object, but only if it does not exist already.
  std::string session_id;
  {
    auto old_os =
        client.WriteObject(bucket_name_, object_name, IfGenerationMatch(0),
                           NewResumableUploadSession());
    ASSERT_TRUE(old_os.good()) << "status=" << old_os.metadata().status();
    session_id = old_os.resumable_session_id();
    std::move(old_os).Suspend();
  }

  auto os = client.WriteObject(bucket_name_, object_name,
                               RestoreResumableUploadSession(session_id));
  ASSERT_TRUE(os.good()) << "status=" << os.metadata().status();
  EXPECT_EQ(session_id, os.resumable_session_id());
  os << LoremIpsum();
  os.Close();
  ASSERT_STATUS_OK(os.metadata());
  ObjectMetadata meta = os.metadata().value();
  ScheduleForDelete(meta);
  EXPECT_EQ(object_name, meta.name());
  EXPECT_EQ(bucket_name_, meta.bucket());
  if (UsingEmulator()) {
    EXPECT_TRUE(meta.has_metadata("x_emulator_upload"));
    EXPECT_EQ("resumable", meta.metadata("x_emulator_upload"));
  }
}

TEST_F(ObjectResumableWriteIntegrationTest, WriteResumeWithPartial) {
  auto client = MakeIntegrationTestClient();
  auto object_name = MakeRandomObjectName();
  auto constexpr kUploadQuantum = 256 * 1024;
  auto const q0 = MakeRandomData(kUploadQuantum);
  auto const q1 = MakeRandomData(2 * kUploadQuantum);
  auto const q2 = MakeRandomData(3 * kUploadQuantum);

  auto const session_id = [&]() {
    // Start the upload, add some data, and flush it.
    auto os =
        client.WriteObject(bucket_name_, object_name, IfGenerationMatch(0));
    EXPECT_TRUE(os.good()) << "status=" << os.last_status();
    os.write(q0.data(), q0.size());
    os.flush();
    EXPECT_STATUS_OK(os.last_status());
    auto id = os.resumable_session_id();
    std::move(os).Suspend();
    return id;
  }();

  auto expected_committed_size = static_cast<std::uint64_t>(q0.size());
  for (auto const& data : {q1, q2}) {
    auto os = client.WriteObject(bucket_name_, object_name,
                                 RestoreResumableUploadSession(session_id));
    ASSERT_TRUE(os.good()) << "status=" << os.last_status();
    EXPECT_EQ(os.resumable_session_id(), session_id);
    EXPECT_EQ(os.next_expected_byte(), expected_committed_size);
    os.write(data.data(), data.size());
    os.flush();
    EXPECT_STATUS_OK(os.last_status());
    expected_committed_size += data.size();
    std::move(os).Suspend();
  }

  auto os = client.WriteObject(bucket_name_, object_name,
                               RestoreResumableUploadSession(session_id));
  ASSERT_TRUE(os.good()) << "status=" << os.last_status();
  EXPECT_EQ(os.resumable_session_id(), session_id);
  EXPECT_EQ(os.next_expected_byte(), expected_committed_size);
  os.Close();
  ASSERT_STATUS_OK(os.metadata());
  auto meta = os.metadata().value();
  ScheduleForDelete(meta);
  EXPECT_EQ(object_name, meta.name());
  EXPECT_EQ(bucket_name_, meta.bucket());

  auto stream = client.ReadObject(bucket_name_, object_name);
  ASSERT_STATUS_OK(stream.status());
  auto const actual = std::string{std::istreambuf_iterator<char>{stream}, {}};
  EXPECT_EQ(q0 + q1 + q2, actual);
}

TEST_F(ObjectResumableWriteIntegrationTest, WriteNotChunked) {
  auto client = MakeIntegrationTestClient();
  auto object_name = MakeRandomObjectName();
  auto constexpr kUploadQuantum = 256 * 1024;
  auto const payload =
      std::string(internal::ClientImplDetails::GetConnection(client)
                      ->options()
                      .get<UploadBufferSizeOption>(),
                  '*');
  auto const header = MakeRandomData(kUploadQuantum / 2);

  auto os = client.WriteObject(bucket_name_, object_name, IfGenerationMatch(0));
  ASSERT_TRUE(os.good()) << "status=" << os.metadata().status();
  // Write a small header that is too small to be flushed...
  os.write(header.data(), header.size());
  for (int i = 0; i != 3; ++i) {
    // Append some data that is large enough to flush, this creates a call to
    // UploadChunk() with two buffers, and that triggered chunked transfer
    // encoding, even though the size is known which wastes bandwidth.
    os.write(payload.data(), payload.size());
    ASSERT_TRUE(os.good());
  }
  os.Close();
  ASSERT_STATUS_OK(os.metadata());
  ObjectMetadata meta = os.metadata().value();
  ScheduleForDelete(meta);
  if (meta.has_metadata("x_emulator_upload")) {
    EXPECT_EQ("resumable", meta.metadata("x_emulator_upload"));
  }
  if (meta.has_metadata("x_emulator_transfer_encoding")) {
    EXPECT_THAT(meta.metadata("x_emulator_transfer_encoding"),
                Not(HasSubstr("chunked")));
  }
}

TEST_F(ObjectResumableWriteIntegrationTest, WriteResumeFinalizedUpload) {
  auto client = MakeIntegrationTestClient();
  auto object_name = MakeRandomObjectName();
  // Start a resumable upload and finalize the upload.
  std::string session_id;
  {
    auto old_os =
        client.WriteObject(bucket_name_, object_name, IfGenerationMatch(0),
                           NewResumableUploadSession());
    ASSERT_TRUE(old_os.good()) << "status=" << old_os.metadata().status();
    session_id = old_os.resumable_session_id();
    old_os << LoremIpsum();
  }

  auto os = client.WriteObject(bucket_name_, object_name,
                               RestoreResumableUploadSession(session_id));
  EXPECT_FALSE(os.IsOpen());
  EXPECT_EQ(session_id, os.resumable_session_id());
  ASSERT_STATUS_OK(os.metadata());
  ScheduleForDelete(*os.metadata());
  ObjectMetadata meta = os.metadata().value();
  EXPECT_EQ(object_name, meta.name());
  EXPECT_EQ(bucket_name_, meta.bucket());
  if (UsingEmulator()) {
    EXPECT_TRUE(meta.has_metadata("x_emulator_upload"));
    EXPECT_EQ("resumable", meta.metadata("x_emulator_upload"));
  }
}

TEST_F(ObjectResumableWriteIntegrationTest, StreamingWriteFailure) {
  auto client = MakeIntegrationTestClient();

  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client.InsertObject(
      bucket_name_, object_name, expected, IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);

  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name_, meta->bucket());

  auto os = client.WriteObject(bucket_name_, object_name, IfGenerationMatch(0),
                               NewResumableUploadSession());
  os << "Expected failure data:\n" << LoremIpsum();

  // This operation should fail because the object already exists.
  os.Close();
  EXPECT_TRUE(os.bad());
  // The GCS server returns a different error code depending on the
  // protocol (REST vs. gRPC) used
  EXPECT_THAT(
      os.metadata().status().code(),
      AnyOf(Eq(StatusCode::kFailedPrecondition), Eq(StatusCode::kAborted)))
      << " status=" << os.metadata().status();

  if (os.metadata().status().code() == StatusCode::kFailedPrecondition &&
      !UsingEmulator() && !UsingGrpc()) {
    EXPECT_THAT(os.metadata().status().message(), Not(IsEmpty()));
    EXPECT_EQ(os.metadata().status().error_info().domain(), "global");
    EXPECT_EQ(os.metadata().status().error_info().reason(), "conditionNotMet");
  }

  auto status = client.DeleteObject(bucket_name_, object_name);
  EXPECT_STATUS_OK(status);
}

TEST_F(ObjectResumableWriteIntegrationTest, StreamingWriteSlow) {
  auto const timeout = std::chrono::seconds(3);
  auto client = MakeIntegrationTestClient(Options{}.set<RetryPolicyOption>(
      LimitedTimeRetryPolicy(/*maximum_duration=*/timeout).clone()));

  auto object_name = MakeRandomObjectName();

  auto data = MakeRandomData(1024 * 1024);

  auto os = client.WriteObject(bucket_name_, object_name, IfGenerationMatch(0));
  os.write(data.data(), data.size());
  EXPECT_FALSE(os.bad());
  std::cout << "Sleeping to force timeout ... " << std::flush;
  std::this_thread::sleep_for(2 * timeout);
  std::cout << "DONE\n";

  os.write(data.data(), data.size());
  EXPECT_FALSE(os.bad());

  os.Close();
  ASSERT_STATUS_OK(os.metadata());
  ScheduleForDelete(*os.metadata());
  EXPECT_FALSE(os.bad());
}

TEST_F(ObjectResumableWriteIntegrationTest, WithXUploadContentLength) {
  if (UsingEmulator() || UsingGrpc()) GTEST_SKIP();
  auto constexpr kMiB = 1024 * 1024L;
  auto constexpr kChunkSize = 2 * kMiB;

  Client client(Options{}.set<UploadBufferSizeOption>(kChunkSize));

  auto const chunk = MakeRandomData(kChunkSize);

  for (auto const desired_size : {2 * kMiB, 3 * kMiB, 4 * kMiB}) {
    auto object_name = MakeRandomObjectName();
    SCOPED_TRACE("Testing with desired_size=" + std::to_string(desired_size) +
                 ", name=" + object_name);
    auto os = client.WriteObject(
        bucket_name_, object_name, IfGenerationMatch(0),
        CustomHeader("X-Upload-Content-Length", std::to_string(desired_size)));
    auto offset = 0L;
    while (offset < desired_size) {
      auto const n = (std::min)(desired_size - offset, kChunkSize);
      os.write(chunk.data(), n);
      ASSERT_FALSE(os.bad());
      offset += n;
    }

    os.Close();
    ASSERT_STATUS_OK(os.metadata());
    ScheduleForDelete(*os.metadata());
    EXPECT_FALSE(os.bad()) << *os.metadata();
    EXPECT_EQ(desired_size, os.metadata()->size());
    EXPECT_EQ(desired_size, offset);
  }
}

TEST_F(ObjectResumableWriteIntegrationTest, WithXUploadContentLengthRandom) {
  if (UsingGrpc()) GTEST_SKIP();
  auto constexpr kQuantum = 256 * 1024L;
  size_t constexpr kChunkSize = 2 * kQuantum;

  Client client(Options{}.set<UploadBufferSizeOption>(kChunkSize));

  auto const chunk = MakeRandomData(kChunkSize);

  std::uniform_int_distribution<std::size_t> size_gen(kQuantum, 5 * kQuantum);
  for (int i = 0; i != 10; ++i) {
    auto object_name = MakeRandomObjectName();
    auto const desired_size = size_gen(generator_);
    SCOPED_TRACE("Testing with desired_size=" + std::to_string(desired_size) +
                 ", name=" + object_name);
    auto os = client.WriteObject(
        bucket_name_, object_name, IfGenerationMatch(0),
        CustomHeader("X-Upload-Content-Length", std::to_string(desired_size)));
    std::size_t offset = 0L;
    while (offset < desired_size) {
      auto const n = (std::min)(desired_size - offset, kChunkSize);
      os.write(chunk.data(), n);
      ASSERT_FALSE(os.bad());
      offset += n;
    }

    os.Close();
    ASSERT_STATUS_OK(os.metadata());
    ScheduleForDelete(*os.metadata());
    EXPECT_FALSE(os.bad());
    EXPECT_EQ(desired_size, os.metadata()->size());
  }
}

TEST_F(ObjectResumableWriteIntegrationTest, WithInvalidXUploadContentLength) {
  if (UsingEmulator() || UsingGrpc()) GTEST_SKIP();
  auto client = MakeIntegrationTestClient();

  auto constexpr kChunkSize = 256 * 1024L;
  auto const chunk = MakeRandomData(kChunkSize);

  auto object_name = MakeRandomObjectName();
  auto const desired_size = 5 * kChunkSize;
  // Use an invalid value in the X-Upload-Content-Length header, the library
  // should return an error.
  auto os = client.WriteObject(
      bucket_name_, object_name, IfGenerationMatch(0),
      CustomHeader("X-Upload-Content-Length", std::to_string(3 * kChunkSize)));
  auto offset = 0L;
  while (offset < desired_size) {
    auto const n = (std::min)(desired_size - offset, kChunkSize);
    os.write(chunk.data(), n);
    ASSERT_FALSE(os.bad());
    offset += n;
  }

  // This operation should fail because the x-upload-content-length header
  // does not match the amount of data sent in the upload.
  os.Close();
  EXPECT_TRUE(os.bad());
  EXPECT_FALSE(os.metadata().ok());
  // No need to delete the object, as it is never created.
}

}  // anonymous namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
