// Copyright 2022 Google LLC
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
#include "google/cloud/bigtable/admin/mocks/mock_bigtable_instance_admin_connection.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// Helper class for checking that the legacy API still functions correctly
class InstanceAdminTester {
 public:
  static std::shared_ptr<bigtable_admin::BigtableInstanceAdminConnection>
  Connection(InstanceAdmin const& admin) {
    return admin.connection_;
  }
};

namespace {

using MockConnection =
    ::google::cloud::bigtable_admin_mocks::MockBigtableInstanceAdminConnection;

std::string const kProjectId = "the-project";

/// A fixture for the bigtable::InstanceAdmin tests.
class InstanceAdminTest : public ::testing::Test {
 protected:
  std::shared_ptr<MockConnection> connection_ =
      std::make_shared<MockConnection>();
};

TEST_F(InstanceAdminTest, Project) {
  Project p(kProjectId);
  InstanceAdmin tested(MakeInstanceAdminClient(p.project_id()));
  EXPECT_EQ(p.project_id(), tested.project_id());
  EXPECT_EQ(p.FullName(), tested.project_name());
}

TEST_F(InstanceAdminTest, CopyConstructor) {
  auto source = InstanceAdmin(connection_, kProjectId);
  std::string const& expected = source.project_id();
  // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
  InstanceAdmin copy(source);
  EXPECT_EQ(expected, copy.project_id());
}

TEST_F(InstanceAdminTest, MoveConstructor) {
  auto source = InstanceAdmin(connection_, kProjectId);
  std::string expected = source.project_id();
  InstanceAdmin copy(std::move(source));
  EXPECT_EQ(expected, copy.project_id());
}

TEST_F(InstanceAdminTest, CopyAssignment) {
  std::shared_ptr<MockConnection> other_client =
      std::make_shared<MockConnection>();

  auto source = InstanceAdmin(connection_, kProjectId);
  std::string const& expected = source.project_id();
  auto dest = InstanceAdmin(other_client, "other-project");
  EXPECT_NE(expected, dest.project_id());
  dest = source;
  EXPECT_EQ(expected, dest.project_id());
}

TEST_F(InstanceAdminTest, MoveAssignment) {
  std::shared_ptr<MockConnection> other_client =
      std::make_shared<MockConnection>();

  auto source = InstanceAdmin(connection_, kProjectId);
  std::string expected = source.project_id();
  auto dest = InstanceAdmin(other_client, "other-project");
  EXPECT_NE(expected, dest.project_id());
  dest = std::move(source);
  EXPECT_EQ(expected, dest.project_id());
}

TEST_F(InstanceAdminTest, LegacyConstructorSharesConnection) {
  auto admin_client = MakeInstanceAdminClient("test-project");
  auto admin_1 = InstanceAdmin(admin_client);
  auto admin_2 = InstanceAdmin(admin_client);
  auto conn_1 = InstanceAdminTester::Connection(admin_1);
  auto conn_2 = InstanceAdminTester::Connection(admin_2);

  EXPECT_EQ(conn_1, conn_2);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
