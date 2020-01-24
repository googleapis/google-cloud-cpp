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

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/init_google_mock.h"
#include <gmock/gmock.h>
#include <thread>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::google::cloud::storage::testing::TestPermanentFailure;
using ::testing::HasSubstr;

// Initialized in main() below.
char const* flag_bucket_name;

class ObjectResumableWriteIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {};

TEST_F(ObjectResumableWriteIntegrationTest, WriteWithContentType) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;

  // Create the object, but only if it does not exist already.
  auto os = client->WriteObject(
      bucket_name, object_name, IfGenerationMatch(0),
      WithObjectMetadata(ObjectMetadata().set_content_type("text/plain")));
  os.exceptions(std::ios_base::failbit);
  os << LoremIpsum();
  EXPECT_FALSE(os.resumable_session_id().empty());
  os.Close();
  ASSERT_STATUS_OK(os.metadata());
  ObjectMetadata meta = os.metadata().value();
  EXPECT_EQ(object_name, meta.name());
  EXPECT_EQ(bucket_name, meta.bucket());
  EXPECT_EQ("text/plain", meta.content_type());
  if (UsingTestbench()) {
    EXPECT_TRUE(meta.has_metadata("x_testbench_upload"));
    EXPECT_EQ("resumable", meta.metadata("x_testbench_upload"));
  }

  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
}

TEST_F(ObjectResumableWriteIntegrationTest, WriteWithContentTypeFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto bucket_name = MakeRandomBucketName();
  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;

  // Create the object, but only if it does not exist already.
  auto os = client->WriteObject(
      bucket_name, object_name, IfGenerationMatch(0),
      WithObjectMetadata(ObjectMetadata().set_content_type("text/plain")));
  EXPECT_TRUE(os.bad());
  EXPECT_FALSE(os.metadata().status().ok())
      << ", status=" << os.metadata().status();
}

TEST_F(ObjectResumableWriteIntegrationTest, WriteWithUseResumable) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;

  // Create the object, but only if it does not exist already.
  auto os = client->WriteObject(bucket_name, object_name, IfGenerationMatch(0),
                                NewResumableUploadSession());
  os.exceptions(std::ios_base::failbit);
  os << LoremIpsum();
  EXPECT_FALSE(os.resumable_session_id().empty());
  os.Close();
  ASSERT_STATUS_OK(os.metadata());
  ObjectMetadata meta = os.metadata().value();
  EXPECT_EQ(object_name, meta.name());
  EXPECT_EQ(bucket_name, meta.bucket());
  if (UsingTestbench()) {
    EXPECT_TRUE(meta.has_metadata("x_testbench_upload"));
    EXPECT_EQ("resumable", meta.metadata("x_testbench_upload"));
  }

  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
}

TEST_F(ObjectResumableWriteIntegrationTest, WriteResume) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;

  // Create the object, but only if it does not exist already.
  std::string session_id;
  {
    auto old_os =
        client->WriteObject(bucket_name, object_name, IfGenerationMatch(0),
                            NewResumableUploadSession());
    ASSERT_TRUE(old_os.good()) << "status=" << old_os.metadata().status();
    session_id = old_os.resumable_session_id();
    std::move(old_os).Suspend();
  }

  auto os = client->WriteObject(bucket_name, object_name,
                                RestoreResumableUploadSession(session_id));
  ASSERT_TRUE(os.good()) << "status=" << os.metadata().status();
  EXPECT_EQ(session_id, os.resumable_session_id());
  os << LoremIpsum();
  os.Close();
  ASSERT_STATUS_OK(os.metadata());
  ObjectMetadata meta = os.metadata().value();
  EXPECT_EQ(object_name, meta.name());
  EXPECT_EQ(bucket_name, meta.bucket());
  if (UsingTestbench()) {
    EXPECT_TRUE(meta.has_metadata("x_testbench_upload"));
    EXPECT_EQ("resumable", meta.metadata("x_testbench_upload"));
  }

  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
}

TEST_F(ObjectResumableWriteIntegrationTest, WriteResumeFinalizedUpload) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // Start a resumable upload and finalize the upload.
  std::string session_id;
  {
    auto old_os =
        client->WriteObject(bucket_name, object_name, IfGenerationMatch(0),
                            NewResumableUploadSession());
    ASSERT_TRUE(old_os.good()) << "status=" << old_os.metadata().status();
    session_id = old_os.resumable_session_id();
    old_os << LoremIpsum();
  }

  auto os = client->WriteObject(bucket_name, object_name,
                                RestoreResumableUploadSession(session_id));
  EXPECT_FALSE(os.IsOpen());
  EXPECT_EQ(session_id, os.resumable_session_id());
  ASSERT_STATUS_OK(os.metadata());
  ObjectMetadata meta = os.metadata().value();
  EXPECT_EQ(object_name, meta.name());
  EXPECT_EQ(bucket_name, meta.bucket());
  if (UsingTestbench()) {
    EXPECT_TRUE(meta.has_metadata("x_testbench_upload"));
    EXPECT_EQ("resumable", meta.metadata("x_testbench_upload"));
  }

  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
}

TEST_F(ObjectResumableWriteIntegrationTest, StreamingWriteFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name, object_name, expected, IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);

  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name, meta->bucket());

  auto os = client->WriteObject(bucket_name, object_name, IfGenerationMatch(0),
                                NewResumableUploadSession());
  os << "Expected failure data:\n" << LoremIpsum();

  // This operation should fail because the object already exists.
  os.Close();
  EXPECT_TRUE(os.bad());
  EXPECT_FALSE(os.metadata().ok());
  EXPECT_EQ(StatusCode::kFailedPrecondition, os.metadata().status().code());

  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
}

