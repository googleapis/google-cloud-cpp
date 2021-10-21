// Copyright 2017 Google Inc.
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

#include "google/cloud/bigtable/table_admin.h"
#include "google/cloud/bigtable/testing/mock_admin_client.h"
#include "google/cloud/bigtable/testing/mock_async_failing_rpc_factory.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/internal/time_utils.h"
#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/fake_completion_queue_impl.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/mock_async_response_reader.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include "absl/memory/memory.h"
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/time_util.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {

class TableAdminTester {
 public:
  TableAdminTester() = default;

  void SetTableAdmin(std::unique_ptr<TableAdmin> table_admin) {
    table_admin_ = std::move(table_admin);
  }

  future<StatusOr<google::bigtable::admin::v2::Backup>> AsyncCreateBackupImpl(
      CompletionQueue& cq, TableAdmin::CreateBackupParams const& params) {
    return table_admin_->AsyncCreateBackupImpl(cq, params);
  };

  future<StatusOr<google::bigtable::admin::v2::Table>> AsyncRestoreTableImpl(
      CompletionQueue& cq, TableAdmin::RestoreTableParams const& params) {
    return table_admin_->AsyncRestoreTableImpl(cq, params);
  }

  future<StatusOr<Consistency>> AsyncWaitForConsistencyImpl(
      CompletionQueue& cq, std::string const& table_id,
      std::string const& consistency_token) {
    return table_admin_->AsyncWaitForConsistencyImpl(cq, table_id,
                                                     consistency_token);
  }

 private:
  std::unique_ptr<TableAdmin> table_admin_;
};

namespace {

namespace btadmin = ::google::bigtable::admin::v2;

using ::google::cloud::testing_util::IsContextMDValid;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::chrono_literals::operator"" _h;
using ::google::cloud::testing_util::chrono_literals::operator"" _min;
using ::google::cloud::testing_util::chrono_literals::operator"" _s;
using ::google::cloud::testing_util::chrono_literals::operator"" _ms;
using ::google::cloud::testing_util::FakeCompletionQueueImpl;
using ::testing::Not;
using ::testing::Return;
using ::testing::ReturnRef;

using MockAdminClient = ::google::cloud::bigtable::testing::MockAdminClient;
using MockAsyncCheckConsistencyResponse =
    ::google::cloud::testing_util::MockAsyncResponseReader<
        btadmin::CheckConsistencyResponse>;

std::string const kProjectId = "the-project";
std::string const kInstanceId = "the-instance";

/// A fixture for the bigtable::TableAdmin tests.
class TableAdminTest : public ::testing::Test {
 protected:
  void SetUp() override {
    EXPECT_CALL(*client_, project()).WillRepeatedly(ReturnRef(kProjectId));
  }

  std::shared_ptr<MockAdminClient> client_ =
      std::make_shared<MockAdminClient>();
};

// A lambda to create lambdas.  Basically we would be rewriting the same
// lambda twice without this thing.
auto create_list_tables_lambda =
    [](std::string const& expected_token, std::string const& returned_token,
       std::vector<std::string> const& table_names) {
      return [expected_token, returned_token, table_names](
                 grpc::ClientContext* context,
                 btadmin::ListTablesRequest const& request,
                 btadmin::ListTablesResponse* response) {
        EXPECT_STATUS_OK(IsContextMDValid(
            *context, "google.bigtable.admin.v2.BigtableTableAdmin.ListTables",
            google::cloud::internal::ApiClientHeader()));
        auto const instance_name =
            "projects/" + kProjectId + "/instances/" + kInstanceId;
        EXPECT_EQ(instance_name, request.parent());
        EXPECT_EQ(btadmin::Table::FULL, request.view());
        EXPECT_EQ(expected_token, request.page_token());

        EXPECT_NE(nullptr, response);
        for (auto const& table_name : table_names) {
          auto& table = *response->add_tables();
          // NOLINTNEXTLINE(performance-inefficient-string-concatenation)
          table.set_name(instance_name + "/tables/" + table_name);
          table.set_granularity(btadmin::Table::MILLIS);
        }
        // Return the right token.
        response->set_next_page_token(returned_token);
        return grpc::Status::OK;
      };
    };

auto create_get_policy_mock = []() {
  return [](grpc::ClientContext* context,
            ::google::iam::v1::GetIamPolicyRequest const&,
            ::google::iam::v1::Policy* response) {
    EXPECT_STATUS_OK(IsContextMDValid(
        *context, "google.bigtable.admin.v2.BigtableTableAdmin.GetIamPolicy",
        google::cloud::internal::ApiClientHeader()));
    EXPECT_NE(nullptr, response);
    response->set_version(3);
    response->set_etag("random-tag");
    return grpc::Status::OK;
  };
};

auto create_get_policy_mock_for_backup = [](std::string const& backup_id) {
  return [backup_id](grpc::ClientContext* context,
                     ::google::iam::v1::GetIamPolicyRequest const&,
                     ::google::iam::v1::Policy* response) {
    EXPECT_STATUS_OK(IsContextMDValid(
        *context, "google.bigtable.admin.v2.BigtableTableAdmin.GetIamPolicy",
        google::cloud::internal::ApiClientHeader(), backup_id));
    EXPECT_NE(nullptr, response);
    response->set_version(3);
    response->set_etag("random-tag");
    return grpc::Status::OK;
  };
};
auto create_policy_with_params = []() {
  return [](grpc::ClientContext* context,
            ::google::iam::v1::SetIamPolicyRequest const& request,
            ::google::iam::v1::Policy* response) {
    EXPECT_STATUS_OK(IsContextMDValid(
        *context, "google.bigtable.admin.v2.BigtableTableAdmin.SetIamPolicy",
        google::cloud::internal::ApiClientHeader()));
    EXPECT_NE(nullptr, response);
    *response = request.policy();
    return grpc::Status::OK;
  };
};

auto create_policy_with_params_for_backup = [](std::string const& backup_id) {
  return [backup_id](grpc::ClientContext* context,
                     ::google::iam::v1::SetIamPolicyRequest const& request,
                     ::google::iam::v1::Policy* response) {
    EXPECT_STATUS_OK(IsContextMDValid(
        *context, "google.bigtable.admin.v2.BigtableTableAdmin.SetIamPolicy",
        google::cloud::internal::ApiClientHeader(), backup_id));
    EXPECT_NE(nullptr, response);
    *response = request.policy();
    return grpc::Status::OK;
  };
};
auto create_list_backups_lambda =
    [](std::string const& expected_token, std::string const& returned_token,
       std::vector<std::string> const& backup_names) {
      return [expected_token, returned_token, backup_names](
                 grpc::ClientContext* context,
                 btadmin::ListBackupsRequest const& request,
                 btadmin::ListBackupsResponse* response) {
        EXPECT_STATUS_OK(IsContextMDValid(
            *context, "google.bigtable.admin.v2.BigtableTableAdmin.ListBackups",
            google::cloud::internal::ApiClientHeader()));
        auto const instance_name =
            "projects/" + kProjectId + "/instances/" + kInstanceId;
        auto const cluster_name = instance_name + "/clusters/-";
        EXPECT_EQ(cluster_name, request.parent());
        EXPECT_EQ(expected_token, request.page_token());

        EXPECT_NE(nullptr, response);
        for (auto const& backup_name : backup_names) {
          auto& backup = *response->add_backups();
          // NOLINTNEXTLINE(performance-inefficient-string-concatenation)
          backup.set_name(instance_name + "/clusters/the-cluster/backups/" +
                          backup_name);
        }
        // Return the right token.
        response->set_next_page_token(returned_token);
        return grpc::Status::OK;
      };
    };

/**
 * Helper class to create the expectations for a simple RPC call.
 *
 * Given the type of the request and responses, this struct provides a function
 * to create a mock implementation with the right signature and checks.
 *
 * @tparam RequestType the protobuf type for the request.
 * @tparam ResponseType the protobuf type for the response.
 */
template <typename RequestType, typename ResponseType>
struct MockRpcFactory {
  using SignatureType = grpc::Status(grpc::ClientContext* ctx,
                                     RequestType const& request,
                                     ResponseType* response);

  /// Refactor the boilerplate common to most tests.
  static std::function<SignatureType> Create(
      std::string const& expected_request, std::string const& method) {
    return std::function<SignatureType>(
        [expected_request, method](grpc::ClientContext* context,
                                   RequestType const& request,
                                   ResponseType* response) {
          EXPECT_STATUS_OK(IsContextMDValid(
              *context, method, google::cloud::internal::ApiClientHeader()));
          if (response == nullptr) {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                                "invalid call to MockRpcFactory::Create()");
          }
          RequestType expected;
          // Cannot use ASSERT_TRUE() here, it has an embedded "return;"
          EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(
              expected_request, &expected));
          EXPECT_THAT(expected, IsProtoEqual(request));

          return grpc::Status::OK;
        });
  }
};

