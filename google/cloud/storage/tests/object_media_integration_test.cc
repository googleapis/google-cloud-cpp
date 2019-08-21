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
  void SetUp() override {
    google::cloud::storage::testing::StorageIntegrationTest::SetUp();
    endpoint_.SetUp();
  }
  void TearDown() override {
    endpoint_.TearDown();
    google::cloud::storage::testing::StorageIntegrationTest::TearDown();
  }

  google::cloud::testing_util::EnvironmentVariableRestore endpoint_;
};

TEST_F(ObjectMediaIntegrationTest, StreamingReadClose) {
  StatusOr<Client> client = MakeIntegrationTestClient();
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

  // Create an iostream to read the object back.
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
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // This produces a 64 KiB text object. Normally applications should download
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

  // Create an iostream to read the object back.
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
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // This produces a 64 KiB text object. Normally applications should download
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

  // Create an iostream to read the object back.
  auto stream = client->ReadObject(bucket_name, object_name,
                                   ReadRange(1 * chunk, 2 * chunk));
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(1 * chunk, actual.size());
  EXPECT_EQ(large_text.substr(1 * chunk, 1 * chunk), actual);

  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
}

/// @test Read a portion of a relatively large object using the JSON API.
TEST_F(ObjectMediaIntegrationTest, ReadFromOffsetJSON) {
  // The testbench always requires multiple iterations to copy this object.
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // This produces a 64 KiB text object. Normally applications should download
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

  // Create an iostream to read the object back.
  auto stream =
      client->ReadObject(bucket_name, object_name, ReadFromOffset(2 * chunk),
                         IfGenerationNotMatch(0));
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(2 * chunk, actual.size());
  EXPECT_EQ(large_text.substr(2 * chunk), actual);

  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
}

/// @test Read a portion of a relatively large object using the XML API.
TEST_F(ObjectMediaIntegrationTest, ReadFromOffsetXml) {
  // The testbench always requires multiple iterations to copy this object.
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // This produces a 64 KiB text object. Normally applications should download
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

  // Create an iostream to read the object back.
  auto stream =
      client->ReadObject(bucket_name, object_name, ReadFromOffset(2 * chunk));
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(2 * chunk, actual.size());
  EXPECT_EQ(large_text.substr(2 * chunk), actual);

  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
}

/// @test Read a relatively large object using chunks of different sizes.
TEST_F(ObjectMediaIntegrationTest, ReadMixedChunks) {
  // The testbench always requires multiple iterations to copy this object.
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // This produces a 4 MiB text object. Normally applications should download
  // much larger chunks from GCS, but it is really hard to figure out what is
  // broken when the error messages are in the MiB ranges.
  auto const object_size = 4 * 1024 * 1024LL;
  auto const lines = object_size / 128;
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

  // Create an iostream to read the object back.
  auto stream = client->ReadObject(bucket_name, object_name);

  // Read the object with a random mix of std::getline(), and stream.read()
  // it is unlikely that any application would actually read like this,
  // nevertheless the library should work in this case.
  std::string actual;
  actual.reserve(object_size);
  auto const maximum_chunk_size = 256 * 1024L;
  auto const minimum_chunk_size = 16;
  std::vector<char> buffer(maximum_chunk_size);
  std::uniform_int_distribution<int> chunk_size_generator(0,
                                                          maximum_chunk_size);
  do {
    auto size = chunk_size_generator(generator_);
    if (size < minimum_chunk_size) {
      std::string line;
      if (std::getline(stream, line)) {
        actual.append(line);
        actual.append("\n");
      }
    } else {
      stream.read(buffer.data(), buffer.size());
      actual.append(buffer.data(), buffer.data() + stream.gcount());
    }
  } while (stream);

  EXPECT_EQ(object_size, actual.size());
  EXPECT_EQ(large_text, actual);

  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
}

