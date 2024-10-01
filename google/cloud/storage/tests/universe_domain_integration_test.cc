// Copyright 2024 Google LLC
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
#include "google/cloud/storage/options.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/internal/filesystem.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/universe_domain.h"
#include "google/cloud/universe_domain_options.h"
#include "google/cloud/version.h"
#include <gmock/gmock.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::internal::GetEnv;

class UniverseDomainIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {
 protected:
  void SetUp() override {
    bucket_name_ = MakeRandomBucketName();
    object_name_ = MakeRandomObjectName();
  }

  std::string const& bucket_name() const { return bucket_name_; }
  std::string const& object_name() const { return object_name_; }

 private:
  std::string bucket_name_;
  std::string object_name_;
};

auto TestOptions() {
  auto sa_key_file = GetEnv("UD_SA_KEY_FILE").value_or("");
  auto projectId = GetEnv("UD_PROJECT").value_or("");
  Options options;

  auto is = std::ifstream(sa_key_file);
  is.exceptions(std::ios::badbit);
  auto contents = std::string(std::istreambuf_iterator<char>(is.rdbuf()), {});
  options.set<UnifiedCredentialsOption>(
      MakeServiceAccountCredentials(contents));

  auto ud_options = AddUniverseDomainOption(
      ExperimentalTag{}, options.set<ProjectIdOption>(projectId));
  if (!ud_options.ok()) throw std::move(ud_options).status();

  return *ud_options;
}

TEST_F(UniverseDomainIntegrationTest, BucketAndObjectCRUD) {
  auto region = GetEnv("UD_REGION").value_or("");
  if (GetEnv("UD_SA_KEY_FILE").value_or("").empty() ||
      GetEnv("UD_PROJECT").value_or("").empty() || region.empty())
    GTEST_SKIP();

  std::cout << "Did not skip" << std::endl;

  auto client = Client(TestOptions());
  auto bucket =
      client.CreateBucket(bucket_name(), BucketMetadata{}.set_location(region));
  ASSERT_STATUS_OK(bucket);
  ScheduleForDelete(*bucket);

  std::cout << "After bucket" << std::endl;

  auto insert = client.InsertObject(bucket_name(), object_name(), LoremIpsum());
  ASSERT_STATUS_OK(insert);
  ScheduleForDelete(*insert);

  auto reader = client.ReadObject(bucket_name(), object_name());
  ASSERT_TRUE(reader.good());
  ASSERT_STATUS_OK(reader.status());

  std::cout << "After Read" << std::endl;

  auto const actual =
      std::string{std::istreambuf_iterator<char>{reader.rdbuf()}, {}};
  std::cout << actual << std::endl;
  EXPECT_EQ(LoremIpsum(), actual);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
