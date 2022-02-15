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
#include "google/cloud/bigtable/admin/bigtable_table_admin_options.h"
#include "google/cloud/bigtable/admin/mocks/mock_bigtable_table_admin_connection.h"
#include "google/cloud/bigtable/testing/mock_policies.h"
#include "google/cloud/internal/time_utils.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// Helper class for checking that the legacy API still functions correctly
class TableAdminTester {
 public:
  static bigtable::TableAdmin MakeTestTableAdmin(
      std::shared_ptr<bigtable_admin::BigtableTableAdminConnection> conn,
      CompletionQueue cq, std::string const& kProjectId,
      std::string const& kInstanceId) {
    return bigtable::TableAdmin(std::move(conn), std::move(cq), kProjectId,
                                kInstanceId);
  }

  static std::shared_ptr<bigtable_admin::BigtableTableAdminConnection>
  Connection(bigtable::TableAdmin const& admin) {
    return admin.connection_;
  }

  static CompletionQueue CQ(bigtable::TableAdmin const& admin) {
    return admin.cq_;
  }

  static std::shared_ptr<BackgroundThreads> Threads(
      bigtable::TableAdmin const& admin) {
    return admin.background_threads_;
  }

  static Options Policies(bigtable::TableAdmin const& admin) {
    return admin.policies_;
  }
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

namespace {

using ::google::cloud::bigtable::testing::MockBackoffPolicy;
using ::google::cloud::bigtable::testing::MockPollingPolicy;
using ::google::cloud::bigtable::testing::MockRetryPolicy;
using ::google::cloud::bigtable_internal::TableAdminTester;
using ::google::cloud::internal::ToChronoTimePoint;
using ::testing::An;
using ::testing::ElementsAre;
using ::testing::IsNull;
using ::testing::NotNull;
using MockConnection =
    ::google::cloud::bigtable_admin_mocks::MockBigtableTableAdminConnection;

auto const* const kProjectId = "the-project";
auto const* const kInstanceId = "the-instance";
auto const* const kTableId = "the-table";
auto const* const kClusterId = "the-cluster";
auto const* const kBackupId = "the-backup";
auto const* const kInstanceName = "projects/the-project/instances/the-instance";
auto const* const kTableName =
    "projects/the-project/instances/the-instance/tables/the-table";
auto const* const kClusterName =
    "projects/the-project/instances/the-instance/clusters/the-cluster";
auto const* const kBackupName =
    "projects/the-project/instances/the-instance/clusters/the-cluster/backups/"
    "the-backup";

Options TestOptions() {
  return Options{}.set<GrpcCredentialOption>(
      grpc::InsecureChannelCredentials());
}

bool SameCQ(CompletionQueue const& a, CompletionQueue const& b) {
  using ::google::cloud::internal::GetCompletionQueueImpl;
  return GetCompletionQueueImpl(a) == GetCompletionQueueImpl(b);
}

void CheckPolicies(Options const& options) {
  EXPECT_TRUE(
      options.has<bigtable_admin::BigtableTableAdminRetryPolicyOption>());
  EXPECT_TRUE(
      options.has<bigtable_admin::BigtableTableAdminBackoffPolicyOption>());
  EXPECT_TRUE(
      options.has<bigtable_admin::BigtableTableAdminPollingPolicyOption>());
  EXPECT_TRUE(options.has<google::cloud::internal::GrpcSetupOption>());
  EXPECT_TRUE(options.has<google::cloud::internal::GrpcSetupPollOption>());
}

class TableAdminTest : public ::testing::Test {
 protected:
  TableAdmin DefaultTableAdmin() {
    return TableAdminTester::MakeTestTableAdmin(connection_, {}, kProjectId,
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

TEST_F(TableAdminTest, LegacyConstructorSetsCQ) {
  auto admin_client = MakeAdminClient(kProjectId, TestOptions());
  auto admin = TableAdmin(admin_client, kInstanceId);
  auto conn = TableAdminTester::Connection(admin);
  ASSERT_TRUE(conn->options().has<GrpcCompletionQueueOption>());
  auto conn_cq = conn->options().get<GrpcCompletionQueueOption>();
  auto client_cq = TableAdminTester::CQ(admin);

  EXPECT_TRUE(SameCQ(conn_cq, client_cq));
  EXPECT_THAT(TableAdminTester::Threads(admin), NotNull());
}

TEST_F(TableAdminTest, LegacyConstructorSetsCustomCQ) {
  CompletionQueue user_cq;
  auto admin_client = MakeAdminClient(
      kProjectId, TestOptions().set<GrpcCompletionQueueOption>(user_cq));
  auto admin = TableAdmin(admin_client, kInstanceId);
  auto conn = TableAdminTester::Connection(admin);
  ASSERT_TRUE(conn->options().has<GrpcCompletionQueueOption>());
  auto conn_cq = conn->options().get<GrpcCompletionQueueOption>();
  auto client_cq = TableAdminTester::CQ(admin);

  EXPECT_TRUE(SameCQ(user_cq, client_cq));
  EXPECT_TRUE(SameCQ(conn_cq, client_cq));
  EXPECT_THAT(TableAdminTester::Threads(admin), IsNull());
}

TEST_F(TableAdminTest, LegacyConstructorDefaultsPolicies) {
  auto admin_client = MakeAdminClient(kProjectId, TestOptions());
  auto admin = TableAdmin(std::move(admin_client), kInstanceId);
  auto policies = TableAdminTester::Policies(admin);
  CheckPolicies(policies);
}

TEST_F(TableAdminTest, LegacyConstructorWithPolicies) {
  // In this test, we make a series of simple calls to verify that the policies
  // passed to the `TableAdmin` constructor are actually collected as
  // `Options`.
  //
  // Upon construction of an TableAdmin, each policy is cloned twice: Once
  // while processing the variadic parameters, once while converting from
  // Bigtable policies to common policies. This should explain the nested mocks
  // below.

  auto mock_r = std::make_shared<MockRetryPolicy>();
  auto mock_b = std::make_shared<MockBackoffPolicy>();
  auto mock_p = std::make_shared<MockPollingPolicy>();

  EXPECT_CALL(*mock_r, clone).WillOnce([] {
    auto clone_1 = absl::make_unique<MockRetryPolicy>();
    EXPECT_CALL(*clone_1, clone).WillOnce([] {
      auto clone_2 = absl::make_unique<MockRetryPolicy>();
      EXPECT_CALL(*clone_2, OnFailure(An<Status const&>()));
      return clone_2;
    });
    return clone_1;
  });

  EXPECT_CALL(*mock_b, clone).WillOnce([] {
    auto clone_1 = absl::make_unique<MockBackoffPolicy>();
    EXPECT_CALL(*clone_1, clone).WillOnce([] {
      auto clone_2 = absl::make_unique<MockBackoffPolicy>();
      EXPECT_CALL(*clone_2, OnCompletion(An<Status const&>()));
      return clone_2;
    });
    return clone_1;
  });

  EXPECT_CALL(*mock_p, clone).WillOnce([] {
    auto clone_1 = absl::make_unique<MockPollingPolicy>();
    EXPECT_CALL(*clone_1, clone).WillOnce([] {
      auto clone_2 = absl::make_unique<MockPollingPolicy>();
      EXPECT_CALL(*clone_2, WaitPeriod);
      return clone_2;
    });
    return clone_1;
  });

  auto admin_client = MakeAdminClient(kProjectId, TestOptions());
  auto admin = TableAdmin(std::move(admin_client), kInstanceId, *mock_r,
                          *mock_b, *mock_p);
  auto policies = TableAdminTester::Policies(admin);
  CheckPolicies(policies);

  auto const& common_retry =
      policies.get<bigtable_admin::BigtableTableAdminRetryPolicyOption>();
  (void)common_retry->OnFailure({});

  auto const& common_backoff =
      policies.get<bigtable_admin::BigtableTableAdminBackoffPolicyOption>();
  (void)common_backoff->OnCompletion();

  auto const& common_polling =
      policies.get<bigtable_admin::BigtableTableAdminPollingPolicyOption>();
  (void)common_polling->WaitPeriod();
}

TEST_F(TableAdminTest, CreateBackupParams) {
  auto const expire_time =
      std::chrono::system_clock::now() + std::chrono::hours(24);
  TableAdmin::CreateBackupParams params(kClusterId, kBackupId, kTableId,
                                        expire_time);

  auto request = params.AsProto(kInstanceName);
  EXPECT_EQ(kClusterName, request.parent());
  EXPECT_EQ(kBackupId, request.backup_id());
  EXPECT_EQ(kTableName, request.backup().source_table());
  EXPECT_EQ(expire_time, ToChronoTimePoint(request.backup().expire_time()));
}

TEST_F(TableAdminTest, UpdateBackupParams) {
  auto const expire_time =
      std::chrono::system_clock::now() + std::chrono::hours(24);
  TableAdmin::UpdateBackupParams params(kClusterId, kBackupId, expire_time);

  auto request = params.AsProto(kInstanceName);
  EXPECT_EQ(kBackupName, request.backup().name());
  EXPECT_EQ(expire_time, ToChronoTimePoint(request.backup().expire_time()));
  EXPECT_THAT(request.update_mask().paths(), ElementsAre("expire_time"));
}

TEST_F(TableAdminTest, ListBackupsParams) {
  TableAdmin::ListBackupsParams params;
  auto request = params.AsProto(kInstanceName);
  EXPECT_EQ(request.parent(), ClusterName(kProjectId, kInstanceId, "-"));

  params.set_cluster(kClusterId).set_filter("state:READY").set_order_by("name");
  request = params.AsProto(kInstanceName);
  EXPECT_EQ(request.parent(), kClusterName);
  EXPECT_EQ(*request.mutable_filter(), "state:READY");
  EXPECT_EQ(*request.mutable_order_by(), "name");
}

TEST_F(TableAdminTest, RestoreTableParams) {
  TableAdmin::RestoreTableParams params(kTableId, kClusterId, kBackupId);
  auto request = params.AsProto(kInstanceName);
  EXPECT_EQ(kInstanceName, request.parent());
  EXPECT_EQ(kTableId, request.table_id());
  EXPECT_EQ(kBackupName, request.backup());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
