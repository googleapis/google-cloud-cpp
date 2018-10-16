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
#include "google/cloud/testing_util/init_google_mock.h"
#include <gmock/gmock.h>
#include <regex>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
using ::testing::HasSubstr;
namespace {
/// Store the project and instance captured from the command-line arguments.
class ObjectDownloadTestEnvironment : public ::testing::Environment {
 public:
  ObjectDownloadTestEnvironment(std::string project, std::string instance) {
    project_id_ = std::move(project);
    bucket_name_ = std::move(instance);
  }

  static std::string const& project_id() { return project_id_; }
  static std::string const& bucket_name() { return bucket_name_; }

 private:
  static std::string project_id_;
  static std::string bucket_name_;
};

std::string ObjectDownloadTestEnvironment::project_id_;
std::string ObjectDownloadTestEnvironment::bucket_name_;

class ObjectDownloadIntegrationTest : public ::testing::Test {
 protected:
  std::string MakeRandomObjectName() {
    return "ob-read-" +
           google::cloud::internal::Sample(generator_, 32,
                                           "abcdefghijklmnopqrstuvwxyz"
                                           "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                           "012456789") +
           ".txt";
  }

  std::string LoremIpsum() const {
    return R"""(Lorem ipsum dolor sit amet, consectetur adipiscing
elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea
commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit
esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat
non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.
)""";
  }

 protected:
  google::cloud::internal::DefaultPRNG generator_ =
      google::cloud::internal::MakeDefaultPRNG();
};

bool UsingTestbench() {
  return std::getenv("CLOUD_STORAGE_TESTBENCH_ENDPOINT") != nullptr;
}

/// @test Verify that MD5 hash mismatches are reported by default on downloads.
TEST_F(ObjectDownloadIntegrationTest, MismatchedMD5StreamingReadXML) {
  if (not UsingTestbench()) {
    // This test is disabled when not using the testbench as it relies on the
    // testbench to inject faults.
    return;
  }
  Client client;
  auto bucket_name = ObjectDownloadTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // Create an object and a stream to read it back.
  ObjectMetadata meta =
      client.InsertObject(bucket_name, object_name, LoremIpsum(),
                          IfGenerationMatch(0), Projection::Full());
  auto stream = client.ReadObject(bucket_name, object_name,
                                  QuotaUser("return-mismatched-data"));

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
TEST_F(ObjectDownloadIntegrationTest, MismatchedMD5StreamingReadJSON) {
  if (not UsingTestbench()) {
    // This test is disabled when not using the testbench as it relies on the
    // testbench to inject faults.
    return;
  }
  Client client;
  auto bucket_name = ObjectDownloadTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // Create an object and a stream to read it back.
  ObjectMetadata meta =
      client.InsertObject(bucket_name, object_name, LoremIpsum(),
                          IfGenerationMatch(0), Projection::Full());
  auto stream =
      client.ReadObject(bucket_name, object_name, IfMetagenerationNotMatch(0),
                        QuotaUser("return-mismatched-data"));

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
      new google::cloud::storage::ObjectDownloadTestEnvironment(project_id,
                                                                bucket_name));

  return RUN_ALL_TESTS();
}