TEST_F(ObjectResumableWriteIntegrationTest, StreamingWriteSlow) {
  std::chrono::seconds timeout(3);
  auto retry_policy =
      LimitedTimeRetryPolicy(/*maximum_duration=*/timeout).clone();
  StatusOr<Client> client = MakeIntegrationTestClient(std::move(retry_policy));
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  auto data = MakeRandomData(1024 * 1024);

  auto os = client->WriteObject(bucket_name, object_name, IfGenerationMatch(0));
  os.write(data.data(), data.size());
  EXPECT_FALSE(os.bad());
  std::cout << "Sleeping to force timeout ... " << std::flush;
  std::this_thread::sleep_for(2 * timeout);
  std::cout << "DONE\n";

  os.write(data.data(), data.size());
  EXPECT_FALSE(os.bad());

  // This operation should fail because the object already exists.
  os.Close();
  EXPECT_FALSE(os.bad());
  EXPECT_STATUS_OK(os.metadata());

  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
}

TEST_F(ObjectResumableWriteIntegrationTest, WithXUploadContentLength) {
  auto const MiB = 1024 * 1024L;
  auto const chunk_size = 2 * MiB;

  auto options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(options);
  Client client(options->SetUploadBufferSize(chunk_size));

  std::string bucket_name = flag_bucket_name;

  auto const chunk = MakeRandomData(chunk_size);

  for (auto const desired_size : {2 * MiB, 3 * MiB, 4 * MiB}) {
    auto object_name = MakeRandomObjectName();
    SCOPED_TRACE("Testing with desired_size=" + std::to_string(desired_size) +
                 ", name=" + object_name);
    auto os = client.WriteObject(
        bucket_name, object_name, IfGenerationMatch(0),
        CustomHeader("X-Upload-Content-Length", std::to_string(desired_size)));
    auto offset = 0L;
    while (offset < desired_size) {
      auto const n = (std::min)(desired_size - offset, chunk_size);
      os.write(chunk.data(), n);
      ASSERT_FALSE(os.bad());
      offset += n;
    }

    os.Close();
    EXPECT_FALSE(os.bad());
    EXPECT_STATUS_OK(os.metadata());
    EXPECT_EQ(desired_size, os.metadata()->size());

    auto status = client.DeleteObject(bucket_name, object_name);
    EXPECT_STATUS_OK(status);
  }
}

TEST_F(ObjectResumableWriteIntegrationTest, WithXUploadContentLengthRandom) {
  auto const quantum = 256 * 1024L;
  auto const chunk_size = 2 * quantum;

  auto options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(options);
  Client client(options->SetUploadBufferSize(chunk_size));

  std::string bucket_name = flag_bucket_name;

  auto const chunk = MakeRandomData(chunk_size);

  std::uniform_int_distribution<long> size_gen(quantum, 5 * quantum);
  for (int i = 0; i != 10; ++i) {
    auto object_name = MakeRandomObjectName();
    auto const desired_size = size_gen(generator_);
    SCOPED_TRACE("Testing with desired_size=" + std::to_string(desired_size) +
                 ", name=" + object_name);
    auto os = client.WriteObject(
        bucket_name, object_name, IfGenerationMatch(0),
        CustomHeader("X-Upload-Content-Length", std::to_string(desired_size)));
    auto offset = 0L;
    while (offset < desired_size) {
      auto const n = (std::min)(desired_size - offset, chunk_size);
      os.write(chunk.data(), n);
      ASSERT_FALSE(os.bad());
      offset += n;
    }

    os.Close();
    EXPECT_FALSE(os.bad());
    EXPECT_STATUS_OK(os.metadata());
    EXPECT_EQ(desired_size, os.metadata()->size());

    auto status = client.DeleteObject(bucket_name, object_name);
    EXPECT_STATUS_OK(status);
  }
}

TEST_F(ObjectResumableWriteIntegrationTest, WithInvalidXUploadContentLength) {
  if (UsingTestbench()) return;
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;

  auto const chunk_size = 256 * 1024L;
  auto const chunk = MakeRandomData(chunk_size);

  auto object_name = MakeRandomObjectName();
  auto const desired_size = 5 * chunk_size;
  // Use an invalid value in the X-Upload-Content-Length header, the library
  // should return an error.
  auto os = client->WriteObject(
      bucket_name, object_name, IfGenerationMatch(0),
      CustomHeader("X-Upload-Content-Length", std::to_string(3 * chunk_size)));
  auto offset = 0L;
  while (offset < desired_size) {
    auto const n = (std::min)(desired_size - offset, chunk_size);
    os.write(chunk.data(), n);
    ASSERT_FALSE(os.bad());
    offset += n;
  }

  // This operation should fail because the x-upload-content-length header does
  // not match the amount of data sent in the upload.
  os.Close();
  EXPECT_TRUE(os.bad());
  EXPECT_FALSE(os.metadata().ok());
  // No need to delete the object, as it is never created.
}

}  // anonymous namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

int main(int argc, char* argv[]) {
  google::cloud::testing_util::InitGoogleMock(argc, argv);

  // Make sure the arguments are valid.
  if (argc != 2) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(argv[0]).find_last_of('/');
    std::cerr << "Usage: " << cmd.substr(last_slash + 1) << " <bucket-name>\n";
    return 1;
  }

  google::cloud::storage::flag_bucket_name = argv[1];

  return RUN_ALL_TESTS();
}