/// @test Verify basic functionality in the `bigtable::TableAdmin` class.
TEST_F(TableAdminTest, Default) {
  TableAdmin tested(client_, "the-instance");
  EXPECT_EQ("the-instance", tested.instance_id());
  EXPECT_EQ("projects/the-project/instances/the-instance",
            tested.instance_name());
}

/// @test Verify that `bigtable::TableAdmin::ListTables` works in the easy case.
TEST_F(TableAdminTest, ListTables) {
  TableAdmin tested(client_, kInstanceId);
  auto mock_list_tables = create_list_tables_lambda("", "", {"t0", "t1"});
  EXPECT_CALL(*client_, ListTables).WillOnce(mock_list_tables);

  // After all the setup, make the actual call we want to test.
  auto actual = tested.ListTables(btadmin::Table::FULL);
  ASSERT_STATUS_OK(actual);
  auto const& v = *actual;
  std::string instance_name = tested.instance_name();
  ASSERT_EQ(2UL, v.size());
  EXPECT_EQ(instance_name + "/tables/t0", v[0].name());
  EXPECT_EQ(instance_name + "/tables/t1", v[1].name());
}

/// @test Verify that `bigtable::TableAdmin::ListTables` handles failures.
TEST_F(TableAdminTest, ListTablesRecoverableFailures) {
  TableAdmin tested(client_, "the-instance");
  auto mock_recoverable_failure = [](grpc::ClientContext* context,
                                     btadmin::ListTablesRequest const&,
                                     btadmin::ListTablesResponse*) {
    EXPECT_STATUS_OK(IsContextMDValid(
        *context, "google.bigtable.admin.v2.BigtableTableAdmin.ListTables",
        google::cloud::internal::ApiClientHeader()));
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
  };
  auto batch0 = create_list_tables_lambda("", "token-001", {"t0", "t1"});
  auto batch1 = create_list_tables_lambda("token-001", "", {"t2", "t3"});
  EXPECT_CALL(*client_, ListTables)
      .WillOnce(mock_recoverable_failure)
      .WillOnce(batch0)
      .WillOnce(mock_recoverable_failure)
      .WillOnce(mock_recoverable_failure)
      .WillOnce(batch1);

  // After all the setup, make the actual call we want to test.
  auto actual = tested.ListTables(btadmin::Table::FULL);
  ASSERT_STATUS_OK(actual);
  auto const& v = *actual;
  std::string instance_name = tested.instance_name();
  ASSERT_EQ(4UL, v.size());
  EXPECT_EQ(instance_name + "/tables/t0", v[0].name());
  EXPECT_EQ(instance_name + "/tables/t1", v[1].name());
  EXPECT_EQ(instance_name + "/tables/t2", v[2].name());
  EXPECT_EQ(instance_name + "/tables/t3", v[3].name());
}

/**
 * @test Verify that `bigtable::TableAdmin::ListTables` handles unrecoverable
 * failures.
 */
TEST_F(TableAdminTest, ListTablesUnrecoverableFailures) {
  TableAdmin tested(client_, "the-instance");
  EXPECT_CALL(*client_, ListTables)
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

  EXPECT_FALSE(tested.ListTables(btadmin::Table::FULL));
}

/**
 * @test Verify that `bigtable::TableAdmin::ListTables` handles too many
 * recoverable failures.
 */
TEST_F(TableAdminTest, ListTablesTooManyFailures) {
  TableAdmin tested(client_, "the-instance", LimitedErrorCountRetryPolicy(3),
                    ExponentialBackoffPolicy(10_ms, 10_min));
  auto mock_recoverable_failure = [](grpc::ClientContext* context,
                                     btadmin::ListTablesRequest const&,
                                     btadmin::ListTablesResponse*) {
    EXPECT_STATUS_OK(IsContextMDValid(
        *context, "google.bigtable.admin.v2.BigtableTableAdmin.ListTables",
        google::cloud::internal::ApiClientHeader()));
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
  };
  EXPECT_CALL(*client_, ListTables).WillRepeatedly(mock_recoverable_failure);

  EXPECT_FALSE(tested.ListTables(btadmin::Table::FULL));
}

/// @test Verify that `bigtable::TableAdmin::Create` works in the easy case.
TEST_F(TableAdminTest, CreateTableSimple) {
  TableAdmin tested(client_, "the-instance");

  std::string expected_text = R"pb(
    parent: 'projects/the-project/instances/the-instance'
    table_id: 'new-table'
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
  auto mock_create_table =
      MockRpcFactory<btadmin::CreateTableRequest, btadmin::Table>::Create(
          expected_text,
          "google.bigtable.admin.v2.BigtableTableAdmin.CreateTable");
  EXPECT_CALL(*client_, CreateTable).WillOnce(mock_create_table);

  // After all the setup, make the actual call we want to test.
  using GC = GcRule;
  TableConfig config({{"f1", GC::MaxNumVersions(1)}, {"f2", GC::MaxAge(1_s)}},
                     {"a", "c", "p"});
  auto table = tested.CreateTable("new-table", std::move(config));
  EXPECT_STATUS_OK(table);
}

/**
 * @test Verify that `bigtable::TableAdmin::CreateTable` supports
 * only one try and let client know request status.
 */
