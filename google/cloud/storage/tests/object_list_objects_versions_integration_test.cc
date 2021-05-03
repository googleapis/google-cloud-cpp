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

  std::string bucket_name = MakeRandomBucketName();
  auto create = client->CreateBucketForProject(
      bucket_name, project_id_,
      BucketMetadata{}.set_versioning(BucketVersioning{true}));
  ASSERT_STATUS_OK(create) << bucket_name;

  std::vector<std::pair<std::string, std::int64_t>> expected;
  for (auto i : {1, 2, 3, 4}) {
    SCOPED_TRACE("Creating objects #" + std::to_string(i));
    auto object_name = MakeRandomObjectName();
    // ASSERT_TRUE does not work inside this lambda because the return type is
    // not `void`, use `.value()` instead to throw (or crash) on the unexpected
    // error.
    auto precondition = storage::IfGenerationMatch(0);
    for (std::string rev : {"first", "second", "third"}) {
      auto meta = client
                      ->InsertObject(bucket_name, object_name,
                                     "contents for the " + rev + " revision",
                                     precondition)
                      .value();
      expected.emplace_back(meta.name(), meta.generation());
      precondition = storage::IfGenerationMatch{};
    }
  }

  ListObjectsReader reader = client->ListObjects(bucket_name, Versions(true));
  std::vector<std::pair<std::string, std::int64_t>> actual;
  for (auto& it : reader) {
    auto const& meta = it.value();
    actual.emplace_back(meta.name(), meta.generation());
  }
  EXPECT_THAT(actual, ::testing::IsSupersetOf(expected));
  for (auto const& o : client->ListObjects(bucket_name, Versions(true))) {
    if (!o) break;
    (void)client->DeleteObject(bucket_name, o->name(),
                               Generation(o->generation()));
  }

  auto status = client->DeleteBucket(bucket_name);
  ASSERT_STATUS_OK(status);
}

}  // anonymous namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
