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
  ServiceAccountTestEnvironment(std::string project) {
    project_id_ = std::move(project);
  }

  static std::string const& project_id() { return project_id_; }

 private:
  static std::string project_id_;
};

std::string ServiceAccountTestEnvironment::project_id_;

TEST(ServiceAccountIntegrationTest, Get) {
  auto project_id = ServiceAccountTestEnvironment::project_id();
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_TRUE(client.ok()) << "status=" << client.status();

  StatusOr<ServiceAccount> a1 = client->GetServiceAccountForProject(project_id);
  EXPECT_FALSE(a1->email_address().empty());

  auto client_options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_TRUE(client_options.ok()) << "status=" << client_options.status();
  Client client_with_default(client_options->set_project_id(project_id));
  StatusOr<ServiceAccount> a2 = client_with_default.GetServiceAccount();
  EXPECT_FALSE(a2->email_address().empty());

  EXPECT_EQ(*a1, *a2);
}

}  // namespace
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
    std::cerr << "Usage: " << cmd.substr(last_slash + 1) << " <project>"
              << std::endl;
    return 1;
  }

  std::string const project_id = argv[1];
  (void)::testing::AddGlobalTestEnvironment(
      new google::cloud::storage::ServiceAccountTestEnvironment(project_id));

  return RUN_ALL_TESTS();
}
