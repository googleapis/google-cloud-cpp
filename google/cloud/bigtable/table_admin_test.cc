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

#include "google/cloud/bigtable/table_admin.h"
#include "google/cloud/bigtable/admin/bigtable_table_admin_connection.h"
#include "google/cloud/bigtable/admin/mocks/mock_bigtable_table_admin_connection.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// Helper class for checking that the legacy API still functions correctly
class TableAdminTester {
 public:
  static TableAdmin MakeTestTableAdmin(
      std::shared_ptr<bigtable_admin::BigtableTableAdminConnection> conn,
      std::string const& kProjectId, std::string const& kInstanceId) {
    return TableAdmin(std::move(conn), kProjectId, kInstanceId);
  }

  static std::shared_ptr<bigtable_admin::BigtableTableAdminConnection>
  Connection(TableAdmin const& admin) {
    return admin.connection_;
  }
};

namespace {

using ::testing::NotNull;
using MockConnection =
    ::google::cloud::bigtable_admin_mocks::MockBigtableTableAdminConnection;

auto const* const kProjectId = "the-project";
auto const* const kInstanceId = "the-instance";
auto const* const kInstanceName = "projects/the-project/instances/the-instance";

Options TestOptions() {
  return Options{}.set<GrpcCredentialOption>(
      grpc::InsecureChannelCredentials());
}

class TableAdminTest : public ::testing::Test {
 protected:
  TableAdmin DefaultTableAdmin() {
    return TableAdminTester::MakeTestTableAdmin(connection_, kProjectId,
                                                kInstanceId);
  }

  std::shared_ptr<MockConnection> connection_ =
      std::make_shared<MockConnection>();
};

TEST_F(TableAdminTest, ResourceNames) {
  auto admin = DefaultTableAdmin();
  EXPECT_EQ(kProjectId, admin.project());
  EXPECT_EQ(kInstanceId, admin.instance_id());
  EXPECT_EQ(kInstanceName, admin.instance_name());
}

TEST_F(TableAdminTest, WithNewTarget) {
  auto admin = DefaultTableAdmin();
  auto other_admin = admin.WithNewTarget("other-project", "other-instance");
  EXPECT_EQ(other_admin.project(), "other-project");
  EXPECT_EQ(other_admin.instance_id(), "other-instance");
  EXPECT_EQ(other_admin.instance_name(),
            InstanceName("other-project", "other-instance"));
}

TEST_F(TableAdminTest, LegacyConstructorSharesConnection) {
  auto admin_client = MakeAdminClient(kProjectId, TestOptions());
  auto admin_1 = TableAdmin(admin_client, kInstanceId);
  auto admin_2 = TableAdmin(admin_client, kInstanceId);
  auto conn_1 = TableAdminTester::Connection(admin_1);
  auto conn_2 = TableAdminTester::Connection(admin_2);

  EXPECT_EQ(conn_1, conn_2);
  EXPECT_THAT(conn_1, NotNull());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