TEST_F(TableAdminTest, CreateTableFailure) {
  TableAdmin tested(client_, "the-instance");
  EXPECT_CALL(*client_, CreateTable)
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

  EXPECT_FALSE(tested.CreateTable("other-table", TableConfig()));
}

/**
 * @test Verify that Copy Constructor and assignment operator
 * copies all properties.
 */
TEST_F(TableAdminTest, CopyConstructibleAssignableTest) {
  // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
  TableAdmin tested(client_, "the-copy-instance");
  // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
  TableAdmin table_admin(tested);

  EXPECT_EQ(tested.instance_id(), table_admin.instance_id());
  EXPECT_EQ(tested.instance_name(), table_admin.instance_name());
  EXPECT_EQ(tested.project(), table_admin.project());

  TableAdmin table_admin_assign(client_, "the-assign-instance");
  EXPECT_NE(tested.instance_id(), table_admin_assign.instance_id());
  EXPECT_NE(tested.instance_name(), table_admin_assign.instance_name());

  table_admin_assign = tested;
  EXPECT_EQ(tested.instance_id(), table_admin_assign.instance_id());
  EXPECT_EQ(tested.instance_name(), table_admin_assign.instance_name());
  EXPECT_EQ(tested.project(), table_admin_assign.project());
}

/**
 * @test Verify that Copy Constructor and assignment operator copies
 * all properties including policies applied.
 */
TEST_F(TableAdminTest, CopyConstructibleAssignablePolicyTest) {
  TableAdmin tested(client_, "the-construct-instance",
                    LimitedErrorCountRetryPolicy(3),
                    ExponentialBackoffPolicy(10_ms, 10_min));
  // Copy Constructor
  TableAdmin table_admin(tested);
  // Create New Instance
  TableAdmin table_admin_assign(client_, "the-assign-instance");
  // Copy assignable
  table_admin_assign = table_admin;

  EXPECT_CALL(*client_, GetTable)
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")));

  EXPECT_FALSE(table_admin.GetTable("other-table"));
  EXPECT_FALSE(table_admin_assign.GetTable("other-table"));
}

/// @test Verify that `bigtable::TableAdmin::GetTable` works in the easy case.
TEST_F(TableAdminTest, GetTableSimple) {
  TableAdmin tested(client_, "the-instance");
  std::string expected_text = R"pb(
    name: 'projects/the-project/instances/the-instance/tables/the-table'
    view: SCHEMA_VIEW
  )pb";
  auto mock = MockRpcFactory<btadmin::GetTableRequest, btadmin::Table>::Create(
      expected_text, "google.bigtable.admin.v2.BigtableTableAdmin.GetTable");
  EXPECT_CALL(*client_, GetTable)
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")))
      .WillOnce(mock);

  // After all the setup, make the actual call we want to test.
  tested.GetTable("the-table");
}

/**
 * @test Verify that `bigtable::TableAdmin::GetTable` reports unrecoverable
 * failures.
 */
TEST_F(TableAdminTest, GetTableUnrecoverableFailures) {
  TableAdmin tested(client_, "the-instance");
  EXPECT_CALL(*client_, GetTable)
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::NOT_FOUND, "uh oh")));

  // After all the setup, make the actual call we want to test.
  EXPECT_FALSE(tested.GetTable("other-table"));
}

/**
 * @test Verify that `bigtable::TableAdmin::GetTable` works with too many
 * recoverable failures.
 */
TEST_F(TableAdminTest, GetTableTooManyFailures) {
  TableAdmin tested(client_, "the-instance", LimitedErrorCountRetryPolicy(3),
                    ExponentialBackoffPolicy(10_ms, 10_min));
  EXPECT_CALL(*client_, GetTable)
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")));

  // After all the setup, make the actual call we want to test.
  EXPECT_FALSE(tested.GetTable("other-table"));
}

/// @test Verify that bigtable::TableAdmin::DeleteTable works as expected.
TEST_F(TableAdminTest, DeleteTable) {
  using google::protobuf::Empty;

  TableAdmin tested(client_, "the-instance");
  std::string expected_text = R"pb(
    name: 'projects/the-project/instances/the-instance/tables/the-table'
  )pb";
  auto mock = MockRpcFactory<btadmin::DeleteTableRequest, Empty>::Create(
      expected_text, "google.bigtable.admin.v2.BigtableTableAdmin.DeleteTable");
  EXPECT_CALL(*client_, DeleteTable).WillOnce(mock);

  // After all the setup, make the actual call we want to test.
  EXPECT_STATUS_OK(tested.DeleteTable("the-table"));
}

/**
 * @test Verify that `bigtable::TableAdmin::DeleteTable` supports
 * only one try and let client know request status.
 */
TEST_F(TableAdminTest, DeleteTableFailure) {
  TableAdmin tested(client_, "the-instance");
  EXPECT_CALL(*client_, DeleteTable)
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

  // After all the setup, make the actual call we want to test.
  EXPECT_THAT(tested.DeleteTable("other-table"), Not(IsOk()));
}

/// @test Verify that `bigtable::TableAdmin::ListBackups` works in the easy
/// case.
TEST_F(TableAdminTest, ListBackups) {
  TableAdmin tested(client_, kInstanceId);
  auto mock_list_backups = create_list_backups_lambda("", "", {"b0", "b1"});
  EXPECT_CALL(*client_, ListBackups).WillOnce(mock_list_backups);

  // After all the setup, make the actual call we want to test.
  auto actual = tested.ListBackups({});
  ASSERT_STATUS_OK(actual);
  auto const& v = *actual;
  std::string instance_name = tested.instance_name();
  ASSERT_EQ(2UL, v.size());
  EXPECT_EQ(instance_name + "/clusters/the-cluster/backups/b0", v[0].name());
  EXPECT_EQ(instance_name + "/clusters/the-cluster/backups/b1", v[1].name());
}

/// @test Verify that `bigtable::TableAdmin::ListBackups` handles failures.
TEST_F(TableAdminTest, ListBackupsRecoverableFailures) {
  TableAdmin tested(client_, "the-instance");
  auto mock_recoverable_failure = [](grpc::ClientContext* context,
                                     btadmin::ListBackupsRequest const&,
                                     btadmin::ListBackupsResponse*) {
    EXPECT_STATUS_OK(IsContextMDValid(
        *context, "google.bigtable.admin.v2.BigtableTableAdmin.ListBackups",
        google::cloud::internal::ApiClientHeader()));
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
  };
  auto batch0 = create_list_backups_lambda("", "token-001", {"b0", "b1"});
  auto batch1 = create_list_backups_lambda("token-001", "", {"b2", "b3"});
  EXPECT_CALL(*client_, ListBackups)
      .WillOnce(mock_recoverable_failure)
      .WillOnce(batch0)
      .WillOnce(mock_recoverable_failure)
      .WillOnce(mock_recoverable_failure)
      .WillOnce(batch1);

  // After all the setup, make the actual call we want to test.
  auto actual = tested.ListBackups({});
  ASSERT_STATUS_OK(actual);
  auto const& v = *actual;
  std::string instance_name = tested.instance_name();
  ASSERT_EQ(4UL, v.size());
  EXPECT_EQ(instance_name + "/clusters/the-cluster/backups/b0", v[0].name());
  EXPECT_EQ(instance_name + "/clusters/the-cluster/backups/b1", v[1].name());
  EXPECT_EQ(instance_name + "/clusters/the-cluster/backups/b2", v[2].name());
  EXPECT_EQ(instance_name + "/clusters/the-cluster/backups/b3", v[3].name());
}

