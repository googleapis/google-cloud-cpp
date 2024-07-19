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
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::internal::GetEnv;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Pair;

// When GOOGLE_CLOUD_CPP_HAVE_GRPC is not set these tests compile, but they
// actually just run against the regular GCS REST API. That is fine.
class GrpcObjectMetadataIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {};

TEST_F(GrpcObjectMetadataIntegrationTest, ObjectMetadataCRUD) {
  ScopedEnvironment grpc_config("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG",
                                "metadata");
  auto const bucket_name =
      GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME").value_or("");
  ASSERT_THAT(bucket_name, Not(IsEmpty()))
      << "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME is not set";

  auto client = MakeIntegrationTestClient();
  auto object_name = MakeRandomObjectName();
  auto rewrite_name = MakeRandomObjectName();
  auto copy_name = MakeRandomObjectName();
  auto compose_name = MakeRandomObjectName();

  // Use the full projection to get consistent behavior out of gRPC and REST.
  auto insert = client.InsertObject(bucket_name, object_name, LoremIpsum(),
                                    IfGenerationMatch(0), Projection::Full());
  ASSERT_STATUS_OK(insert);
  ScheduleForDelete(*insert);

  auto get =
      client.GetObjectMetadata(bucket_name, object_name, Projection::Full());
  ASSERT_STATUS_OK(get);
  EXPECT_EQ(*insert, *get);

  std::vector<std::string> names;
  for (auto const& object : client.ListObjects(bucket_name)) {
    ASSERT_STATUS_OK(object);
    names.push_back(object->name());
  }
  EXPECT_THAT(names, Contains(object_name));

  auto rewrite = client.RewriteObjectBlocking(bucket_name, object_name,
                                              bucket_name, rewrite_name);
  ASSERT_STATUS_OK(rewrite);
  ScheduleForDelete(*rewrite);

  auto copy =
      client.CopyObject(bucket_name, object_name, bucket_name, rewrite_name);
  ASSERT_STATUS_OK(copy);
  ScheduleForDelete(*copy);

  auto patch = client.PatchObject(
      bucket_name, object_name,
      ObjectMetadataPatchBuilder{}.SetCacheControl("no-cache"));
  ASSERT_STATUS_OK(patch);
  EXPECT_EQ(patch->cache_control(), "no-cache");

  auto compose = client.ComposeObject(
      bucket_name,
      {
          ComposeSourceObject{object_name, absl::nullopt, absl::nullopt},
          ComposeSourceObject{object_name, absl::nullopt, absl::nullopt},
      },
      compose_name);
  ASSERT_STATUS_OK(compose);
  ScheduleForDelete(*compose);

  auto custom = std::chrono::system_clock::now() + std::chrono::hours(24);
  auto desired_metadata = ObjectMetadata(*patch).set_custom_time(custom);
  auto updated =
      client.UpdateObject(bucket_name, object_name, desired_metadata);
  ASSERT_STATUS_OK(updated);
  ASSERT_TRUE(updated->has_custom_time());
  ASSERT_EQ(updated->custom_time(), custom);

  auto del = client.DeleteObject(bucket_name, object_name);
  ASSERT_STATUS_OK(del);

  get = client.GetObjectMetadata(bucket_name, object_name);
  EXPECT_THAT(get, StatusIs(StatusCode::kNotFound));
}

TEST_F(GrpcObjectMetadataIntegrationTest, PatchMetadata) {
  ScopedEnvironment grpc_config("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG",
                                "metadata");
  auto const bucket_name =
      GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME").value_or("");
  ASSERT_THAT(bucket_name, Not(IsEmpty()))
      << "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME is not set";

  auto client = MakeIntegrationTestClient();
  auto object_name = MakeRandomObjectName();

  // Use the full projection to get consistent behavior out of gRPC and REST.
  auto insert = client.InsertObject(bucket_name, object_name, LoremIpsum(),
                                    IfGenerationMatch(0), Projection::Full());
  ASSERT_STATUS_OK(insert);
  ScheduleForDelete(*insert);

  auto patch = client.PatchObject(bucket_name, object_name,
                                  ObjectMetadataPatchBuilder{}
                                      .SetMetadata("test-key0", "v0")
                                      .SetMetadata("test-key1", "v1")
                                      .SetMetadata("test-key2", "v2"));
  ASSERT_STATUS_OK(patch);
  EXPECT_THAT(patch->metadata(), AllOf(Contains(Pair("test-key0", "v0")),
                                       Contains(Pair("test-key1", "v1")),
                                       Contains(Pair("test-key2", "v2"))));

  patch = client.PatchObject(bucket_name, object_name,
                             ObjectMetadataPatchBuilder{}
                                 .SetMetadata("test-key0", "new-v0")
                                 .ResetMetadata("test-key1")
                                 .SetMetadata("test-key3", "v3"));
  ASSERT_STATUS_OK(patch);
  EXPECT_THAT(patch->metadata(), AllOf(Contains(Pair("test-key0", "new-v0")),
                                       Not(Contains(Pair("test-key1", _))),
                                       Contains(Pair("test-key2", "v2")),
                                       Contains(Pair("test-key3", "v3"))));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
