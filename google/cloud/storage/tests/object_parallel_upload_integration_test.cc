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
#include "google/cloud/storage/parallel_upload.h"
#include "google/cloud/storage/testing/object_integration_test.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/storage/testing/temp_file.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/expect_exception.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <sys/types.h>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ObjectParallelUploadIntegrationTest =
    ::google::cloud::storage::testing::ObjectIntegrationTest;

TEST_F(ObjectParallelUploadIntegrationTest, ParallelUpload) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto prefix = CreateRandomPrefixName();
  std::string const dest_object_name = prefix + ".dest";

  testing::TempFile temp_file(LoremIpsum());

  auto object_metadata = ParallelUploadFile(
      *client, temp_file.name(), bucket_name_, dest_object_name, prefix, false,
      MinStreamSize(0), IfGenerationMatch(0));

  auto stream =
      client->ReadObject(bucket_name_, dest_object_name,
                         IfGenerationMatch(object_metadata->generation()));
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(LoremIpsum(), actual);

  std::vector<ObjectMetadata> objects;
  for (auto& object : client->ListObjects(bucket_name_)) {
    ASSERT_STATUS_OK(object);
    if (object->name().substr(0, prefix.size()) == prefix &&
        object->name() != prefix) {
      objects.emplace_back(*std::move(object));
    }
  }
  ASSERT_EQ(1U, objects.size());
  ASSERT_EQ(prefix + ".dest", objects[0].name());

  auto deletion_status =
      client->DeleteObject(bucket_name_, dest_object_name,
                           IfGenerationMatch(object_metadata->generation()));
  ASSERT_STATUS_OK(deletion_status);
}

}  // anonymous namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