/// @test Read the last chunk of an object.
TEST_F(ObjectMediaIntegrationTest, ReadLastChunk) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // This produces an object larger than 3MiB, but with a size that is not a
  // multiple of 128KiB.
  auto constexpr kKiB = 1024L;
  auto constexpr kMiB = 1024L * kKiB;
  auto constexpr object_size = 3 * kMiB + 129 * kKiB;
  auto constexpr line_size = 128;
  auto constexpr lines = object_size / line_size;
  std::string large_text;
  for (long i = 0; i != lines; ++i) {
    auto line = google::cloud::internal::Sample(generator_, line_size - 1,
                                                "abcdefghijklmnopqrstuvwxyz"
                                                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                                "012456789");
    large_text += line + "\n";
  }
  static_assert(object_size % line_size == 0,
                "Object must be multiple of line size");
  EXPECT_EQ(object_size, large_text.size());

  StatusOr<ObjectMetadata> source_meta = client->InsertObject(
      bucket_name, object_name, large_text, IfGenerationMatch(0));
  ASSERT_STATUS_OK(source_meta);

  EXPECT_EQ(object_name, source_meta->name());
  EXPECT_EQ(bucket_name, source_meta->bucket());

  // Create an iostream to read the last 256KiB of the object, but simulate an
  // application that does not know how large that last chunk is.
  auto stream = client->ReadObject(bucket_name, object_name,
                                   ReadRange(3 * kMiB, 4 * kMiB));

  std::vector<char> buffer(1 * kMiB);
  stream.read(buffer.data(), buffer.size());
  EXPECT_TRUE(stream.eof());
  EXPECT_TRUE(stream.fail());
  EXPECT_FALSE(stream.bad());
  EXPECT_EQ(object_size - 3 * kMiB, stream.gcount());
  std::string actual(buffer.data(), stream.gcount());
  EXPECT_EQ(large_text.substr(3 * kMiB), actual);

  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
}

/// @test Read an object by chunks of equal size.
TEST_F(ObjectMediaIntegrationTest, ReadByChunk) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // This produces a 3.25 MiB text object.
  auto constexpr kKiB = 1024L;
  auto constexpr kMiB = 1024L * kKiB;
  auto constexpr object_size = 3 * kMiB + 129 * kKiB;
  auto constexpr line_size = 128;
  auto constexpr lines = object_size / line_size;
  std::string large_text;
  for (long i = 0; i != lines; ++i) {
    auto line = google::cloud::internal::Sample(generator_, line_size - 1,
                                                "abcdefghijklmnopqrstuvwxyz"
                                                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                                "012456789");
    large_text += line + "\n";
  }
  static_assert(object_size % line_size == 0,
                "Object must be multiple of line size");
  EXPECT_EQ(object_size, large_text.size());

  StatusOr<ObjectMetadata> source_meta = client->InsertObject(
      bucket_name, object_name, large_text, IfGenerationMatch(0));
  ASSERT_STATUS_OK(source_meta);

  EXPECT_EQ(object_name, source_meta->name());
  EXPECT_EQ(bucket_name, source_meta->bucket());

  std::vector<char> buffer(1 * kMiB);
  for (int i = 0; i != 3; ++i) {
    SCOPED_TRACE("Reading chunk from object, chunk=" + std::to_string(i));
    // Create an iostream to read from (i * kMiB) to ((i + 1) * kMiB).
    auto stream = client->ReadObject(bucket_name, object_name,
                                     ReadRange(i * kMiB, (i + 1) * kMiB));

    stream.read(buffer.data(), buffer.size());
    EXPECT_FALSE(stream.eof());
    EXPECT_FALSE(stream.fail());
    EXPECT_FALSE(stream.bad());
    EXPECT_EQ(1 * kMiB, stream.gcount());
    std::string actual(buffer.data(), stream.gcount());

    EXPECT_EQ(large_text.substr(i * kMiB, 1 * kMiB), actual);
  }

  // Create an iostream to read the last 256KiB of the object, but simulate an
  // application that does not know how large that last chunk is.
  auto stream = client->ReadObject(bucket_name, object_name,
                                   ReadRange(3 * kMiB, 4 * kMiB));

  stream.read(buffer.data(), buffer.size());
  EXPECT_TRUE(stream.eof());
  EXPECT_TRUE(stream.fail());
  EXPECT_FALSE(stream.bad());
  EXPECT_EQ(object_size - 3 * kMiB, stream.gcount());
  std::string actual(buffer.data(), stream.gcount());
  auto expected = large_text.substr(3 * kMiB);
  EXPECT_EQ(expected.size(), actual.size());
  EXPECT_EQ(expected, actual);

  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
}

