// Copyright 2018 Google Inc.
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

#include "google/cloud/bigtable/instance_admin.h"
#include "google/cloud/bigtable/testing/mock_instance_admin_client.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::ReturnRef;

using MockAdminClient =
    ::google::cloud::bigtable::testing::MockInstanceAdminClient;

std::string const kProjectId = "the-project";

/// A fixture for the bigtable::InstanceAdmin tests.
class InstanceAdminTest : public ::testing::Test {
 protected:
  void SetUp() override {
    EXPECT_CALL(*client_, project()).WillRepeatedly(ReturnRef(kProjectId));
  }

  std::shared_ptr<MockAdminClient> client_ =
      std::make_shared<MockAdminClient>();
};

/// @test Verify basic functionality in the `bigtable::InstanceAdmin` class.
TEST_F(InstanceAdminTest, Default) {
  InstanceAdmin tested(client_);
  EXPECT_EQ("the-project", tested.project_id());
}

TEST_F(InstanceAdminTest, CopyConstructor) {
  InstanceAdmin source(client_);
  std::string const& expected = source.project_id();
  // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
  InstanceAdmin copy(source);
  EXPECT_EQ(expected, copy.project_id());
}

TEST_F(InstanceAdminTest, MoveConstructor) {
  InstanceAdmin source(client_);
  std::string expected = source.project_id();
  InstanceAdmin copy(std::move(source));
  EXPECT_EQ(expected, copy.project_id());
}

TEST_F(InstanceAdminTest, CopyAssignment) {
  std::shared_ptr<MockAdminClient> other_client =
      std::make_shared<MockAdminClient>();
  std::string other_project = "other-project";
  EXPECT_CALL(*other_client, project())
      .WillRepeatedly(ReturnRef(other_project));

  InstanceAdmin source(client_);
  std::string const& expected = source.project_id();
  InstanceAdmin dest(other_client);
  EXPECT_NE(expected, dest.project_id());
  dest = source;
  EXPECT_EQ(expected, dest.project_id());
}

TEST_F(InstanceAdminTest, MoveAssignment) {
  std::shared_ptr<MockAdminClient> other_client =
      std::make_shared<MockAdminClient>();
  std::string other_project = "other-project";
  EXPECT_CALL(*other_client, project())
      .WillRepeatedly(ReturnRef(other_project));

  InstanceAdmin source(client_);
  std::string expected = source.project_id();
  InstanceAdmin dest(other_client);
  EXPECT_NE(expected, dest.project_id());
  dest = std::move(source);
  EXPECT_EQ(expected, dest.project_id());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
