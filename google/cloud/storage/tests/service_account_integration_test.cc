// Copyright 2018 Google LLC
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
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsOk;
using ::testing::Contains;
using ::testing::Not;

class ServiceAccountIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {
 protected:
  void SetUp() override {
    project_id_ =
        google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
    ASSERT_FALSE(project_id_.empty());
    service_account_ = google::cloud::internal::GetEnv(
                           "GOOGLE_CLOUD_CPP_STORAGE_TEST_HMAC_SERVICE_ACCOUNT")
                           .value_or("");
    ASSERT_FALSE(service_account_.empty());
  }

  std::string project_id_;
  std::string service_account_;
};

TEST_F(ServiceAccountIntegrationTest, Get) {
  auto client = MakeIntegrationTestClient();

  StatusOr<ServiceAccount> a1 = client.GetServiceAccountForProject(project_id_);
  ASSERT_STATUS_OK(a1);
  EXPECT_FALSE(a1->email_address().empty());

  Client client_with_default(Options{}.set<ProjectIdOption>(project_id_));
  StatusOr<ServiceAccount> a2 = client_with_default.GetServiceAccount();
  ASSERT_STATUS_OK(a2);
  EXPECT_FALSE(a2->email_address().empty());

  EXPECT_EQ(*a1, *a2);
}

TEST_F(ServiceAccountIntegrationTest, CreateHmacKeyForProject) {
  // HMAC keys are a scarce resource. Testing in production would require
  // redesigning the tests to use a random service account (or creating one)
  // dynamically.  For now, simply skip these tests.
  if (!UsingEmulator()) GTEST_SKIP();
  Client client(Options{}.set<ProjectIdOption>(project_id_));

  StatusOr<std::pair<HmacKeyMetadata, std::string>> key = client.CreateHmacKey(
      service_account_, OverrideDefaultProject(project_id_));
  ASSERT_STATUS_OK(key);

  EXPECT_FALSE(key->second.empty());

  StatusOr<HmacKeyMetadata> update_details = client.UpdateHmacKey(
      key->first.access_id(), HmacKeyMetadata().set_state("INACTIVE"));
  ASSERT_STATUS_OK(update_details);
  EXPECT_EQ("INACTIVE", update_details->state());

  Status deleted_key = client.DeleteHmacKey(key->first.access_id());
  ASSERT_STATUS_OK(deleted_key);
}

TEST_F(ServiceAccountIntegrationTest, HmacKeyCRUD) {
  // HMAC keys are a scarce resource. Testing in production would require
  // redesigning the tests to use a random service account (or creating one)
  // dynamically.  For now, simply skip these tests.
  if (!UsingEmulator()) GTEST_SKIP();
  Client client(Options{}.set<ProjectIdOption>(project_id_));

  auto get_current_access_ids = [&client, this]() {
    std::vector<std::string> access_ids;
    auto range = client.ListHmacKeys(OverrideDefaultProject(project_id_),
                                     ServiceAccountFilter(service_account_));
    std::transform(
        range.begin(), range.end(), std::back_inserter(access_ids),
        [](StatusOr<HmacKeyMetadata> x) { return x.value().access_id(); });
    return access_ids;
  };

  auto initial_access_ids = get_current_access_ids();

  StatusOr<std::pair<HmacKeyMetadata, std::string>> key =
      client.CreateHmacKey(service_account_);
  ASSERT_STATUS_OK(key);

  EXPECT_FALSE(key->second.empty());
  auto access_id = key->first.access_id();

  EXPECT_THAT(initial_access_ids, Not(Contains(access_id)));

  auto post_create_access_ids = get_current_access_ids();
  EXPECT_THAT(post_create_access_ids, Contains(access_id));

  StatusOr<HmacKeyMetadata> get_details = client.GetHmacKey(access_id);
  ASSERT_STATUS_OK(get_details);

  EXPECT_EQ(access_id, get_details->access_id());
  EXPECT_EQ(key->first, *get_details);

  StatusOr<HmacKeyMetadata> update_details =
      client.UpdateHmacKey(access_id, HmacKeyMetadata().set_state("INACTIVE"));
  ASSERT_STATUS_OK(update_details);
  EXPECT_EQ("INACTIVE", update_details->state());

  Status deleted_key = client.DeleteHmacKey(key->first.access_id());
  ASSERT_STATUS_OK(deleted_key);

  auto post_delete_access_ids = get_current_access_ids();
  EXPECT_THAT(post_delete_access_ids, Not(Contains(access_id)));
}

TEST_F(ServiceAccountIntegrationTest, HmacKeyCRUDFailures) {
  Client client(Options{}.set<ProjectIdOption>(project_id_));

  // Test failures in the HmacKey operations by using an invalid project id:
  auto create_status = client.CreateHmacKey("invalid-service-account",
                                            OverrideDefaultProject(""));
  EXPECT_FALSE(create_status) << "value=" << create_status->first;

  Status deleted_status =
      client.DeleteHmacKey("invalid-access-id", OverrideDefaultProject(""));
  EXPECT_THAT(deleted_status, Not(IsOk()));

  StatusOr<HmacKeyMetadata> get_status =
      client.GetHmacKey("invalid-access-id", OverrideDefaultProject(""));
  EXPECT_THAT(get_status, Not(IsOk())) << "value=" << get_status.value();

  StatusOr<HmacKeyMetadata> update_status = client.UpdateHmacKey(
      "invalid-access-id", HmacKeyMetadata(), OverrideDefaultProject(""));
  EXPECT_THAT(update_status, Not(IsOk())) << "value=" << update_status.value();

  auto range = client.ListHmacKeys(OverrideDefaultProject(""));
  auto begin = range.begin();
  EXPECT_NE(begin, range.end());
  EXPECT_THAT(*begin, Not(IsOk())) << "value=" << begin->value();
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
