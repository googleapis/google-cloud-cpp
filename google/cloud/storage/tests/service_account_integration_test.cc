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
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/init_google_mock.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::testing::HasSubstr;

/// Store the project and instance captured from the command-line arguments.
class ServiceAccountTestEnvironment : public ::testing::Environment {
 public:
  ServiceAccountTestEnvironment(std::string project,
                                std::string service_account) {
    project_id_ = std::move(project);
    service_account_ = std::move(service_account);
  }

  static std::string const& project_id() { return project_id_; }
  static std::string const& service_account() { return service_account_; }

 private:
  static std::string project_id_;
  static std::string service_account_;
};

std::string ServiceAccountTestEnvironment::project_id_;
std::string ServiceAccountTestEnvironment::service_account_;

bool UsingTestbench() {
  return google::cloud::internal::GetEnv("CLOUD_STORAGE_TESTBENCH_ENDPOINT")
      .has_value();
}

TEST(ServiceAccountIntegrationTest, Get) {
  auto project_id = ServiceAccountTestEnvironment::project_id();
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);

  StatusOr<ServiceAccount> a1 = client->GetServiceAccountForProject(project_id);
  ASSERT_STATUS_OK(a1);
  EXPECT_FALSE(a1->email_address().empty());

  auto client_options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(client_options);
  Client client_with_default(client_options->set_project_id(project_id));
  StatusOr<ServiceAccount> a2 = client_with_default.GetServiceAccount();
  ASSERT_STATUS_OK(a2);
  EXPECT_FALSE(a2->email_address().empty());

  EXPECT_EQ(*a1, *a2);
}

TEST(ServiceAccountIntegrationTest, CreateHmacKeyForProject) {
  if (!UsingTestbench()) {
    // Temporarily disabled outside the testbench because the test does not
    // cleanup after itself.
    return;
  }

  auto project_id = ServiceAccountTestEnvironment::project_id();
  auto service_account = ServiceAccountTestEnvironment::service_account();
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);

  StatusOr<std::pair<HmacKeyMetadata, std::string>> key = client->CreateHmacKey(
      service_account, OverrideDefaultProject(project_id));
  ASSERT_STATUS_OK(key);

  EXPECT_FALSE(key->second.empty());
}

TEST(ServiceAccountIntegrationTest, CreateHmacKey) {
  if (!UsingTestbench()) {
    // Temporarily disabled outside the testbench because the test does not
    // cleanup after itself.
    return;
  }

  auto project_id = ServiceAccountTestEnvironment::project_id();
  auto client_options = ClientOptions::CreateDefaultClientOptions();
  auto service_account = ServiceAccountTestEnvironment::service_account();
  ASSERT_STATUS_OK(client_options);

  Client client(client_options->set_project_id(project_id));
  StatusOr<std::pair<HmacKeyMetadata, std::string>> key =
      client.CreateHmacKey(service_account);
  ASSERT_STATUS_OK(key);

  EXPECT_FALSE(key->second.empty());
}

TEST(ServiceAccountIntegrationTest, ListHmacKeys) {
  if (!UsingTestbench()) {
    // Temporarily disabled outside the testbench because the test does not
    // cleanup after itself.
    return;
  }

  auto project_id = ServiceAccountTestEnvironment::project_id();
  auto client_options = ClientOptions::CreateDefaultClientOptions();
  auto service_account = ServiceAccountTestEnvironment::service_account();
  ASSERT_STATUS_OK(client_options);

  Client client(client_options->set_project_id(project_id));
  StatusOr<std::pair<HmacKeyMetadata, std::string>> key =
      client.CreateHmacKey(service_account);
  ASSERT_STATUS_OK(key);
  EXPECT_FALSE(key->second.empty());

  bool found = false;
  for (auto&& k : client.ListHmacKeys(OverrideDefaultProject(project_id))) {
    ASSERT_STATUS_OK(key);
    if (k->access_id() == key->first.access_id()) {
      found = true;
    }
  }
  EXPECT_TRUE(found);
}

}  // namespace
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
              << " <project> <service-account>\n";
    return 1;
  }

  std::string const project_id = argv[1];
  std::string const service_account = argv[2];
  (void)::testing::AddGlobalTestEnvironment(
      new google::cloud::storage::ServiceAccountTestEnvironment(
          project_id, service_account));

  return RUN_ALL_TESTS();
}
