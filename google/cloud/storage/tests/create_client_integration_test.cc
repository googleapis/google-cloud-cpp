// Copyright 2021 Google LLC
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
#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/storage/internal/unified_rest_credentials.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/storage/testing/temp_file.h"
#include "google/cloud/credentials.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <fstream>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::google::cloud::internal::GetEnv;
using ::google::cloud::testing_util::IsOk;
using ::testing::IsEmpty;

class CreateClientIntegrationTest
    : public ::google::cloud::storage::testing::StorageIntegrationTest {
 protected:
  void SetUp() override {
    bucket_name_ =
        GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME").value_or("");

    ASSERT_THAT(bucket_name_, Not(IsEmpty()));
  }

  void UseClient(Client client, std::string const& bucket_name,
                 std::string const& object_name, std::string const& payload) {
    StatusOr<ObjectMetadata> meta = client.InsertObject(
        bucket_name, object_name, payload, IfGenerationMatch(0));
    ASSERT_THAT(meta, IsOk());
    ScheduleForDelete(*meta);
    EXPECT_EQ(object_name, meta->name());

    auto stream = client.ReadObject(bucket_name, object_name);
    std::string actual(std::istreambuf_iterator<char>{stream}, {});
    EXPECT_EQ(payload, actual);
  }

  std::string const& bucket_name() const { return bucket_name_; }

 private:
  std::string bucket_name_;
};

#include "google/cloud/internal/disable_deprecation_warnings.inc"

TEST_F(CreateClientIntegrationTest, DefaultWorks) {
  auto client = Client::CreateDefaultClient();
  ASSERT_THAT(client, IsOk());
  ASSERT_NO_FATAL_FAILURE(
      UseClient(*client, bucket_name(), MakeRandomObjectName(), LoremIpsum()));
}

TEST_F(CreateClientIntegrationTest, SettingPolicies) {
  auto credentials = oauth2::GoogleDefaultCredentials();
  ASSERT_THAT(credentials, IsOk());
  auto client =
      Client(ClientOptions(*credentials),
             LimitedErrorCountRetryPolicy(/*maximum_failures=*/5),
             ExponentialBackoffPolicy(/*initial_delay=*/std::chrono::seconds(1),
                                      /*maximum_delay=*/std::chrono::minutes(5),
                                      /*scaling=*/1.5));
  ASSERT_NO_FATAL_FAILURE(
      UseClient(client, bucket_name(), MakeRandomObjectName(), LoremIpsum()));
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
