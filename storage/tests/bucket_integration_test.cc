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

#include "storage/client/bucket.h"
#include <gtest/gtest.h>

namespace {
/// Store the project and instance captured from the command-line arguments.
class BucketTestEnvironment : public ::testing::Environment {
 public:
  BucketTestEnvironment(std::string project, std::string instance) {
    project_id_ = std::move(project);
    bucket_name_ = std::move(instance);
  }

  static std::string const& project_id() { return project_id_; }
  static std::string const& bucket_name() { return bucket_name_; }

 private:
  static std::string project_id_;
  static std::string bucket_name_;
};

std::string BucketTestEnvironment::project_id_;
std::string BucketTestEnvironment::bucket_name_;

class BucketIntegrationTest : public ::testing::Test {};
}  // anonymous namespace

TEST_F(BucketIntegrationTest, GetMetadata) {
  auto bucket_name = BucketTestEnvironment::bucket_name();
  auto client =
      storage::CreateDefaultClient(storage::GoogleDefaultCredentials());
  storage::Bucket bucket(client, bucket_name);

  auto metadata = bucket.GetMetadata();
  EXPECT_EQ(bucket_name, metadata.name());
  EXPECT_EQ(bucket_name, metadata.id());
  EXPECT_EQ("storage#bucket", metadata.kind());
  std::cout << metadata << std::endl;
}

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);

  // Make sure the arguments are valid.
  if (argc != 3) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(argv[0]).find_last_of("/");
    std::cerr << "Usage: " << cmd.substr(last_slash + 1)
              << " <project> <bucket>" << std::endl;
    return 1;
  }

  std::string const project_id = argv[1];
  std::string const bucket_name = argv[2];
  (void)::testing::AddGlobalTestEnvironment(
      new BucketTestEnvironment(project_id, bucket_name));

  return RUN_ALL_TESTS();
}