/**
 * @test Verify that `bigtable::TableAdmin::ListBackups` handles unrecoverable
 * failures.
 */
TEST_F(TableAdminTest, ListBackupsUnrecoverableFailures) {
  TableAdmin tested(client_, "the-instance");
  EXPECT_CALL(*client_, ListBackups)
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

  EXPECT_FALSE(tested.ListBackups({}));
}

/**
 * @test Verify that `bigtable::TableAdmin::ListBackups` handles too many
 * recoverable failures.
 */
TEST_F(TableAdminTest, ListBackupsTooManyFailures) {
  TableAdmin tested(client_, "the-instance", LimitedErrorCountRetryPolicy(3),
                    ExponentialBackoffPolicy(10_ms, 10_min));
  auto mock_recoverable_failure = [](grpc::ClientContext* context,
                                     btadmin::ListBackupsRequest const&,
                                     btadmin::ListBackupsResponse*) {
    EXPECT_STATUS_OK(IsContextMDValid(
        *context, "google.bigtable.admin.v2.BigtableTableAdmin.ListBackups",
        google::cloud::internal::ApiClientHeader()));
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
  };
  EXPECT_CALL(*client_, ListBackups).WillRepeatedly(mock_recoverable_failure);

  EXPECT_FALSE(tested.ListBackups({}));
}

/// @test Verify that `bigtable::TableAdmin::GetBackup` works in the easy case.
TEST_F(TableAdminTest, GetBackupSimple) {
  TableAdmin tested(client_, "the-instance");
  std::string expected_text = R"pb(
    name: 'projects/the-project/instances/the-instance/clusters/the-cluster/backups/the-backup'
  )pb";
  auto mock =
      MockRpcFactory<btadmin::GetBackupRequest, btadmin::Backup>::Create(
          expected_text,
          "google.bigtable.admin.v2.BigtableTableAdmin.GetBackup");
  EXPECT_CALL(*client_, GetBackup)
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")))
      .WillOnce(mock);

  // After all the setup, make the actual call we want to test.
  tested.GetBackup("the-cluster", "the-backup");
}

/**
 * @test Verify that `bigtable::TableAdmin::GetBackup` reports unrecoverable
 * failures.
 */
TEST_F(TableAdminTest, GetBackupUnrecoverableFailures) {
  TableAdmin tested(client_, "the-instance");
  EXPECT_CALL(*client_, GetBackup)
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::NOT_FOUND, "uh oh")));

  // After all the setup, make the actual call we want to test.
  EXPECT_FALSE(tested.GetBackup("other-cluster", "other-table"));
}

/**
 * @test Verify that `bigtable::TableAdmin::GetBackup` works with too many
 * recoverable failures.
 */
TEST_F(TableAdminTest, GetBackupTooManyFailures) {
  TableAdmin tested(client_, "the-instance", LimitedErrorCountRetryPolicy(3),
                    ExponentialBackoffPolicy(10_ms, 10_min));
  EXPECT_CALL(*client_, GetBackup)
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")));

  // After all the setup, make the actual call we want to test.
  EXPECT_FALSE(tested.GetBackup("other-cluster", "other-table"));
}

/// @test Verify that `bigtable::TableAdmin::UpdateBackup` works in the easy
/// case.
TEST_F(TableAdminTest, UpdateBackupSimple) {
  TableAdmin tested(client_, "the-instance");
  std::string expected_text = R"pb(
    backup {
      name: 'projects/the-project/instances/the-instance/clusters/the-cluster/backups/the-backup'
      expire_time: { seconds: 1893387600 }
    }
    update_mask: { paths: 'expire_time' }
  )pb";

  auto mock =
      MockRpcFactory<btadmin::UpdateBackupRequest, btadmin::Backup>::Create(
          expected_text,
          "google.bigtable.admin.v2.BigtableTableAdmin.UpdateBackup");
  EXPECT_CALL(*client_, UpdateBackup)
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")))
      .WillOnce(mock);

  // After all the setup, make the actual call we want to test.
  google::protobuf::Timestamp expire_time;
  EXPECT_TRUE(google::protobuf::util::TimeUtil::FromString(
      "2029-12-31T00:00:00.000-05:00", &expire_time));
  TableAdmin::UpdateBackupParams params(
      "the-cluster", "the-backup",
      google::cloud::internal::ToChronoTimePoint(expire_time));
  tested.UpdateBackup(std::move(params));
}

/**
 * @test Verify that `bigtable::TableAdmin::UpdateBackup` reports unrecoverable
 * failures.
 */
TEST_F(TableAdminTest, UpdateBackupUnrecoverableFailures) {
  TableAdmin tested(client_, "the-instance");
  EXPECT_CALL(*client_, UpdateBackup)
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::NOT_FOUND, "uh oh")));

  // After all the setup, make the actual call we want to test.
  google::protobuf::Timestamp expire_time;
  EXPECT_TRUE(google::protobuf::util::TimeUtil::FromString(
      "2029-12-31T00:00:00.000-05:00", &expire_time));
  TableAdmin::UpdateBackupParams params(
      "the-cluster", "the-backup",
      google::cloud::internal::ToChronoTimePoint(expire_time));
  EXPECT_FALSE(tested.UpdateBackup(std::move(params)));
}

/**
 * @test Verify that `bigtable::TableAdmin::UpdateBackup` works with too many
 * recoverable failures.
 */
TEST_F(TableAdminTest, UpdateBackupTooManyFailures) {
  TableAdmin tested(client_, "the-instance", LimitedErrorCountRetryPolicy(3),
                    ExponentialBackoffPolicy(10_ms, 10_min));
  EXPECT_CALL(*client_, UpdateBackup)
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")));

  // After all the setup, make the actual call we want to test.
  google::protobuf::Timestamp expire_time;
  EXPECT_TRUE(google::protobuf::util::TimeUtil::FromString(
      "2029-12-31T00:00:00.000-05:00", &expire_time));
  TableAdmin::UpdateBackupParams params(
      "the-cluster", "the-backup",
      google::cloud::internal::ToChronoTimePoint(expire_time));
  EXPECT_FALSE(tested.UpdateBackup(std::move(params)));
}

/// @test Verify that bigtable::TableAdmin::DeleteBackup works as expected.
TEST_F(TableAdminTest, DeleteBackup) {
  using google::protobuf::Empty;

  TableAdmin tested(client_, "the-instance");
  std::string expected_text = R"pb(
    name: 'projects/the-project/instances/the-instance/clusters/the-cluster/backups/the-backup'
  )pb";
  auto mock = MockRpcFactory<btadmin::DeleteBackupRequest, Empty>::Create(
      expected_text,
      "google.bigtable.admin.v2.BigtableTableAdmin.DeleteBackup");
  EXPECT_CALL(*client_, DeleteBackup).WillOnce(mock).WillOnce(mock);

  // After all the setup, make the actual call we want to test.
  EXPECT_STATUS_OK(tested.DeleteBackup("the-cluster", "the-backup"));

  btadmin::Backup backup;
  backup.set_name(
      "projects/the-project/instances/the-instance/clusters/the-cluster/"
      "backups/the-backup");
  EXPECT_STATUS_OK(tested.DeleteBackup(backup));
}

