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
#include "google/cloud/log.h"
#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/expect_exception.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <sys/types.h>
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ObjectPlentyClientsSeriallyIntegrationTest =
    ::google::cloud::storage::testing::ObjectIntegrationTest;

TEST_F(ObjectPlentyClientsSeriallyIntegrationTest, PlentyClientsSerially) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name_, object_name, expected, IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);

  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name_, meta->bucket());

  // Create a iostream to read the object back.

  // Track the number of open files to ensure every client creates the same
  // number of file descriptors and none are leaked.
  //
  // However, `GetNumOpenFiles()` is not implemented on all platforms, so
  // omit the checking when it's not available.
  auto num_fds_before_test = GetNumOpenFiles();
  bool track_open_files = num_fds_before_test.ok();
  if (!track_open_files) {
    EXPECT_EQ(StatusCode::kUnimplemented, num_fds_before_test.status().code());
  }
  std::size_t delta = 0;
  for (int i = 0; i != 100; ++i) {
    auto read_client = MakeIntegrationTestClient();
    ASSERT_STATUS_OK(read_client);
    auto stream = read_client->ReadObject(bucket_name_, object_name);
    char c;
    stream.read(&c, 1);
    if (track_open_files) {
      auto num_fds_during_test = GetNumOpenFiles();
      ASSERT_STATUS_OK(num_fds_during_test);
      if (delta == 0) {
        delta = *num_fds_during_test - *num_fds_before_test;
      }
      EXPECT_GE(*num_fds_before_test + delta, *num_fds_during_test)
          << "Expect each client to create the same number of file descriptors"
          << ", num_fds_before_test=" << *num_fds_before_test
          << ", num_fds_during_test=" << *num_fds_during_test
          << ", delta=" << delta;
    }
  }
  if (track_open_files) {
    auto num_fds_after_test = GetNumOpenFiles();
    ASSERT_STATUS_OK(num_fds_after_test);
    EXPECT_EQ(*num_fds_before_test, *num_fds_after_test)
        << "Clients are leaking descriptors";
  }

  auto status = client->DeleteObject(bucket_name_, object_name);
  ASSERT_STATUS_OK(status);
}

}  // anonymous namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
