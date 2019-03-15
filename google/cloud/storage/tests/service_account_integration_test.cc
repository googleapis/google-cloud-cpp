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

// Initialized in main() below.
char const* flag_project_id;
char const* flag_service_account;

bool UsingTestbench() {
  return google::cloud::internal::GetEnv("CLOUD_STORAGE_TESTBENCH_ENDPOINT")
      .has_value();
}

TEST(ServiceAccountIntegrationTest, Get) {
  std::string project_id = flag_project_id;
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

  std::string project_id = flag_project_id;
  std::string service_account = flag_service_account;
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);

  StatusOr<std::pair<HmacKeyMetadata, std::string>> key = client->CreateHmacKey(
      service_account, OverrideDefaultProject(project_id));
  ASSERT_STATUS_OK(key);

  EXPECT_FALSE(key->second.empty());
}

TEST(ServiceAccountIntegrationTest, HmacKeyCRUD) {
  if (!UsingTestbench()) {
    // Temporarily disabled outside the testbench because the test does not
    // cleanup after itself.
    return;
  }

  std::string project_id = flag_project_id;
  auto client_options = ClientOptions::CreateDefaultClientOptions();
  std::string service_account = flag_service_account;
  ASSERT_STATUS_OK(client_options);

  Client client(client_options->set_project_id(project_id));

  auto get_current_access_ids = [&client, &project_id, &service_account]() {
    std::vector<std::string> access_ids;
    auto range = client.ListHmacKeys(OverrideDefaultProject(project_id),
                                     ServiceAccountFilter(service_account));
    std::transform(
        range.begin(), range.end(), std::back_inserter(access_ids),
        [](StatusOr<HmacKeyMetadata> x) { return x.value().access_id(); });
    return access_ids;
  };

  auto initial_access_ids = get_current_access_ids();

  StatusOr<std::pair<HmacKeyMetadata, std::string>> key =
      client.CreateHmacKey(service_account);
  ASSERT_STATUS_OK(key);

  EXPECT_FALSE(key->second.empty());
  auto access_id = key->first.access_id();

  using ::testing::Contains;
  using ::testing::Not;
  EXPECT_THAT(initial_access_ids, Not(Contains(access_id)));

  auto post_create_access_ids = get_current_access_ids();
  EXPECT_THAT(post_create_access_ids, Contains(access_id));

  StatusOr<HmacKeyMetadata> get_details = client.GetHmacKey(access_id);
  ASSERT_STATUS_OK(get_details);

  EXPECT_EQ(access_id, get_details->access_id());
  EXPECT_EQ(key->first, *get_details);

  StatusOr<HmacKeyMetadata> update_details = client.UpdateHmacKey(
      access_id,
      HmacKeyMetadata().set_state("INACTIVE").set_etag(get_details->etag()));
  ASSERT_STATUS_OK(update_details);
  EXPECT_EQ("INACTIVE", update_details->state());

  StatusOr<HmacKeyMetadata> deleted_key =
      client.DeleteHmacKey(key->first.access_id());
  ASSERT_STATUS_OK(deleted_key);
  EXPECT_EQ(HmacKeyMetadata::state_deleted(), deleted_key->state());

  auto post_delete_access_ids = get_current_access_ids();
  EXPECT_THAT(post_delete_access_ids, Not(Contains(access_id)));

  // Delete all HmacKeys for the test service account, it is just good practice
  // to cleanup after ourselves.
  for (auto const& id : post_delete_access_ids) {
    StatusOr<HmacKeyMetadata> d = client.DeleteHmacKey(id);
    ASSERT_STATUS_OK(d);
  }
}

TEST(ServiceAccountIntegrationTest, HmacKeyCRUDFailures) {
  if (!UsingTestbench()) {
    // Temporarily disabled outside the testbench because the test does not
    // cleanup after itself.
    return;
  }

  std::string project_id = flag_project_id;
  auto client_options = ClientOptions::CreateDefaultClientOptions();
  std::string service_account = flag_service_account;
  ASSERT_STATUS_OK(client_options);

  Client client(client_options->set_project_id(project_id));

  // Test failures in the HmacKey operations by using an invalid project id:
  StatusOr<HmacKeyMetadata> create_status =
      client.DeleteHmacKey("invalid-access-id", OverrideDefaultProject(""));
  EXPECT_FALSE(create_status) << "value=" << *create_status;

  StatusOr<HmacKeyMetadata> deleted_status =
      client.DeleteHmacKey("invalid-access-id", OverrideDefaultProject(""));
  EXPECT_FALSE(deleted_status) << "value=" << *deleted_status;

  StatusOr<HmacKeyMetadata> get_status =
      client.GetHmacKey("invalid-access-id", OverrideDefaultProject(""));
  EXPECT_FALSE(get_status) << "value=" << *get_status;

  StatusOr<HmacKeyMetadata> update_status = client.UpdateHmacKey(
      "invalid-access-id", HmacKeyMetadata(), OverrideDefaultProject(""));
  EXPECT_FALSE(update_status) << "value=" << *update_status;

  auto range = client.ListHmacKeys(OverrideDefaultProject(""));
  auto begin = range.begin();
  EXPECT_NE(begin, range.end());
  EXPECT_FALSE(*begin) << "value=" << **begin;
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

  google::cloud::storage::flag_project_id = argv[1];
  google::cloud::storage::flag_service_account = argv[2];

  return RUN_ALL_TESTS();
}
