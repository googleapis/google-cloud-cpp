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
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/internal/time_utils.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <chrono>
#include <memory>

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

  static ::google::cloud::Options Options(bigtable::TableAdmin const& admin) {
    return admin.options_;
  }
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

namespace {

namespace btadmin = ::google::bigtable::admin::v2;
namespace iamproto = ::google::iam::v1;

using ::google::cloud::bigtable::testing::MockBackoffPolicy;
using ::google::cloud::bigtable::testing::MockPollingPolicy;
using ::google::cloud::bigtable::testing::MockRetryPolicy;
using ::google::cloud::bigtable_internal::TableAdminTester;
using ::google::cloud::internal::ToChronoTimePoint;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using std::chrono::hours;
using ::testing::An;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::UnorderedElementsAreArray;
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

struct TestOption {
  using Type = int;
};

Options TestOptions() {
  return Options{}
      .set<GrpcCredentialOption>(grpc::InsecureChannelCredentials())
      .set<TestOption>(1);
}

Status FailingStatus() { return Status(StatusCode::kPermissionDenied, "fail"); }

bool SameCQ(CompletionQueue const& a, CompletionQueue const& b) {
  using ::google::cloud::internal::GetCompletionQueueImpl;
  return GetCompletionQueueImpl(a) == GetCompletionQueueImpl(b);
}

void CheckOptions(Options const& options) {
  EXPECT_TRUE(
      options.has<bigtable_admin::BigtableTableAdminRetryPolicyOption>());
  EXPECT_TRUE(
      options.has<bigtable_admin::BigtableTableAdminBackoffPolicyOption>());
  EXPECT_TRUE(
      options.has<bigtable_admin::BigtableTableAdminPollingPolicyOption>());
  EXPECT_TRUE(options.has<google::cloud::internal::GrpcSetupOption>());
  EXPECT_TRUE(options.has<google::cloud::internal::GrpcSetupPollOption>());
  EXPECT_TRUE(options.has<TestOption>());
}

class TableAdminTest : public ::testing::Test {
 protected:
  TableAdmin DefaultTableAdmin() {
    EXPECT_CALL(*connection_, options())
        .WillRepeatedly(Return(Options{}.set<TestOption>(1)));
    return TableAdminTester::MakeTestTableAdmin(connection_, {}, kProjectId,
                                                kInstanceId);
  }

  std::shared_ptr<MockConnection> connection_ =
      std::make_shared<MockConnection>();
};

TEST_F(TableAdminTest, Equality) {
  auto client1 = MakeAdminClient(kProjectId, TestOptions());
  auto client2 = MakeAdminClient(kProjectId, TestOptions());
  auto ta1 = TableAdmin(client1, "i1");
  auto ta2 = TableAdmin(client1, "i2");
  auto ta3 = TableAdmin(client2, "i1");
  EXPECT_NE(ta1, ta2);
  EXPECT_NE(ta1, ta3);
  EXPECT_NE(ta2, ta3);

  ta2 = ta1;
  EXPECT_EQ(ta1, ta2);
}

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
  auto options = TableAdminTester::Options(admin);
  CheckOptions(options);
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
  auto options = TableAdminTester::Options(admin);
  CheckOptions(options);

  auto const& common_retry =
      options.get<bigtable_admin::BigtableTableAdminRetryPolicyOption>();
  (void)common_retry->OnFailure({});

  auto const& common_backoff =
      options.get<bigtable_admin::BigtableTableAdminBackoffPolicyOption>();
  (void)common_backoff->OnCompletion();

  auto const& common_polling =
      options.get<bigtable_admin::BigtableTableAdminPollingPolicyOption>();
  (void)common_polling->WaitPeriod();
}