/**
 * @test Verify that `bigtable::TableAdmin::BackupTable` supports
 * only one try and let client know request status.
 */
TEST_F(TableAdminTest, DeleteBackupFailure) {
  TableAdmin tested(client_, "the-instance");
  EXPECT_CALL(*client_, DeleteBackup)
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

  // After all the setup, make the actual call we want to test.
  EXPECT_THAT(tested.DeleteBackup("the-cluster", "the-backup"), Not(IsOk()));
}

/**
 * @test Verify that bigtable::TableAdmin::ModifyColumnFamilies works as
 * expected.
 */
TEST_F(TableAdminTest, ModifyColumnFamilies) {
  using google::protobuf::Empty;

  TableAdmin tested(client_, "the-instance");
  std::string expected_text = R"pb(
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
  auto mock = MockRpcFactory<btadmin::ModifyColumnFamiliesRequest,
                             btadmin::Table>::
      Create(
          expected_text,
          "google.bigtable.admin.v2.BigtableTableAdmin.ModifyColumnFamilies");
  EXPECT_CALL(*client_, ModifyColumnFamilies).WillOnce(mock);

  // After all the setup, make the actual call we want to test.
  using M = ColumnFamilyModification;
  using GC = GcRule;
  auto actual = tested.ModifyColumnFamilies(
      "the-table",
      {M::Create("foo", GC::MaxAge(48_h)), M::Update("bar", GC::MaxAge(24_h))});
}

/**
 * @test Verify that `bigtable::TableAdmin::ModifyColumnFamilies` makes only one
 * RPC attempt and reports errors on failure.
 */
TEST_F(TableAdminTest, ModifyColumnFamiliesFailure) {
  TableAdmin tested(client_, "the-instance");
  EXPECT_CALL(*client_, ModifyColumnFamilies)
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

  using M = ColumnFamilyModification;
  using GC = GcRule;
  std::vector<M> changes{M::Create("foo", GC::MaxAge(48_h)),
                         M::Update("bar", GC::MaxAge(24_h))};

  EXPECT_FALSE(tested.ModifyColumnFamilies("other-table", std::move(changes)));
}

/// @test Verify that bigtable::TableAdmin::DropRowsByPrefix works as expected.
TEST_F(TableAdminTest, DropRowsByPrefix) {
  using google::protobuf::Empty;

  TableAdmin tested(client_, "the-instance");
  std::string expected_text = R"pb(
    name: 'projects/the-project/instances/the-instance/tables/the-table'
    row_key_prefix: 'foobar'
  )pb";
  auto mock = MockRpcFactory<btadmin::DropRowRangeRequest, Empty>::Create(
      expected_text,
      "google.bigtable.admin.v2.BigtableTableAdmin.DropRowRange");
  EXPECT_CALL(*client_, DropRowRange).WillOnce(mock);

  // After all the setup, make the actual call we want to test.
  EXPECT_STATUS_OK(tested.DropRowsByPrefix("the-table", "foobar"));
}

/**
 * @test Verify that `bigtable::TableAdmin::DropRowsByPrefix` makes only one
 * RPC attempt and reports errors on failure.
 */
TEST_F(TableAdminTest, DropRowsByPrefixFailure) {
  TableAdmin tested(client_, "the-instance");
  EXPECT_CALL(*client_, DropRowRange)
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

  EXPECT_THAT(tested.DropRowsByPrefix("other-table", "prefix"), Not(IsOk()));
}

/// @test Verify that bigtable::TableAdmin::DropRowsByPrefix works as expected.
TEST_F(TableAdminTest, DropAllRows) {
  using google::protobuf::Empty;

  TableAdmin tested(client_, "the-instance");
  std::string expected_text = R"pb(
    name: 'projects/the-project/instances/the-instance/tables/the-table'
    delete_all_data_from_table: true
  )pb";
  auto mock = MockRpcFactory<btadmin::DropRowRangeRequest, Empty>::Create(
      expected_text,
      "google.bigtable.admin.v2.BigtableTableAdmin.DropRowRange");
  EXPECT_CALL(*client_, DropRowRange).WillOnce(mock);

  // After all the setup, make the actual call we want to test.
  EXPECT_STATUS_OK(tested.DropAllRows("the-table"));
}

/**
 * @test Verify that `bigtable::TableAdmin::DropAllRows` makes only one
 * RPC attempt and reports errors on failure.
 */
TEST_F(TableAdminTest, DropAllRowsFailure) {
  TableAdmin tested(client_, "the-instance");
  EXPECT_CALL(*client_, DropRowRange)
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

  // After all the setup, make the actual call we want to test.
  EXPECT_THAT(tested.DropAllRows("other-table"), Not(IsOk()));
}

/**
 * @test Verify that `bigtagble::TableAdmin::GenerateConsistencyToken` works as
 * expected.
 */
TEST_F(TableAdminTest, GenerateConsistencyTokenSimple) {
  TableAdmin tested(client_, "the-instance");
  std::string expected_text = R"pb(
    name: 'projects/the-project/instances/the-instance/tables/the-table'
  )pb";
  auto mock = MockRpcFactory<btadmin::GenerateConsistencyTokenRequest,
                             btadmin::GenerateConsistencyTokenResponse>::
      Create(expected_text,
             "google.bigtable.admin.v2.BigtableTableAdmin."
             "GenerateConsistencyToken");
  EXPECT_CALL(*client_, GenerateConsistencyToken).WillOnce(mock);

  // After all the setup, make the actual call we want to test.
  tested.GenerateConsistencyToken("the-table");
}

/**
 * @test Verify that `bigtable::TableAdmin::GenerateConsistencyToken` makes only
 * one RPC attempt and reports errors on failure.
 */
TEST_F(TableAdminTest, GenerateConsistencyTokenFailure) {
  TableAdmin tested(client_, "the-instance");
  EXPECT_CALL(*client_, GenerateConsistencyToken)
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

  // After all the setup, make the actual call we want to test.
  EXPECT_FALSE(tested.GenerateConsistencyToken("other-table"));
}

/**
 * @test Verify that `bigtagble::TableAdmin::CheckConsistency` works as
 * expected.
 */
TEST_F(TableAdminTest, CheckConsistencySimple) {
  TableAdmin tested(client_, "the-instance");
  std::string expected_text = R"pb(
    name: 'projects/the-project/instances/the-instance/tables/the-table'
    consistency_token: 'test-token'
  )pb";
  auto mock = MockRpcFactory<btadmin::CheckConsistencyRequest,
                             btadmin::CheckConsistencyResponse>::
      Create(expected_text,
             "google.bigtable.admin.v2.BigtableTableAdmin.CheckConsistency");
  EXPECT_CALL(*client_, CheckConsistency).WillOnce(mock);

  // After all the setup, make the actual call we want to test.
  auto result = tested.CheckConsistency("the-table", "test-token");
  ASSERT_STATUS_OK(result);
}

