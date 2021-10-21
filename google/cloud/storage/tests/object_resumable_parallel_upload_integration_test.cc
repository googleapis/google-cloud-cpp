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

using ObjectResumableParallelUploadIntegrationTest =
    ::google::cloud::storage::testing::ObjectIntegrationTest;

TEST_F(ObjectResumableParallelUploadIntegrationTest, ResumableParallelUpload) {
  // TODO(b/146890058) - reenable the test for gRPC
  if (UsingGrpc()) GTEST_SKIP();
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto prefix = CreateRandomPrefixName();
  std::string const dest_object_name = prefix + ".dest";

  std::string resumable_session_id;
  {
    auto state =
        PrepareParallelUpload(*client, bucket_name_, dest_object_name, 2,
                              prefix, UseResumableUploadSession(""));
    ASSERT_STATUS_OK(state);
    resumable_session_id = state->resumable_session_id();
    state->shards()[0] << "1";
    std::move(state->shards()[0]).Suspend();
    state->shards()[1] << "34";
    state->shards().clear();
  }
  std::int64_t res_gen;
  {
    auto state = PrepareParallelUpload(
        *client, bucket_name_, dest_object_name, 2, prefix,
        UseResumableUploadSession(resumable_session_id));
    ASSERT_STATUS_OK(state);
    ASSERT_EQ(0, state->shards()[0].next_expected_byte());
    state->shards()[0] << "12";
    state->shards().clear();
    auto res = state->WaitForCompletion().get();
    ASSERT_STATUS_OK(res);
    ScheduleForDelete(*res);
    res_gen = res->generation();
  }
  auto stream = client->ReadObject(bucket_name_, dest_object_name,
                                   IfGenerationMatch(res_gen));
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ("1234", actual);
}

TEST_F(ObjectResumableParallelUploadIntegrationTest, ResumeParallelUploadFile) {
  // TODO(b/146890058) - reenable the test for gRPC
  if (UsingGrpc()) GTEST_SKIP();
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto prefix = CreateRandomPrefixName();
  std::string const dest_object_name = prefix + ".dest";

  testing::TempFile temp_file(LoremIpsum());

  auto shards = CreateUploadShards(*client, temp_file.name(), bucket_name_,
                                   dest_object_name, prefix, MinStreamSize(0),
                                   MaxStreams(3), IfGenerationMatch(0),
                                   UseResumableUploadSession(""));

  ASSERT_STATUS_OK(shards);
  ASSERT_GT(shards->size(), 1);

  std::string resumable_session_id = (*shards)[0].resumable_session_id();
  auto first_part_res_future = (*shards)[0].WaitForCompletion();
  ASSERT_STATUS_OK((*shards)[0].Upload());
  shards->clear();  // we'll resume those
  auto first_part_res = first_part_res_future.get();
  EXPECT_FALSE(first_part_res);
  EXPECT_EQ(StatusCode::kCancelled, first_part_res.status().code());

  auto object_metadata = ParallelUploadFile(
      *client, temp_file.name(), bucket_name_, dest_object_name, prefix, false,
      MinStreamSize(0), IfGenerationMatch(0),
      UseResumableUploadSession(resumable_session_id));
  ASSERT_STATUS_OK(object_metadata);
  ScheduleForDelete(*object_metadata);

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
}

}  // anonymous namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
