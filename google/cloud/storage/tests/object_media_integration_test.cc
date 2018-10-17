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
#include "google/cloud/log.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/testing_util/init_google_mock.h"
#include <gmock/gmock.h>
#include <cstdio>
#include <fstream>
#include <regex>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
using ::testing::HasSubstr;
namespace {
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
  return std::getenv("CLOUD_STORAGE_TESTBENCH_ENDPOINT") != nullptr;
}

TEST_F(ObjectMediaIntegrationTest, UploadFile) {
  Client client;
  auto file_name = MakeRandomObjectName();
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
  std::remove(file_name.c_str());
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
  auto file_name = MakeRandomObjectName();
  auto bucket_name = ObjectMediaTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // Create the file.
  std::ofstream(file_name) << LoremIpsum();

  // Create the object.
  ObjectMetadata meta = client.InsertObject(bucket_name, object_name,
                                            LoremIpsum(), IfGenerationMatch(0));

  // Trying to upload the file to the same object with the IfGenerationMatch(0)
  // condition should fail because the object already exists.
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      try {
        client.UploadFile(file_name, bucket_name, object_name,
                          IfGenerationMatch(0));
      } catch (std::runtime_error const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("[412]"));
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
  std::remove(file_name.c_str());
}

TEST_F(ObjectMediaIntegrationTest, XmlUploadFile) {
  Client client;
  auto file_name = MakeRandomObjectName();
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
  std::remove(file_name.c_str());
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
  ObjectMetadata meta =
      client.InsertObject(bucket_name, object_name, LoremIpsum(),
                          IfGenerationMatch(0), Projection::Full());
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
        EXPECT_THAT(ex.what(), HasSubstr("mismatched hashes"));
        throw;
      },
      HashMismatchError);
#else
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_NE(stream.received_hash(), stream.computed_hash());
  EXPECT_EQ(stream.received_hash(), meta.md5_hash());
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
  ObjectMetadata meta =
      client.InsertObject(bucket_name, object_name, LoremIpsum(),
                          IfGenerationMatch(0), Projection::Full());
  auto stream = client.ReadObject(
      bucket_name, object_name, IfMetagenerationNotMatch(0),
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
  ObjectWriteStream stream = client.WriteObject(
      bucket_name, object_name, IfGenerationMatch(0), Fields(""),
      CustomHeader("x-goog-testbench-instructions",
                   "inject-upload-data-error"));
  stream << LoremIpsum() << "\n";
  stream << LoremIpsum();
  std::string md5_hash = ComputeMD5Hash(LoremIpsum() + "\n" + LoremIpsum());

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(try { stream.Close(); } catch (HashMismatchError const& ex) {
    EXPECT_NE(ex.received_hash(), ex.computed_hash());
    EXPECT_THAT(ex.what(), HasSubstr("ValidateHash"));
    throw;
  },
               HashMismatchError);
#else
  stream.Close();
  EXPECT_FALSE(stream.received_hash().empty());
  EXPECT_FALSE(stream.computed_hash().empty());
  EXPECT_NE(stream.received_hash(), stream.computed_hash());
  EXPECT_EQ(stream.computed_hash(), md5_hash);
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS

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
      client.WriteObject(bucket_name, object_name, IfGenerationMatch(0),
                         CustomHeader("x-goog-testbench-instructions",
                                      "inject-upload-data-error"));
  stream << LoremIpsum() << "\n";
  stream << LoremIpsum();
  std::string md5_hash = ComputeMD5Hash(LoremIpsum() + "\n" + LoremIpsum());

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(try { stream.Close(); } catch (HashMismatchError const& ex) {
    EXPECT_NE(ex.received_hash(), ex.computed_hash());
    EXPECT_THAT(ex.what(), HasSubstr("ValidateHash"));
    throw;
  },
               HashMismatchError);
#else
  stream.Close();
  EXPECT_FALSE(stream.received_hash().empty());
  EXPECT_FALSE(stream.computed_hash().empty());
  EXPECT_NE(stream.received_hash(), stream.computed_hash());
  EXPECT_EQ(stream.computed_hash(), md5_hash);
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS

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
