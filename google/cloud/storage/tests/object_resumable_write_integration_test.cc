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
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/testing_util/init_google_mock.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {
using ::testing::HasSubstr;
using google::cloud::storage::testing::TestPermanentFailure;

/// Store the project and instance captured from the command-line arguments.
class ObjectResumableWriteTestEnvironment : public ::testing::Environment {
 public:
  ObjectResumableWriteTestEnvironment(std::string bucket_name) {
    bucket_name_ = std::move(bucket_name);
  }

  static std::string const& bucket_name() { return bucket_name_; }

 private:
  static std::string bucket_name_;
};

std::string ObjectResumableWriteTestEnvironment::bucket_name_;

class ObjectResumableWriteIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {};

bool UsingTestbench() {
  return google::cloud::internal::GetEnv("CLOUD_STORAGE_TESTBENCH_ENDPOINT")
      .has_value();
}

TEST_F(ObjectResumableWriteIntegrationTest, WriteWithContentType) {
  Client client;
  auto bucket_name = ObjectResumableWriteTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;

  // Create the object, but only if it does not exist already.
  auto os = client.WriteObject(
      bucket_name, object_name, IfGenerationMatch(0),
      WithObjectMetadata(ObjectMetadata().set_content_type("text/plain")));
  os << LoremIpsum();
  ObjectMetadata meta = os.Close();
  EXPECT_EQ(object_name, meta.name());
  EXPECT_EQ(bucket_name, meta.bucket());
  EXPECT_EQ("text/plain", meta.content_type());
  if (UsingTestbench()) {
    EXPECT_TRUE(meta.has_metadata("x_testbench_upload"));
    EXPECT_EQ("resumable", meta.metadata("x_testbench_upload"));
  }

  client.DeleteObject(bucket_name, object_name);
}

TEST_F(ObjectResumableWriteIntegrationTest, WriteWithContentTypeFailure) {
  Client client;
  auto bucket_name = MakeRandomBucketName();
  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;

  // Create the object, but only if it does not exist already.
  TestPermanentFailure([&] {
    auto os = client.WriteObject(
        bucket_name, object_name, IfGenerationMatch(0),
        WithObjectMetadata(ObjectMetadata().set_content_type("text/plain")));
    os << LoremIpsum();
    ObjectMetadata meta = os.Close();
  });
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
    std::cerr << "Usage: " << cmd.substr(last_slash + 1) << " <bucket-name>"
              << std::endl;
    return 1;
  }

  std::string const bucket_name = argv[1];
  (void)::testing::AddGlobalTestEnvironment(
      new google::cloud::storage::ObjectResumableWriteTestEnvironment(
          bucket_name));

  return RUN_ALL_TESTS();
}