TEST_F(ObjectMediaIntegrationTest, ConnectionFailureReadJSON) {
  Client client{ClientOptions(oauth2::CreateAnonymousCredentials())
                    .set_endpoint("http://localhost:1"),
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
                                  "http://localhost:1");
  Client client{ClientOptions(oauth2::CreateAnonymousCredentials())
                    .set_endpoint("http://localhost:1"),
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
                    .set_endpoint("http://localhost:1"),
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
                                  "http://localhost:1");
  Client client{ClientOptions(oauth2::CreateAnonymousCredentials())
                    .set_endpoint("http://localhost:1"),
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
                                  "http://localhost:1");
  Client client{ClientOptions(oauth2::CreateAnonymousCredentials())
                    .set_endpoint("http://localhost:1"),
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
                                  "http://localhost:1");
  Client client{ClientOptions(oauth2::CreateAnonymousCredentials())
                    .set_endpoint("http://localhost:1"),
                LimitedErrorCountRetryPolicy(2)};

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();
  auto file_name = MakeRandomObjectName();

  std::ofstream(file_name, std::ios::binary) << LoremIpsum();

  StatusOr<ObjectMetadata> meta =
      client.UploadFile(file_name, bucket_name, object_name);
  EXPECT_FALSE(meta.ok()) << "value=" << meta.value();
  EXPECT_EQ(StatusCode::kUnavailable, meta.status().code())
      << ", status=" << meta.status();

  EXPECT_EQ(0, std::remove(file_name.c_str()));
}

TEST_F(ObjectMediaIntegrationTest, StreamingReadTimeout) {
  if (!UsingTestbench()) {
    return;
  }

  auto options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(options);

  Client client(options->set_download_stall_timeout(std::chrono::seconds(3)),
                LimitedErrorCountRetryPolicy(3));

  std::string const bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // Construct an object large enough to not be downloaded in the first chunk.
  auto const object_size = 512 * 1024L;
  auto large_text = MakeRandomData(object_size);

  // Create an object with the contents to download.
  StatusOr<ObjectMetadata> source_meta = client.InsertObject(
      bucket_name, object_name, large_text, IfGenerationMatch(0));
  ASSERT_STATUS_OK(source_meta);

  auto stream = client.ReadObject(
      bucket_name, object_name,
      CustomHeader("x-goog-testbench-instructions", "stall-always"));

  std::vector<char> buffer(object_size);
  stream.read(buffer.data(), object_size);
  EXPECT_TRUE(stream.bad());
  EXPECT_FALSE(stream.status().ok());

  auto status = client.DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);
}

TEST_F(ObjectMediaIntegrationTest, StreamingReadTimeoutContinues) {
  if (!UsingTestbench()) {
    return;
  }

  auto options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(options);

  Client client(options->set_download_stall_timeout(std::chrono::seconds(3)),
                LimitedErrorCountRetryPolicy(10));

  std::string const bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // Construct an object large enough to not be downloaded in the first chunk.
  auto const object_size = 512 * 1024L;
  auto large_text = MakeRandomData(object_size);
  EXPECT_EQ(object_size, large_text.size());

  // Create an object with the contents to download.
  StatusOr<ObjectMetadata> source_meta = client.InsertObject(
      bucket_name, object_name, large_text, IfGenerationMatch(0));
  ASSERT_STATUS_OK(source_meta);

  auto stream = client.ReadObject(
      bucket_name, object_name,
      CustomHeader("x-goog-testbench-instructions", "stall-at-256KiB"));

  std::vector<char> buffer(object_size);
  stream.read(buffer.data(), object_size);
  EXPECT_STATUS_OK(stream.status());
  EXPECT_EQ(object_size, stream.gcount());
  stream.read(buffer.data(), object_size);

  EXPECT_TRUE(stream.eof());
  EXPECT_EQ(0, stream.gcount());
  EXPECT_STATUS_OK(stream.status());

  auto status = client.DeleteObject(bucket_name, object_name);
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
