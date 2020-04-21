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
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/expect_exception.h"
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

class ObjectComposeManyIntegrationTest
    : public google::cloud::storage::testing::ObjectIntegrationTest {
 protected:
  void SetUp() override {
    google::cloud::storage::testing::ObjectIntegrationTest::SetUp();
    client_ = MakeIntegrationTestClient();
    ASSERT_STATUS_OK(client_);
  }

  void TearDown() override {
    for (auto const& source_object : source_objects_) {
      EXPECT_STATUS_OK(
          client_->DeleteObject(bucket_name_, source_object.object_name));
    }
    google::cloud::storage::testing::ObjectIntegrationTest::TearDown();
  }

  StatusOr<Client> client_;
  std::vector<ComposeSourceObject> source_objects_;
};

TEST_F(ObjectComposeManyIntegrationTest, ComposeMany) {
  auto prefix = CreateRandomPrefixName();
  std::string const dest_object_name = prefix + ".dest";

  std::string expected;
  for (int i = 0; i != 33; ++i) {
    std::string const object_name = prefix + ".src-" + std::to_string(i);
    std::string content = std::to_string(i);
    expected += content;
    StatusOr<ObjectMetadata> insert_meta = client_->InsertObject(
        bucket_name_, object_name, std::move(content), IfGenerationMatch(0));
    ASSERT_STATUS_OK(insert_meta);
    source_objects_.emplace_back(ComposeSourceObject{
        std::move(object_name), insert_meta->generation(), {}});
  }

  auto res = ComposeMany(*client_, bucket_name_, source_objects_, prefix,
                         dest_object_name, false);

  ASSERT_STATUS_OK(res);
  EXPECT_EQ(dest_object_name, res->name());

  auto stream = client_->ReadObject(bucket_name_, dest_object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);

  auto deletion_status = client_->DeleteObject(
      bucket_name_, dest_object_name, IfGenerationMatch(res->generation()));
  EXPECT_STATUS_OK(deletion_status);
}

}  // anonymous namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