/**
 * @test Verify that `bigtable::TableAdmin::CheckConsistency` makes only
 * one RPC attempt and reports errors on failure.
 */
TEST_F(TableAdminTest, CheckConsistencyFailure) {
  TableAdmin tested(client_, "the-instance");
  EXPECT_CALL(*client_, CheckConsistency)
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

  // After all the setup, make the actual call we want to test.
  EXPECT_FALSE(tested.CheckConsistency("other-table", "test-token"));
}

/// @test Verify positive scenario for TableAdmin::GetIamPolicy.
TEST_F(TableAdminTest, GetIamPolicy) {
  TableAdmin tested(client_, "the-instance");
  auto mock_policy = create_get_policy_mock();
  EXPECT_CALL(*client_, GetIamPolicy).WillOnce(mock_policy);

  std::string resource = "test-resource";
  auto policy = tested.GetIamPolicy(resource);
  ASSERT_STATUS_OK(policy);
  EXPECT_EQ(3, policy->version());
  EXPECT_EQ("random-tag", policy->etag());
}

/// @test Verify positive scenario for TableAdmin::GetIamPolicy.
TEST_F(TableAdminTest, GetIamPolicyForBackup) {
  TableAdmin tested(client_, "the-instance");
  auto mock_policy = create_get_policy_mock_for_backup("the-backup");
  EXPECT_CALL(*client_, GetIamPolicy).WillOnce(mock_policy);

  std::string resource = "test-resource";
  auto policy = tested.GetIamPolicy("the-cluster", resource);
  ASSERT_STATUS_OK(policy);
  EXPECT_EQ(3, policy->version());
  EXPECT_EQ("random-tag", policy->etag());
}

/// @test Verify unrecoverable errors for TableAdmin::GetIamPolicy.
TEST_F(TableAdminTest, GetIamPolicyUnrecoverableError) {
  TableAdmin tested(client_, "the-instance");

  EXPECT_CALL(*client_, GetIamPolicy)
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "err!")));

  std::string resource = "other-resource";

  EXPECT_FALSE(tested.GetIamPolicy(resource));
}

/// @test Verify recoverable errors for TableAdmin::GetIamPolicy.
TEST_F(TableAdminTest, GetIamPolicyRecoverableError) {
  namespace iamproto = ::google::iam::v1;

  TableAdmin tested(client_, "the-instance");

  auto mock_recoverable_failure = [](grpc::ClientContext* context,
                                     iamproto::GetIamPolicyRequest const&,
                                     iamproto::Policy*) {
    EXPECT_STATUS_OK(IsContextMDValid(
        *context, "google.bigtable.admin.v2.BigtableTableAdmin.GetIamPolicy",
        google::cloud::internal::ApiClientHeader()));
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
  };
  auto mock_policy = create_get_policy_mock();

  EXPECT_CALL(*client_, GetIamPolicy)
      .WillOnce(mock_recoverable_failure)
      .WillOnce(mock_policy);

  std::string resource = "test-resource";
  auto policy = tested.GetIamPolicy(resource);
  ASSERT_STATUS_OK(policy);
  EXPECT_EQ(3, policy->version());
  EXPECT_EQ("random-tag", policy->etag());
}

/// @test Verify positive scenario for TableAdmin::SetIamPolicy.
TEST_F(TableAdminTest, SetIamPolicy) {
  TableAdmin tested(client_, "the-instance");
  auto mock_policy = create_policy_with_params();
  EXPECT_CALL(*client_, SetIamPolicy).WillOnce(mock_policy);

  std::string resource = "test-resource";
  auto iam_policy =
      IamPolicy({IamBinding("writer", {"abc@gmail.com", "xyz@gmail.com"})},
                "test-tag", 0);
  auto policy = tested.SetIamPolicy(resource, iam_policy);
  ASSERT_STATUS_OK(policy);

  EXPECT_EQ(1, policy->bindings().size());
  EXPECT_EQ("test-tag", policy->etag());
}

/// @test Verify positive scenario for TableAdmin::SetIamPolicy.
TEST_F(TableAdminTest, SetIamPolicyForBackup) {
  TableAdmin tested(client_, "the-instance");
  auto mock_policy = create_policy_with_params_for_backup("the-backup");
  EXPECT_CALL(*client_, SetIamPolicy).WillOnce(mock_policy);

  std::string resource = "test-resource";
  auto iam_policy =
      IamPolicy({IamBinding("writer", {"abc@gmail.com", "xyz@gmail.com"})},
                "test-tag", 0);
  auto policy = tested.SetIamPolicy("the-cluster", resource, iam_policy);
  ASSERT_STATUS_OK(policy);

  EXPECT_EQ(1, policy->bindings().size());
  EXPECT_EQ("test-tag", policy->etag());
}

/// @test Verify unrecoverable errors for TableAdmin::SetIamPolicy.
TEST_F(TableAdminTest, SetIamPolicyUnrecoverableError) {
  TableAdmin tested(client_, "the-instance");

  EXPECT_CALL(*client_, SetIamPolicy)
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "err!")));

  std::string resource = "test-resource";
  auto iam_policy =
      IamPolicy({IamBinding("writer", {"abc@gmail.com", "xyz@gmail.com"})},
                "test-tag", 0);
  EXPECT_FALSE(tested.SetIamPolicy(resource, iam_policy));
}

/// @test Verify recoverable errors for TableAdmin::SetIamPolicy.
TEST_F(TableAdminTest, SetIamPolicyRecoverableError) {
  namespace iamproto = ::google::iam::v1;

  TableAdmin tested(client_, "the-instance");

  auto mock_recoverable_failure = [](grpc::ClientContext* context,
                                     iamproto::SetIamPolicyRequest const&,
                                     iamproto::Policy*) {
    EXPECT_STATUS_OK(IsContextMDValid(
        *context, "google.bigtable.admin.v2.BigtableTableAdmin.SetIamPolicy",
        google::cloud::internal::ApiClientHeader()));
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
  };
  auto mock_policy = create_policy_with_params();

  EXPECT_CALL(*client_, SetIamPolicy)
      .WillOnce(mock_recoverable_failure)
      .WillOnce(mock_policy);

  std::string resource = "test-resource";
  auto iam_policy =
      IamPolicy({IamBinding("writer", {"abc@gmail.com", "xyz@gmail.com"})},
                "test-tag", 0);
  auto policy = tested.SetIamPolicy(resource, iam_policy);
  ASSERT_STATUS_OK(policy);

  EXPECT_EQ(1, policy->bindings().size());
  EXPECT_EQ("test-tag", policy->etag());
}

