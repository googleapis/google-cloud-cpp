// Copyright 2024 Google LLC
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

#include "google/cloud/storagecontrol/v2/testing/remove_stale_folders.h"
#include "google/cloud/storagecontrol/v2/mocks/mock_storage_control_connection.h"
#include "google/cloud/storagecontrol/v2/storage_control_client.h"

namespace google {
namespace cloud {
namespace storagecontrol_v2 {
namespace testing {

TEST(RemoveStaleFoldersTest, RemoveStaleFolders) {
  auto mock_connection = std::make_shared<
      google::cloud::storagecontrol_v2_mocks::MockStorageControlConnection>();
  EXPECT_CALL(*mock_connection, ListFolders)
      .WillOnce([](google::storage::control::v2::ListFoldersRequest const& r) {
        EXPECT_EQ(r.parent(), "fake-parent");
        StreamRange<google::storage::control::v2::Folder> response;

        return response;
      });
  auto client =
      google::cloud::storagecontrol_v2::StorageControlClient(mock_connection);
}
}  // namespace testing
}  // namespace storagecontrol_v2
}  // namespace cloud
}  // namespace google