TEST_F(TableAdminTest, CreateTable) {
  auto admin = DefaultTableAdmin();
  std::string expected_request = R"pb(
    parent: 'projects/the-project/instances/the-instance'
    table_id: 'the-table'
    table {
      column_families {
        key: 'f1'
        value { gc_rule { max_num_versions: 1 } }
      }
      column_families {
        key: 'f2'
        value { gc_rule { max_age { seconds: 1 } } }
      }
      granularity: TIMESTAMP_GRANULARITY_UNSPECIFIED
    }
    initial_splits { key: 'a' }
    initial_splits { key: 'c' }
    initial_splits { key: 'p' }
  )pb";

  EXPECT_CALL(*connection_, CreateTable)
      .WillOnce(
          [&expected_request](btadmin::CreateTableRequest const& request) {
            CheckOptions(google::cloud::internal::CurrentOptions());
            btadmin::CreateTableRequest expected;
            EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(
                expected_request, &expected));
            EXPECT_THAT(expected, IsProtoEqual(request));
            return FailingStatus();
          });

  TableConfig config({{"f1", GcRule::MaxNumVersions(1)},
                      {"f2", GcRule::MaxAge(std::chrono::seconds(1))}},
                     {"a", "c", "p"});
  EXPECT_THAT(admin.CreateTable(kTableId, config),
              StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(TableAdminTest, ListTablesSuccess) {
  auto admin = DefaultTableAdmin();
  auto const expected_view = btadmin::Table::FULL;
  std::vector<std::string> const expected_names = {
      TableName(kProjectId, kInstanceId, "t0"),
      TableName(kProjectId, kInstanceId, "t1")};

  auto iter = expected_names.begin();
  EXPECT_CALL(*connection_, ListTables)
      .WillOnce([&iter, &expected_names,
                 expected_view](btadmin::ListTablesRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kInstanceName, request.parent());
        EXPECT_EQ(expected_view, request.view());

        using ::google::cloud::internal::MakeStreamRange;
        using ::google::cloud::internal::StreamReader;
        auto reader =
            [&iter,
             &expected_names]() -> StreamReader<btadmin::Table>::result_type {
          if (iter != expected_names.end()) {
            btadmin::Table t;
            t.set_name(*iter);
            ++iter;
            return t;
          }
          return Status();
        };
        return MakeStreamRange<btadmin::Table>(std::move(reader));
      });

  auto tables = admin.ListTables(expected_view);
  ASSERT_STATUS_OK(tables);
  std::vector<std::string> names;
  std::transform(tables->begin(), tables->end(), std::back_inserter(names),
                 [](btadmin::Table const& t) { return t.name(); });

  EXPECT_THAT(names, ElementsAreArray(expected_names));
}