/// @test Verify that TableAdmin::TestIamPermissions works in simple case.
TEST_F(TableAdminTest, TestIamPermissions) {
  namespace iamproto = ::google::iam::v1;
  TableAdmin tested(client_, "the-instance");

  auto mock_permission_set =
      [](grpc::ClientContext* context,
         iamproto::TestIamPermissionsRequest const&,
         iamproto::TestIamPermissionsResponse* response) {
        EXPECT_STATUS_OK(IsContextMDValid(
            *context,
            "google.bigtable.admin.v2.BigtableTableAdmin.TestIamPermissions",
            google::cloud::internal::ApiClientHeader()));
        EXPECT_NE(nullptr, response);
        std::vector<std::string> permissions = {"writer", "reader"};
        response->add_permissions(permissions[0]);
        response->add_permissions(permissions[1]);
        return grpc::Status::OK;
      };

  EXPECT_CALL(*client_, TestIamPermissions).WillOnce(mock_permission_set);

  std::string resource = "the-resource";
  auto permission_set =
      tested.TestIamPermissions(resource, {"reader", "writer", "owner"});
  ASSERT_STATUS_OK(permission_set);

  EXPECT_EQ(2, permission_set->size());
}

TEST_F(TableAdminTest, TestIamPermissionsForBackup) {
  namespace iamproto = ::google::iam::v1;
  TableAdmin tested(client_, "the-instance");
  std::string const& backup_id = "the-backup";

  auto mock_permission_set =
      [backup_id](grpc::ClientContext* context,
                  iamproto::TestIamPermissionsRequest const&,
                  iamproto::TestIamPermissionsResponse* response) {
        EXPECT_STATUS_OK(IsContextMDValid(
            *context,
            "google.bigtable.admin.v2.BigtableTableAdmin.TestIamPermissions",
            google::cloud::internal::ApiClientHeader(), backup_id));
        EXPECT_NE(nullptr, response);
        std::vector<std::string> permissions = {"writer", "reader"};
        response->add_permissions(permissions[0]);
        response->add_permissions(permissions[1]);
        return grpc::Status::OK;
      };

  EXPECT_CALL(*client_, TestIamPermissions).WillOnce(mock_permission_set);

  std::string resource = "the-resource";
  auto permission_set = tested.TestIamPermissions(
      "the-cluster", resource, {"reader", "writer", "owner"});
  ASSERT_STATUS_OK(permission_set);

  EXPECT_EQ(2, permission_set->size());
}

/// @test Test for unrecoverable errors for TableAdmin::TestIamPermissions.
TEST_F(TableAdminTest, TestIamPermissionsUnrecoverableError) {
  TableAdmin tested(client_, "the-instance");

  EXPECT_CALL(*client_, TestIamPermissions)
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "err!")));

  std::string resource = "other-resource";

  EXPECT_FALSE(
      tested.TestIamPermissions(resource, {"reader", "writer", "owner"}));
}

/// @test Test for recoverable errors for TableAdmin::TestIamPermissions.
TEST_F(TableAdminTest, TestIamPermissionsRecoverableError) {
  namespace iamproto = ::google::iam::v1;
  TableAdmin tested(client_, "the-instance");

  auto mock_recoverable_failure = [](grpc::ClientContext* context,
                                     iamproto::TestIamPermissionsRequest const&,
                                     iamproto::TestIamPermissionsResponse*) {
    EXPECT_STATUS_OK(IsContextMDValid(
        *context,
        "google.bigtable.admin.v2.BigtableTableAdmin.TestIamPermissions",
        google::cloud::internal::ApiClientHeader()));
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
  };

  auto mock_permission_set =
      [](grpc::ClientContext* context,
         iamproto::TestIamPermissionsRequest const&,
         iamproto::TestIamPermissionsResponse* response) {
        EXPECT_STATUS_OK(IsContextMDValid(
            *context,
            "google.bigtable.admin.v2.BigtableTableAdmin.TestIamPermissions",
            google::cloud::internal::ApiClientHeader()));
        EXPECT_NE(nullptr, response);
        std::vector<std::string> permissions = {"writer", "reader"};
        response->add_permissions(permissions[0]);
        response->add_permissions(permissions[1]);
        return grpc::Status::OK;
      };
  EXPECT_CALL(*client_, TestIamPermissions)
      .WillOnce(mock_recoverable_failure)
      .WillOnce(mock_permission_set);

  std::string resource = "the-resource";
  auto permission_set =
      tested.TestIamPermissions(resource, {"writer", "reader", "owner"});
  ASSERT_STATUS_OK(permission_set);

  EXPECT_EQ(2, permission_set->size());
}

/**
 * @test Verify that `bigtagble::TableAdmin::AsyncWaitForConsistencyImpl` works
 * as expected, with multiple asynchronous calls.
 */
TEST_F(TableAdminTest, AsyncWaitForConsistencySimple) {
  TableAdminTester table_admin;
  table_admin.SetTableAdmin(
      absl::make_unique<TableAdmin>(client_, "test-instance"));

  auto r1 = absl::make_unique<MockAsyncCheckConsistencyResponse>();
  EXPECT_CALL(*r1, Finish)
      .WillOnce([](btadmin::CheckConsistencyResponse* response,
                   grpc::Status* status, void*) {
        ASSERT_NE(nullptr, response);
        *status = grpc::Status(grpc::StatusCode::UNAVAILABLE, "try again");
      });
  auto r2 = absl::make_unique<MockAsyncCheckConsistencyResponse>();
  EXPECT_CALL(*r2, Finish)
      .WillOnce([](btadmin::CheckConsistencyResponse* response,
                   grpc::Status* status, void*) {
        ASSERT_NE(nullptr, response);
        response->set_consistent(false);
        *status = grpc::Status::OK;
      });
  auto r3 = absl::make_unique<MockAsyncCheckConsistencyResponse>();
  EXPECT_CALL(*r3, Finish)
      .WillOnce([](btadmin::CheckConsistencyResponse* response,
                   grpc::Status* status, void*) {
        ASSERT_NE(nullptr, response);
        response->set_consistent(true);
        *status = grpc::Status::OK;
      });

  auto make_invoke = [](std::unique_ptr<MockAsyncCheckConsistencyResponse>& r) {
    return [&r](grpc::ClientContext* context,
                btadmin::CheckConsistencyRequest const& request,
                grpc::CompletionQueue*) {
      EXPECT_STATUS_OK(IsContextMDValid(
          *context,
          "google.bigtable.admin.v2.BigtableTableAdmin.CheckConsistency",
          google::cloud::internal::ApiClientHeader()));
      EXPECT_EQ(
          "projects/the-project/instances/test-instance/tables/test-table",
          request.name());
      // This is safe, see comments in MockAsyncResponseReader.
      return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
          btadmin::CheckConsistencyResponse>>(r.get());
    };
  };

  EXPECT_CALL(*client_, project()).WillRepeatedly(ReturnRef(kProjectId));
  EXPECT_CALL(*client_, AsyncCheckConsistency)
      .WillOnce(make_invoke(r1))
      .WillOnce(make_invoke(r2))
      .WillOnce(make_invoke(r3));

  std::shared_ptr<FakeCompletionQueueImpl> cq_impl(new FakeCompletionQueueImpl);
  CompletionQueue cq(cq_impl);

  google::cloud::future<google::cloud::StatusOr<Consistency>> result =
      table_admin.AsyncWaitForConsistencyImpl(cq, "test-table",
                                              "test-async-token");

  // The future is not ready yet.
  auto future_status = result.wait_for(0_ms);
  EXPECT_EQ(std::future_status::timeout, future_status);

  // Simulate the completions for each event.

  // AsyncCheckConsistency() -> TRANSIENT
  cq_impl->SimulateCompletion(true);
  future_status = result.wait_for(0_ms);
  EXPECT_EQ(std::future_status::timeout, future_status);

  // timer
  cq_impl->SimulateCompletion(true);
  future_status = result.wait_for(0_ms);
  EXPECT_EQ(std::future_status::timeout, future_status);

  // AsyncCheckConsistency() -> !consistent
  cq_impl->SimulateCompletion(true);
  future_status = result.wait_for(0_ms);
  EXPECT_EQ(std::future_status::timeout, future_status);

  // timer
  cq_impl->SimulateCompletion(true);
  future_status = result.wait_for(0_ms);
  EXPECT_EQ(std::future_status::timeout, future_status);

  // AsyncCheckConsistency() -> consistent
  cq_impl->SimulateCompletion(true);
  future_status = result.wait_for(0_ms);
  ASSERT_EQ(std::future_status::ready, future_status);

  // The future becomes ready on the first request that completes with a
  // permanent error.
  auto consistent = result.get();
  ASSERT_STATUS_OK(consistent);

  EXPECT_EQ(Consistency::kConsistent, *consistent);
}

