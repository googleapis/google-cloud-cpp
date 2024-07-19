// Copyright 2022 Google LLC
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

#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::internal::GetEnv;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::testing::Contains;
using ::testing::IsEmpty;
using ::testing::Not;

// When GOOGLE_CLOUD_CPP_HAVE_GRPC is not set these tests compile, but they
// actually just run against the regular GCS REST API. That is fine.
class GrpcHmacKeyMetadataIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {};

TEST_F(GrpcHmacKeyMetadataIntegrationTest, HmacKeyCRUD) {
  // We do not run the REST or gRPC integration tests in production because
  // quota is extremely restricted for this type of resource.
  if (!UsingEmulator()) GTEST_SKIP();

  auto const project_name = GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  ASSERT_THAT(project_name, Not(IsEmpty()))
      << "GOOGLE_CLOUD_PROJECT is not set";
  auto const service_account =
      google::cloud::internal::GetEnv(
          "GOOGLE_CLOUD_CPP_STORAGE_TEST_HMAC_SERVICE_ACCOUNT")
          .value_or("");
  ASSERT_THAT(service_account, Not(IsEmpty()));

  ScopedEnvironment grpc_config("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG",
                                "metadata");
  auto client = MakeIntegrationTestClient();

  auto get_ids = [&] {
    std::vector<std::string> ids;
    auto range = client.ListHmacKeys(ServiceAccountFilter(service_account));
    std::transform(range.begin(), range.end(), std::back_inserter(ids),
                   [](StatusOr<HmacKeyMetadata> x) { return x.value().id(); });
    return ids;
  };

  auto const initial_ids = get_ids();

  auto create = client.CreateHmacKey(service_account);
  ASSERT_STATUS_OK(create);
  auto const key = create->second;
  auto const metadata = create->first;
  EXPECT_THAT(key, Not(IsEmpty()));

  EXPECT_THAT(initial_ids, Not(Contains(metadata.id())));
  auto current_ids = get_ids();
  EXPECT_THAT(current_ids, Contains(metadata.id()));

  auto get = client.GetHmacKey(metadata.access_id());
  ASSERT_STATUS_OK(get);
  EXPECT_EQ(*get, metadata);

  // Before we can delete the HmacKey we need to move it to the inactive state.
  auto update = metadata;
  update.set_state(HmacKeyMetadata::state_inactive());
  auto update_response = client.UpdateHmacKey(update.access_id(), update);
  ASSERT_STATUS_OK(update_response);
  EXPECT_EQ(update_response->state(), HmacKeyMetadata::state_inactive());

  auto delete_response = client.DeleteHmacKey(get->access_id());
  ASSERT_STATUS_OK(delete_response);

  current_ids = get_ids();
  EXPECT_THAT(current_ids, Not(Contains(metadata.id())));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
