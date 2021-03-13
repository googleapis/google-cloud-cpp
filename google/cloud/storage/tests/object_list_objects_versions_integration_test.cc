// Copyright 2020 Google LLC
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
#include "google/cloud/storage/testing/object_integration_test.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <sys/types.h>
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ObjectListObjectsVersionsIntegrationTest =
    ::google::cloud::storage::testing::ObjectIntegrationTest;

TEST_F(ObjectListObjectsVersionsIntegrationTest, ListObjectsVersions) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  // This test requires the bucket to be configured with versioning. The buckets
  // used by the CI build are already configured with versioning enabled. The
  // bucket created in the emulator also has versioning. Regardless, set the
  // bucket to the desired state, which will produce a better error message if
  // there is a configuration problem.
  auto bucket_meta = client->GetBucketMetadata(bucket_name_);
  ASSERT_STATUS_OK(bucket_meta);
  auto updated_meta = client->PatchBucket(
      bucket_name_,
      BucketMetadataPatchBuilder().SetVersioning(BucketVersioning{true}),
      IfMetagenerationMatch(bucket_meta->metageneration()));
  ASSERT_STATUS_OK(updated_meta);
  ASSERT_TRUE(updated_meta->versioning().has_value());
  ASSERT_TRUE(updated_meta->versioning().value().enabled);

  auto create_object_with_3_versions = [&client, this] {
    auto object_name = MakeRandomObjectName();
    // ASSERT_TRUE does not work inside this lambda because the return type is
    // not `void`, use `.value()` instead to throw (or crash) on the unexpected
    // error.
    auto meta = client
                    ->InsertObject(bucket_name_, object_name,
                                   "contents for the first revision",
                                   storage::IfGenerationMatch(0))
                    .value();
    client
        ->InsertObject(bucket_name_, object_name,
                       "contents for the second revision")
        .value();
    client
        ->InsertObject(bucket_name_, object_name,
                       "contents for the final revision")
        .value();
    return meta.name();
  };

  std::vector<std::string> expected(4);
  std::generate_n(expected.begin(), expected.size(),
                  create_object_with_3_versions);

  ListObjectsReader reader = client->ListObjects(bucket_name_, Versions(true));
  std::vector<std::string> actual;
  for (auto& it : reader) {
    auto const& meta = it.value();
    EXPECT_EQ(bucket_name_, meta.bucket());
    actual.push_back(meta.name());
  }
  auto produce_joined_list = [&actual] {
    std::string joined;
    for (auto const& x : actual) {
      joined += "  ";
      joined += x;
      joined += "\n";
    }
    return joined;
  };
  // There may be a lot of other objects in the bucket, so we want to verify
  // that any objects we created are found there, but cannot expect a perfect
  // match.

  for (auto const& name : expected) {
    EXPECT_EQ(3, std::count(actual.begin(), actual.end(), name))
        << "Expected to find 3 copies of " << name << " in the object list:\n"
        << produce_joined_list();
  }
}

}  // anonymous namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
