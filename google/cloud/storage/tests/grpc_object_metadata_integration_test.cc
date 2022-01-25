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
class GrpcObjectMetadataIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {};

TEST_F(GrpcObjectMetadataIntegrationTest, ObjectMetadataCRUD) {
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

  auto object_name = MakeRandomObjectName();

  auto insert = client->InsertObject(bucket_name, object_name, LoremIpsum(),
                                     IfGenerationMatch(0));
  ASSERT_STATUS_OK(insert);
  ScheduleForDelete(*insert);

  auto get = client->GetObjectMetadata(bucket_name, object_name);
  ASSERT_STATUS_OK(get);
  auto sans_acls = [](ObjectMetadata meta) {
    meta.set_acl({});
    return meta;
  };
  EXPECT_EQ(sans_acls(*insert), sans_acls(*get));

  std::vector<std::string> names;
  for (auto const& object : client->ListObjects(bucket_name)) {
    ASSERT_STATUS_OK(object);
    names.push_back(object->name());
  }
  EXPECT_THAT(names, Contains(object_name));

  auto del = client->DeleteObject(bucket_name, object_name);
  ASSERT_STATUS_OK(del);

  get = client->GetObjectMetadata(bucket_name, object_name);
  EXPECT_THAT(get, StatusIs(StatusCode::kNotFound));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
