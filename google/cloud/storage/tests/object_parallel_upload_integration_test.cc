// Copyright 2020 Google LLC
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
#include "google/cloud/storage/parallel_upload.h"
#include "google/cloud/storage/testing/object_integration_test.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/storage/testing/temp_file.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/expect_exception.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <algorithm>
#include <iostream>
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::AnyOf;
using ::testing::ElementsAre;

using ObjectParallelUploadIntegrationTest =
    ::google::cloud::storage::testing::ObjectIntegrationTest;

TEST_F(ObjectParallelUploadIntegrationTest, ParallelUpload) {
  auto client = MakeIntegrationTestClient();
  auto prefix = CreateRandomPrefixName();
  std::string const dest_object_name = prefix + ".dest";
  testing::TempFile temp_file(LoremIpsum());

  auto object_metadata = ParallelUploadFile(
      client, temp_file.name(), bucket_name_, dest_object_name, prefix, false,
      MinStreamSize(0), IfGenerationMatch(0),
      WithObjectMetadata(
          ObjectMetadata().set_content_type("application/binary")));
  ASSERT_STATUS_OK(object_metadata);
  ScheduleForDelete(*object_metadata);
  EXPECT_EQ("application/binary", object_metadata->content_type());

  auto stream =
      client.ReadObject(bucket_name_, dest_object_name,
                        IfGenerationMatch(object_metadata->generation()));
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(LoremIpsum(), actual);

  std::vector<std::string> names;
  for (auto& object : client.ListObjects(bucket_name_, Prefix(prefix))) {
    if (!object) break;
    names.push_back(object->name());
  }
  EXPECT_THAT(names, ElementsAre(dest_object_name));
}

TEST_F(ObjectParallelUploadIntegrationTest, DefaultAllowOverwrites) {
  auto client = MakeIntegrationTestClient();

  // Create the local file, we overwrite its contents manually because creating
  // a large block is fairly tedious.
  auto const block = MakeRandomData(1024 * 1024);
  testing::TempFile temp_file(block);

  auto prefix = CreateRandomPrefixName();
  auto const dest_object_name = prefix + ".dest";
  // First insert the object, verify that it did not exist before the upload.
  auto insert = client.InsertObject(bucket_name_, dest_object_name,
                                    LoremIpsum(), IfGenerationMatch(0));
  ASSERT_STATUS_OK(insert);
  ScheduleForDelete(*insert);

  auto object_metadata = ParallelUploadFile(
      client, temp_file.name(), bucket_name_, dest_object_name, prefix, false,
      MinStreamSize(0), MaxStreams(64));
  ASSERT_STATUS_OK(object_metadata);
  ScheduleForDelete(*object_metadata);

  EXPECT_EQ(block.size(), object_metadata->size());

  std::vector<std::string> names;
  for (auto& object : client.ListObjects(bucket_name_, Prefix(prefix))) {
    if (!object) break;
    names.push_back(object->name());
  }
  EXPECT_THAT(names, ElementsAre(dest_object_name));
}

TEST_F(ObjectParallelUploadIntegrationTest, PreconditionsPreventOverwrites) {
  auto client = MakeIntegrationTestClient();

  // Create the local file, we overwrite its contents manually because creating
  // a large block is fairly tedious.
  auto const block = MakeRandomData(1024 * 1024);
  testing::TempFile temp_file(block);

  auto prefix = CreateRandomPrefixName();
  auto const dest_object_name = prefix + ".dest";
  // First insert the object, verify that it did not exist before the upload.
  auto insert = client.InsertObject(bucket_name_, dest_object_name,
                                    LoremIpsum(), IfGenerationMatch(0));
  ASSERT_STATUS_OK(insert);
  ScheduleForDelete(*insert);

  auto object_metadata = ParallelUploadFile(
      client, temp_file.name(), bucket_name_, dest_object_name, prefix, false,
      MinStreamSize(0), MaxStreams(64), IfGenerationMatch(0));
  EXPECT_THAT(object_metadata, StatusIs(AnyOf(StatusCode::kFailedPrecondition,
                                              StatusCode::kAborted)));

  std::vector<std::string> names;
  for (auto& object : client.ListObjects(bucket_name_, Prefix(prefix))) {
    if (!object) break;
    names.push_back(object->name());
  }
  EXPECT_THAT(names, ElementsAre(dest_object_name));
}

}  // anonymous namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
