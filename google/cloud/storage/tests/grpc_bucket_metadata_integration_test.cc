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
#include <crc32c/crc32c.h>
#include <gmock/gmock.h>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::internal::GetEnv;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Contains;
using ::testing::IsEmpty;
using ::testing::Not;

// When GOOGLE_CLOUD_CPP_HAVE_GRPC is not set these tests compile, but they
// actually just run against the regular GCS REST API. That is fine.
class GrpcBucketMetadataIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {};

TEST_F(GrpcBucketMetadataIntegrationTest, ObjectMetadataCRUD) {
  ScopedEnvironment grpc_config("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG",
                                "metadata");
  // TODO(#7257) - restore gRPC integration tests against production
  if (!UsingEmulator()) GTEST_SKIP();

  auto const bucket_name =
      GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME").value_or("");
  ASSERT_THAT(bucket_name, Not(IsEmpty()))
      << "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME is not set";

  auto client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto rest_client = google::cloud::storage::Client();
  auto rest = rest_client.GetBucketMetadata(bucket_name);
  ASSERT_STATUS_OK(rest);

  auto get = client->GetBucketMetadata(bucket_name);
  ASSERT_STATUS_OK(get);
  // There are too many fields with missing values in the testbench, just test
  // some interesting ones:
  EXPECT_EQ(get->name(), rest->name());
  EXPECT_EQ(get->metageneration(), rest->metageneration());
  EXPECT_EQ(get->time_created(), rest->time_created());
  EXPECT_EQ(get->updated(), rest->updated());
  EXPECT_EQ(get->rpo(), rest->rpo());
  EXPECT_EQ(get->location(), rest->location());
  EXPECT_EQ(get->location_type(), rest->location_type());
  EXPECT_EQ(get->storage_class(), rest->storage_class());
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
