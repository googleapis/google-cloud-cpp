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

#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::internal::GetEnv;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::testing::IsEmpty;
using ::testing::Not;

// When GOOGLE_CLOUD_CPP_HAVE_GRPC is not set these tests compile, but they
// actually just run against the regular GCS REST API. That is fine.
class GrpcHmacKeyMetadataIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {};

TEST_F(GrpcHmacKeyMetadataIntegrationTest, HmacKeyCRUD) {
  // TODO(#7257) - restore gRPC integration tests against production, even then
  //     generally we do not create HMAC keys in production because they are
  //     an extremely limited resource.
  if (!UsingEmulator()) GTEST_SKIP();

  auto const project_name = GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  ASSERT_THAT(project_name, Not(IsEmpty()))
      << "GOOGLE_CLOUD_PROJECT is not set";
  auto const service_account =
      google::cloud::internal::GetEnv(
          "GOOGLE_CLOUD_CPP_STORAGE_TEST_HMAC_SERVICE_ACCOUNT")
          .value_or("");
  ASSERT_THAT(service_account, Not(IsEmpty()));

  // TODO(#7757) - use gRPC to also create the keys.
  auto rest_client = Client();
  auto create = rest_client.CreateHmacKey(service_account);
  ASSERT_STATUS_OK(create);
  auto const key = create->second;
  auto const metadata = create->first;
  EXPECT_THAT(key, Not(IsEmpty()));

  ScopedEnvironment grpc_config("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG",
                                "metadata");
  auto client = MakeIntegrationTestClient();
  auto get = client->GetHmacKey(metadata.access_id());
  ASSERT_STATUS_OK(get);
  // Compare member-by-member as the ETag field is missing in the protos:
  EXPECT_EQ(get->id(), metadata.id());
  EXPECT_EQ(get->access_id(), metadata.access_id());
  EXPECT_EQ(get->project_id(), metadata.project_id());
  EXPECT_EQ(get->service_account_email(), metadata.service_account_email());
  EXPECT_EQ(get->state(), metadata.state());
  EXPECT_EQ(get->time_created(), metadata.time_created());
  EXPECT_EQ(get->updated(), metadata.updated());
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
