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

using ObjectPlentyClientsSimultaneouslyIntegrationTest =
    ::google::cloud::storage::testing::ObjectIntegrationTest;

TEST_F(ObjectPlentyClientsSimultaneouslyIntegrationTest,
       PlentyClientsSimultaneously) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name_, object_name, expected, IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  // Create a iostream to read the object back.
  auto num_fds_before_test = GetNumOpenFiles();
  std::vector<Client> read_clients;
  std::vector<ObjectReadStream> read_streams;
  for (int i = 0; i != 100; ++i) {
    auto read_client = MakeIntegrationTestClient();
    ASSERT_STATUS_OK(read_client);
    auto stream = read_client->ReadObject(bucket_name_, object_name);
    char c;
    stream.read(&c, 1);
    read_streams.emplace_back(std::move(stream));
    read_clients.emplace_back(*std::move(read_client));
  }
  auto num_fds_during_test = GetNumOpenFiles();
  read_streams.clear();
  read_clients.clear();
  auto num_fds_after_test = GetNumOpenFiles();

  // `GetNumOpenFiles()` is not implemented on all platforms.
  // If they all return ok then check the values,
  // otherwise they must all be `kUnimplemented`
  if (num_fds_before_test.ok() && num_fds_during_test.ok() &&
      num_fds_after_test.ok()) {
    EXPECT_LT(*num_fds_before_test, *num_fds_during_test)
        << "Clients keeps at least some file descriptors open";
    EXPECT_LT(*num_fds_after_test, *num_fds_during_test)
        << "Releasing clients also releases at least some file descriptors";
    EXPECT_GE(*num_fds_before_test, *num_fds_after_test)
        << "Clients are leaking descriptors";
  } else {
    EXPECT_EQ(StatusCode::kUnimplemented, num_fds_before_test.status().code());
    EXPECT_EQ(StatusCode::kUnimplemented, num_fds_during_test.status().code());
    EXPECT_EQ(StatusCode::kUnimplemented, num_fds_after_test.status().code());
  }
}

}  // anonymous namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