TEST_F(TableAdminTest, ListTablesFailure) {
  auto admin = DefaultTableAdmin();
  auto const expected_view = btadmin::Table::NAME_ONLY;

  EXPECT_CALL(*connection_, ListTables)
      .WillOnce([&expected_view](btadmin::ListTablesRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kInstanceName, request.parent());
        EXPECT_EQ(expected_view, request.view());

        using ::google::cloud::internal::MakeStreamRange;
        return MakeStreamRange<btadmin::Table>([] { return FailingStatus(); });
      });

  EXPECT_THAT(admin.ListTables(expected_view),
              StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(TableAdminTest, GetTable) {
  auto admin = DefaultTableAdmin();
  auto const expected_view = btadmin::Table::NAME_ONLY;

  EXPECT_CALL(*connection_, GetTable)
      .WillOnce([&expected_view](btadmin::GetTableRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kTableName, request.name());
        EXPECT_EQ(expected_view, request.view());
        return FailingStatus();
      });

  EXPECT_THAT(admin.GetTable(kTableId, expected_view),
              StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(TableAdminTest, DeleteTable) {
  auto admin = DefaultTableAdmin();

  EXPECT_CALL(*connection_, DeleteTable)
      .WillOnce([](btadmin::DeleteTableRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kTableName, request.name());
        return FailingStatus();
      });

  EXPECT_THAT(admin.DeleteTable(kTableId),
              StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(TableAdminTest, CreateBackupParams) {
  auto const expire_time = std::chrono::system_clock::now() + hours(24);
  TableAdmin::CreateBackupParams params(kClusterId, kBackupId, kTableId,
                                        expire_time);

  auto request = params.AsProto(kInstanceName);
  EXPECT_EQ(kClusterName, request.parent());
  EXPECT_EQ(kBackupId, request.backup_id());
  EXPECT_EQ(kTableName, request.backup().source_table());
  EXPECT_EQ(expire_time, ToChronoTimePoint(request.backup().expire_time()));
}

TEST_F(TableAdminTest, CreateBackup) {
  auto const expire_time = std::chrono::system_clock::now() + hours(24);
  auto admin = DefaultTableAdmin();

  EXPECT_CALL(*connection_, CreateBackup)
      .WillOnce([expire_time](btadmin::CreateBackupRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kClusterName, request.parent());
        EXPECT_EQ(kBackupId, request.backup_id());
        EXPECT_EQ(kTableName, request.backup().source_table());
        EXPECT_EQ(expire_time,
                  ToChronoTimePoint(request.backup().expire_time()));
        return make_ready_future<StatusOr<btadmin::Backup>>(FailingStatus());
      });

  TableAdmin::CreateBackupParams params(kClusterId, kBackupId, kTableId,
                                        expire_time);
  EXPECT_THAT(admin.CreateBackup(params),
              StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(TableAdminTest, GetBackup) {
  auto admin = DefaultTableAdmin();

  EXPECT_CALL(*connection_, GetBackup)
      .WillOnce([](btadmin::GetBackupRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kBackupName, request.name());
        return FailingStatus();
      });

  EXPECT_THAT(admin.GetBackup(kClusterId, kBackupId),
              StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(TableAdminTest, UpdateBackupParams) {
  auto const expire_time = std::chrono::system_clock::now() + hours(24);
  TableAdmin::UpdateBackupParams params(kClusterId, kBackupId, expire_time);

  auto request = params.AsProto(kInstanceName);
  EXPECT_EQ(kBackupName, request.backup().name());
  EXPECT_EQ(expire_time, ToChronoTimePoint(request.backup().expire_time()));
  EXPECT_THAT(request.update_mask().paths(), ElementsAre("expire_time"));
}

TEST_F(TableAdminTest, UpdateBackup) {
  auto const expire_time = std::chrono::system_clock::now() + hours(24);
  auto admin = DefaultTableAdmin();

  EXPECT_CALL(*connection_, UpdateBackup)
      .WillOnce([expire_time](btadmin::UpdateBackupRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kBackupName, request.backup().name());
        EXPECT_EQ(expire_time,
                  ToChronoTimePoint(request.backup().expire_time()));
        EXPECT_THAT(request.update_mask().paths(), ElementsAre("expire_time"));
        return FailingStatus();
      });

  TableAdmin::UpdateBackupParams params(kClusterId, kBackupId, expire_time);
  EXPECT_THAT(admin.UpdateBackup(params),
              StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(TableAdminTest, DeleteBackup) {
  auto admin = DefaultTableAdmin();

  EXPECT_CALL(*connection_, DeleteBackup)
      .Times(2)
      .WillRepeatedly([](btadmin::DeleteBackupRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kBackupName, request.name());
        return FailingStatus();
      });

  btadmin::Backup b;
  b.set_name(kBackupName);
  EXPECT_THAT(admin.DeleteBackup(b), StatusIs(StatusCode::kPermissionDenied));

  EXPECT_THAT(admin.DeleteBackup(kClusterId, kBackupId),
              StatusIs(StatusCode::kPermissionDenied));
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

TEST_F(TableAdminTest, ListBackupsSuccess) {
  auto admin = DefaultTableAdmin();
  std::vector<std::string> const expected_names = {
      BackupName(kProjectId, kInstanceId, kClusterId, "b0"),
      BackupName(kProjectId, kInstanceId, kClusterId, "b1")};

  auto iter = expected_names.begin();
  EXPECT_CALL(*connection_, ListBackups)
      .WillOnce([&iter,
                 &expected_names](btadmin::ListBackupsRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kClusterName, request.parent());

        using ::google::cloud::internal::MakeStreamRange;
        using ::google::cloud::internal::StreamReader;
        auto reader =
            [&iter,
             &expected_names]() -> StreamReader<btadmin::Backup>::result_type {
          if (iter != expected_names.end()) {
            btadmin::Backup b;
            b.set_name(*iter);
            ++iter;
            return b;
          }
          return Status();
        };
        return MakeStreamRange<btadmin::Backup>(std::move(reader));
      });

  auto params = TableAdmin::ListBackupsParams().set_cluster(kClusterId);
  auto backups = admin.ListBackups(params);
  ASSERT_STATUS_OK(backups);
  std::vector<std::string> names;
  std::transform(backups->begin(), backups->end(), std::back_inserter(names),
                 [](btadmin::Backup const& b) { return b.name(); });

  EXPECT_THAT(names, ElementsAreArray(expected_names));
}

TEST_F(TableAdminTest, ListBackupsFailure) {
  auto admin = DefaultTableAdmin();

  EXPECT_CALL(*connection_, ListBackups)
      .WillOnce([](btadmin::ListBackupsRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kClusterName, request.parent());

        using ::google::cloud::internal::MakeStreamRange;
        return MakeStreamRange<btadmin::Backup>([] { return FailingStatus(); });
      });

  auto params = TableAdmin::ListBackupsParams().set_cluster(kClusterId);
  EXPECT_THAT(admin.ListBackups(params),
              StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(TableAdminTest, RestoreTableParams) {
  TableAdmin::RestoreTableParams params(kTableId, kClusterId, kBackupId);
  auto request = params.AsProto(kInstanceName);
  EXPECT_EQ(kInstanceName, request.parent());
  EXPECT_EQ(kTableId, request.table_id());
  EXPECT_EQ(kBackupName, request.backup());
}

TEST_F(TableAdminTest, RestoreTable) {
  auto admin = DefaultTableAdmin();

  EXPECT_CALL(*connection_, RestoreTable)
      .Times(2)
      .WillRepeatedly([](btadmin::RestoreTableRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kInstanceName, request.parent());
        EXPECT_EQ(kTableId, request.table_id());
        EXPECT_EQ(kBackupName, request.backup());
        return make_ready_future<StatusOr<btadmin::Table>>(FailingStatus());
      });

  TableAdmin::RestoreTableParams params(kTableId, kClusterId, kBackupId);
  EXPECT_THAT(admin.RestoreTable(params),
              StatusIs(StatusCode::kPermissionDenied));

  TableAdmin::RestoreTableFromInstanceParams instance_params;
  instance_params.table_id = kTableId;
  instance_params.backup_name = kBackupName;
  EXPECT_THAT(admin.RestoreTable(instance_params),
              StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(TableAdminTest, ModifyColumnFamilies) {
  auto admin = DefaultTableAdmin();
  std::string expected_request = R"pb(
    name: 'projects/the-project/instances/the-instance/tables/the-table'
    modifications {
      id: 'foo'
      create { gc_rule { max_age { seconds: 172800 } } }
    }
    modifications {
      id: 'bar'
      update { gc_rule { max_age { seconds: 86400 } } }
    }
  )pb";

  EXPECT_CALL(*connection_, ModifyColumnFamilies)
      .WillOnce([&expected_request](
                    btadmin::ModifyColumnFamiliesRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        btadmin::ModifyColumnFamiliesRequest expected;
        EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(
            expected_request, &expected));
        EXPECT_THAT(expected, IsProtoEqual(request));
        return FailingStatus();
      });

  using M = ColumnFamilyModification;
  std::vector<ColumnFamilyModification> mods = {
      M::Create("foo", GcRule::MaxAge(hours(48))),
      M::Update("bar", GcRule::MaxAge(hours(24)))};
  EXPECT_THAT(admin.ModifyColumnFamilies(kTableId, mods),
              StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(TableAdminTest, DropRowsByPrefix) {
  auto admin = DefaultTableAdmin();

  EXPECT_CALL(*connection_, DropRowRange)
      .WillOnce([](btadmin::DropRowRangeRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kTableName, request.name());
        EXPECT_EQ("prefix", request.row_key_prefix());
        return FailingStatus();
      });

  EXPECT_THAT(admin.DropRowsByPrefix(kTableId, "prefix"),
              StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(TableAdminTest, WaitForConsistencySuccess) {
  auto admin = DefaultTableAdmin();

  EXPECT_CALL(*connection_, AsyncCheckConsistency)
      .WillOnce([](btadmin::CheckConsistencyRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kTableName, request.name());
        EXPECT_EQ("consistency-token", request.consistency_token());
        btadmin::CheckConsistencyResponse resp;
        resp.set_consistent(true);
        return make_ready_future(make_status_or(resp));
      });

  auto consistent =
      admin.WaitForConsistency(kTableId, "consistency-token").get();
  ASSERT_STATUS_OK(consistent);
  EXPECT_EQ(*consistent, Consistency::kConsistent);
}

TEST_F(TableAdminTest, WaitForConsistencyFailure) {
  auto admin = DefaultTableAdmin();

  EXPECT_CALL(*connection_, AsyncCheckConsistency)
      .WillOnce([](btadmin::CheckConsistencyRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kTableName, request.name());
        EXPECT_EQ("consistency-token", request.consistency_token());
        return make_ready_future<StatusOr<btadmin::CheckConsistencyResponse>>(
            FailingStatus());
      });

  EXPECT_THAT(admin.WaitForConsistency(kTableId, "consistency-token").get(),
              StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(TableAdminTest, DropAllRows) {
  auto admin = DefaultTableAdmin();

  EXPECT_CALL(*connection_, DropRowRange)
      .WillOnce([](btadmin::DropRowRangeRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kTableName, request.name());
        EXPECT_TRUE(request.delete_all_data_from_table());
        return FailingStatus();
      });

  EXPECT_THAT(admin.DropAllRows(kTableId),
              StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(TableAdminTest, GenerateConsistencyTokenSuccess) {
  auto admin = DefaultTableAdmin();

  EXPECT_CALL(*connection_, GenerateConsistencyToken)
      .WillOnce([](btadmin::GenerateConsistencyTokenRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kTableName, request.name());
        btadmin::GenerateConsistencyTokenResponse resp;
        resp.set_consistency_token("consistency-token");
        return resp;
      });

  auto token = admin.GenerateConsistencyToken(kTableId);
  ASSERT_STATUS_OK(token);
  EXPECT_EQ(*token, "consistency-token");
}

TEST_F(TableAdminTest, GenerateConsistencyTokenFailure) {
  auto admin = DefaultTableAdmin();

  EXPECT_CALL(*connection_, GenerateConsistencyToken)
      .WillOnce([](btadmin::GenerateConsistencyTokenRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kTableName, request.name());
        return FailingStatus();
      });

  EXPECT_THAT(admin.GenerateConsistencyToken(kTableId),
              StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(TableAdminTest, CheckConsistencySuccess) {
  auto admin = DefaultTableAdmin();

  EXPECT_CALL(*connection_, CheckConsistency)
      .WillOnce([](btadmin::CheckConsistencyRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kTableName, request.name());
        EXPECT_EQ("consistency-token", request.consistency_token());
        btadmin::CheckConsistencyResponse resp;
        resp.set_consistent(false);
        return resp;
      })
      .WillOnce([](btadmin::CheckConsistencyRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kTableName, request.name());
        EXPECT_EQ("consistency-token", request.consistency_token());
        btadmin::CheckConsistencyResponse resp;
        resp.set_consistent(true);
        return resp;
      });

  auto consistent = admin.CheckConsistency(kTableId, "consistency-token");
  ASSERT_STATUS_OK(consistent);
  EXPECT_EQ(*consistent, Consistency::kInconsistent);

  consistent = admin.CheckConsistency(kTableId, "consistency-token");
  ASSERT_STATUS_OK(consistent);
  EXPECT_EQ(*consistent, Consistency::kConsistent);
}

TEST_F(TableAdminTest, CheckConsistencyFailure) {
  auto admin = DefaultTableAdmin();

  EXPECT_CALL(*connection_, CheckConsistency)
      .WillOnce([](btadmin::CheckConsistencyRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kTableName, request.name());
        EXPECT_EQ("consistency-token", request.consistency_token());
        return FailingStatus();
      });

  EXPECT_THAT(admin.CheckConsistency(kTableId, "consistency-token"),
              StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(TableAdminTest, GetIamPolicyTable) {
  auto admin = DefaultTableAdmin();

  EXPECT_CALL(*connection_, GetIamPolicy)
      .WillOnce([](iamproto::GetIamPolicyRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kTableName, request.resource());
        return FailingStatus();
      });

  EXPECT_THAT(admin.GetIamPolicy(kTableId),
              StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(TableAdminTest, GetIamPolicyBackup) {
  auto admin = DefaultTableAdmin();

  EXPECT_CALL(*connection_, GetIamPolicy)
      .WillOnce([](iamproto::GetIamPolicyRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kBackupName, request.resource());
        return FailingStatus();
      });

  EXPECT_THAT(admin.GetIamPolicy(kClusterId, kBackupId),
              StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(TableAdminTest, SetIamPolicyTable) {
  auto admin = DefaultTableAdmin();

  EXPECT_CALL(*connection_, SetIamPolicy)
      .WillOnce([](iamproto::SetIamPolicyRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kTableName, request.resource());
        EXPECT_EQ(3, request.policy().version());
        EXPECT_EQ("tag", request.policy().etag());
        return FailingStatus();
      });

  iamproto::Policy p;
  p.set_version(3);
  p.set_etag("tag");
  EXPECT_THAT(admin.SetIamPolicy(kTableId, p),
              StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(TableAdminTest, SetIamPolicyBackup) {
  auto admin = DefaultTableAdmin();

  EXPECT_CALL(*connection_, SetIamPolicy)
      .WillOnce([](iamproto::SetIamPolicyRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kBackupName, request.resource());
        EXPECT_EQ(3, request.policy().version());
        EXPECT_EQ("tag", request.policy().etag());
        return FailingStatus();
      });

  iamproto::Policy p;
  p.set_version(3);
  p.set_etag("tag");
  EXPECT_THAT(admin.SetIamPolicy(kClusterId, kBackupId, p),
              StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(TableAdminTest, TestIamPermissionsTableSuccess) {
  auto admin = DefaultTableAdmin();
  std::vector<std::string> const expected_permissions = {"writer", "reader"};
  std::vector<std::string> const returned_permissions = {"reader"};

  EXPECT_CALL(*connection_, TestIamPermissions)
      .WillOnce([&](iamproto::TestIamPermissionsRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kTableName, request.resource());
        std::vector<std::string> actual_permissions;
        for (auto const& c : request.permissions()) {
          actual_permissions.emplace_back(c);
        }
        EXPECT_THAT(actual_permissions,
                    UnorderedElementsAreArray(expected_permissions));

        iamproto::TestIamPermissionsResponse r;
        for (auto const& p : returned_permissions) r.add_permissions(p);
        return r;
      });

  auto resp = admin.TestIamPermissions(kTableId, expected_permissions);
  ASSERT_STATUS_OK(resp);
  EXPECT_THAT(*resp, ElementsAreArray(returned_permissions));
}

TEST_F(TableAdminTest, TestIamPermissionsTableFailure) {
  auto admin = DefaultTableAdmin();
  std::vector<std::string> const expected_permissions = {"writer", "reader"};

  EXPECT_CALL(*connection_, TestIamPermissions)
      .WillOnce([&](iamproto::TestIamPermissionsRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kTableName, request.resource());
        std::vector<std::string> actual_permissions;
        for (auto const& c : request.permissions()) {
          actual_permissions.emplace_back(c);
        }
        EXPECT_THAT(actual_permissions,
                    UnorderedElementsAreArray(expected_permissions));
        return FailingStatus();
      });

  EXPECT_THAT(admin.TestIamPermissions(kTableId, expected_permissions),
              StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(TableAdminTest, TestIamPermissionsBackupSuccess) {
  auto admin = DefaultTableAdmin();
  std::vector<std::string> const expected_permissions = {"writer", "reader"};
  std::vector<std::string> const returned_permissions = {"reader"};

  EXPECT_CALL(*connection_, TestIamPermissions)
      .WillOnce([&](iamproto::TestIamPermissionsRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kBackupName, request.resource());
        std::vector<std::string> actual_permissions;
        for (auto const& c : request.permissions()) {
          actual_permissions.emplace_back(c);
        }
        EXPECT_THAT(actual_permissions,
                    UnorderedElementsAreArray(expected_permissions));

        iamproto::TestIamPermissionsResponse r;
        for (auto const& p : returned_permissions) r.add_permissions(p);
        return r;
      });

  auto resp =
      admin.TestIamPermissions(kClusterId, kBackupId, expected_permissions);
  ASSERT_STATUS_OK(resp);
  EXPECT_THAT(*resp, ElementsAreArray(returned_permissions));
}

TEST_F(TableAdminTest, TestIamPermissionsBackupFailure) {
  auto admin = DefaultTableAdmin();
  std::vector<std::string> const expected_permissions = {"writer", "reader"};

  EXPECT_CALL(*connection_, TestIamPermissions)
      .WillOnce([&](iamproto::TestIamPermissionsRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kBackupName, request.resource());
        std::vector<std::string> actual_permissions;
        for (auto const& c : request.permissions()) {
          actual_permissions.emplace_back(c);
        }
        EXPECT_THAT(actual_permissions,
                    UnorderedElementsAreArray(expected_permissions));
        return FailingStatus();
      });

  EXPECT_THAT(
      admin.TestIamPermissions(kClusterId, kBackupId, expected_permissions),
      StatusIs(StatusCode::kPermissionDenied));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