/**
 * @test Verify that `bigtable::TableAdmin::AsyncWaitForConsistencyImpl` makes
 * only one RPC attempt and reports errors on failure.
 */
TEST_F(TableAdminTest, AsyncWaitForConsistencyFailure) {
  TableAdminTester table_admin;
  table_admin.SetTableAdmin(
      absl::make_unique<TableAdmin>(client_, "test-instance"));
  auto reader = absl::make_unique<MockAsyncCheckConsistencyResponse>();
  EXPECT_CALL(*reader, Finish)
      .WillOnce([](btadmin::CheckConsistencyResponse* response,
                   grpc::Status* status, void*) {
        ASSERT_NE(nullptr, response);
        *status = grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "oh no");
      });
  EXPECT_CALL(*client_, project()).WillRepeatedly(ReturnRef(kProjectId));
  EXPECT_CALL(*client_, AsyncCheckConsistency)
      .WillOnce([&](grpc::ClientContext* context,
                    btadmin::CheckConsistencyRequest const& request,
                    grpc::CompletionQueue*) {
        EXPECT_STATUS_OK(IsContextMDValid(
            *context,
            "google.bigtable.admin.v2.BigtableTableAdmin.CheckConsistency",
            google::cloud::internal::ApiClientHeader()));
        EXPECT_EQ(
            "projects/the-project/instances/test-instance/tables/test-table",
            request.name());
        // This is safe, see comments in MockAsyncResponseReader.
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            btadmin::CheckConsistencyResponse>>(reader.get());
      });

  std::shared_ptr<FakeCompletionQueueImpl> cq_impl(new FakeCompletionQueueImpl);
  CompletionQueue cq(cq_impl);

  google::cloud::future<google::cloud::StatusOr<Consistency>> result =
      table_admin.AsyncWaitForConsistencyImpl(cq, "test-table",
                                              "test-async-token");

  // The future is not ready yet.
  auto future_status = result.wait_for(0_ms);
  EXPECT_EQ(std::future_status::timeout, future_status);
  cq_impl->SimulateCompletion(true);

  // The future becomes ready on the first request that completes with a
  // permanent error.
  future_status = result.wait_for(0_ms);
  ASSERT_EQ(std::future_status::ready, future_status);

  auto consistent = result.get();
  EXPECT_THAT(consistent, StatusIs(StatusCode::kPermissionDenied));
}

class ValidContextMdAsyncTest : public ::testing::Test {
 public:
  ValidContextMdAsyncTest()
      : cq_impl_(new FakeCompletionQueueImpl),
        cq_(cq_impl_),
        client_(new MockAdminClient) {
    EXPECT_CALL(*client_, project())
        .WillRepeatedly(::testing::ReturnRef(kProjectId));
    table_admin_.SetTableAdmin(
        absl::make_unique<TableAdmin>(client_, kInstanceId));
  }

 protected:
  template <typename ResultType>
  void FinishTest(
      google::cloud::future<google::cloud::StatusOr<ResultType>> res_future) {
    EXPECT_EQ(1U, cq_impl_->size());
    cq_impl_->SimulateCompletion(true);
    EXPECT_EQ(0U, cq_impl_->size());
    auto res = res_future.get();
    EXPECT_THAT(res, StatusIs(StatusCode::kPermissionDenied));
  }

  std::shared_ptr<FakeCompletionQueueImpl> cq_impl_;
  CompletionQueue cq_;
  std::shared_ptr<MockAdminClient> client_;
  TableAdminTester table_admin_;
};

TEST_F(ValidContextMdAsyncTest, AsyncCreateBackup) {
  ::google::cloud::bigtable::testing::MockAsyncFailingRpcFactory<
      btadmin::CreateBackupRequest, google::longrunning::Operation>
      rpc_factory;
  EXPECT_CALL(*client_, AsyncCreateBackup)
      .WillOnce(rpc_factory.Create(
          R"pb(
            parent: "projects/the-project/instances/the-instance/clusters/the-cluster"
            backup_id: "the-backup"
            backup: {
              source_table: "projects/the-project/instances/the-instance/tables/the-table"
              expire_time: { seconds: 1893387600 }
            }
          )pb",
          "google.bigtable.admin.v2.BigtableTableAdmin.CreateBackup"));
  google::protobuf::Timestamp expire_time;
  EXPECT_TRUE(google::protobuf::util::TimeUtil::FromString(
      "2029-12-31T00:00:00.000-05:00", &expire_time));
  TableAdmin::CreateBackupParams backup_config(
      "the-cluster", "the-backup", "the-table",
      google::cloud::internal::ToChronoTimePoint(expire_time));
  FinishTest(table_admin_.AsyncCreateBackupImpl(cq_, backup_config));
}

TEST_F(ValidContextMdAsyncTest, AsyncRestoreTable) {
  ::google::cloud::bigtable::testing::MockAsyncFailingRpcFactory<
      btadmin::RestoreTableRequest, google::longrunning::Operation>
      rpc_factory;
  EXPECT_CALL(*client_, AsyncRestoreTable)
      .WillOnce(rpc_factory.Create(
          R"pb(
            parent: "projects/the-project/instances/the-instance"
            table_id: "restored-table"
            backup: "projects/the-project/instances/the-instance/clusters/the-cluster/backups/the-backup"
          )pb",
          "google.bigtable.admin.v2.BigtableTableAdmin.RestoreTable"));
  bigtable::TableAdmin::RestoreTableParams params("restored-table",
                                                  "the-cluster", "the-backup");
  FinishTest(table_admin_.AsyncRestoreTableImpl(cq_, std::move(params)));
}

}  // namespace
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
