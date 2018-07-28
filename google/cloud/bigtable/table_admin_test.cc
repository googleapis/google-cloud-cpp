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
#include "google/cloud/bigtable/grpc_error.h"
#include "google/cloud/bigtable/internal/make_unique.h"
#include "google/cloud/bigtable/testing/chrono_literals.h"
#include "google/cloud/bigtable/testing/mock_admin_client.h"
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <gmock/gmock.h>

namespace {
namespace btadmin = ::google::bigtable::admin::v2;
namespace bigtable = google::cloud::bigtable;

using MockAdminClient = bigtable::testing::MockAdminClient;

std::string const kProjectId = "the-project";
std::string const kInstanceId = "the-instance";
std::string const kClusterId = "the-cluster";

/// A fixture for the bigtable::TableAdmin tests.
class TableAdminTest : public ::testing::Test {
 protected:
  void SetUp() override {
    using namespace ::testing;

    EXPECT_CALL(*client_, project()).WillRepeatedly(ReturnRef(kProjectId));
  }

  std::shared_ptr<MockAdminClient> client_ =
      std::make_shared<MockAdminClient>();
};

// A lambda to create lambdas.  Basically we would be rewriting the same
// lambda twice without this thing.
auto create_list_tables_lambda = [](std::string expected_token,
                                    std::string returned_token,
                                    std::vector<std::string> table_names) {
  return
      [expected_token, returned_token, table_names](
          grpc::ClientContext* ctx, btadmin::ListTablesRequest const& request,
          btadmin::ListTablesResponse* response) {
        auto const instance_name =
            "projects/" + kProjectId + "/instances/" + kInstanceId;
        EXPECT_EQ(instance_name, request.parent());
        EXPECT_EQ(btadmin::Table::FULL, request.view());
        EXPECT_EQ(expected_token, request.page_token());

        EXPECT_NE(nullptr, response);
        for (auto const& table_name : table_names) {
          auto& table = *response->add_tables();
          table.set_name(instance_name + "/tables/" + table_name);
          table.set_granularity(btadmin::Table::MILLIS);
        }
        // Return the right token.
        response->set_next_page_token(returned_token);
        return grpc::Status::OK;
      };
};

// A lambda to generate snapshot list.
auto create_list_snapshots_lambda =
    [](std::string expected_token, std::string returned_token,
       std::vector<std::string> snapshot_names) {
      return [expected_token, returned_token, snapshot_names](
                 grpc::ClientContext* ctx,
                 btadmin::ListSnapshotsRequest const& request,
                 btadmin::ListSnapshotsResponse* response) {
        auto cluster_name =
            "projects/" + kProjectId + "/instances/" + kInstanceId;
        cluster_name += "/clusters/" + kClusterId;
        EXPECT_EQ(cluster_name, request.parent());
        EXPECT_EQ(expected_token, request.page_token());

        EXPECT_NE(nullptr, response);
        for (auto const& snapshot_name : snapshot_names) {
          auto& snapshot = *response->add_snapshots();
          snapshot.set_name(cluster_name + "/snapshots/" + snapshot_name);
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
  static std::function<SignatureType> Create(std::string expected_request) {
    return std::function<SignatureType>(
        [expected_request](grpc::ClientContext* ctx, RequestType const& request,
                           ResponseType* response) {
          RequestType expected;
          // Cannot use ASSERT_TRUE() here, it has an embedded "return;"
          EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(
              expected_request, &expected));
          std::string delta;
          google::protobuf::util::MessageDifferencer differencer;
          differencer.ReportDifferencesToString(&delta);
          EXPECT_TRUE(differencer.Compare(expected, request)) << delta;

          EXPECT_NE(nullptr, response);
          return grpc::Status::OK;
        });
  }
};

/**
 * Helper class to create the expectations and check consistency over
 * multiple calls for a simple RPC call.
 *
 * Given the type of the request and responses, this struct provides a function
 * to create a mock implementation with the right signature and checks.
 *
 * @tparam RequestType the protobuf type for the request.
 * @tparam ResponseType the protobuf type for the response.
 */
template <typename RequestType, typename ResponseType>
struct MockRpcMultiCallFactory {
  using SignatureType = grpc::Status(grpc::ClientContext* ctx,
                                     RequestType const& request,
                                     ResponseType* response);

  /// Refactor the boilerplate common to most tests.
  static std::function<SignatureType> Create(std::string expected_request,
                                             bool expected_result) {
    return std::function<SignatureType>(
        [expected_request, expected_result](grpc::ClientContext* ctx,
                                            RequestType const& request,
                                            ResponseType* response) {
          RequestType expected;
          response->clear_consistent();
          // Cannot use ASSERT_TRUE() here, it has an embedded "return;"
          EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(
              expected_request, &expected));
          std::string delta;
          google::protobuf::util::MessageDifferencer differencer;
          differencer.ReportDifferencesToString(&delta);
          EXPECT_TRUE(differencer.Compare(expected, request)) << delta;

          if (response != nullptr) {
            response->set_consistent(expected_result);
          }

          EXPECT_NE(nullptr, response);
          return grpc::Status::OK;
        });
  }
};

}  // anonymous namespace

/// @test Verify basic functionality in the `bigtable::TableAdmin` class.
TEST_F(TableAdminTest, Default) {
  using namespace ::testing;

  bigtable::TableAdmin tested(client_, "the-instance");
  EXPECT_EQ("the-instance", tested.instance_id());
  EXPECT_EQ("projects/the-project/instances/the-instance",
            tested.instance_name());
}

/// @test Verify that `bigtable::TableAdmin::ListTables` works in the easy case.
TEST_F(TableAdminTest, ListTables) {
  using namespace ::testing;

  bigtable::TableAdmin tested(client_, kInstanceId);
  auto mock_list_tables = create_list_tables_lambda("", "", {"t0", "t1"});
  EXPECT_CALL(*client_, ListTables(_, _, _)).WillOnce(Invoke(mock_list_tables));

  // After all the setup, make the actual call we want to test.
  auto actual = tested.ListTables(btadmin::Table::FULL);
  std::string instance_name = tested.instance_name();
  ASSERT_EQ(2UL, actual.size());
  EXPECT_EQ(instance_name + "/tables/t0", actual[0].name());
  EXPECT_EQ(instance_name + "/tables/t1", actual[1].name());
}

/// @test Verify that `bigtable::TableAdmin::ListTables` handles failures.
TEST_F(TableAdminTest, ListTablesRecoverableFailures) {
  using namespace ::testing;

  bigtable::TableAdmin tested(client_, "the-instance");
  auto mock_recoverable_failure = [](grpc::ClientContext* ctx,
                                     btadmin::ListTablesRequest const& request,
                                     btadmin::ListTablesResponse* response) {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
  };
  auto batch0 = create_list_tables_lambda("", "token-001", {"t0", "t1"});
  auto batch1 = create_list_tables_lambda("token-001", "", {"t2", "t3"});
  EXPECT_CALL(*client_, ListTables(_, _, _))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(batch0))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(batch1));

  // After all the setup, make the actual call we want to test.
  auto actual = tested.ListTables(btadmin::Table::FULL);
  std::string instance_name = tested.instance_name();
  ASSERT_EQ(4UL, actual.size());
  EXPECT_EQ(instance_name + "/tables/t0", actual[0].name());
  EXPECT_EQ(instance_name + "/tables/t1", actual[1].name());
  EXPECT_EQ(instance_name + "/tables/t2", actual[2].name());
  EXPECT_EQ(instance_name + "/tables/t3", actual[3].name());
}

/**
 * @test Verify that `bigtable::TableAdmin::ListTables` handles unrecoverable
 * failures.
 */
TEST_F(TableAdminTest, ListTablesUnrecoverableFailures) {
  using namespace ::testing;

  bigtable::TableAdmin tested(client_, "the-instance");
  EXPECT_CALL(*client_, ListTables(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

// After all the setup, make the actual call we want to test.
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(tested.ListTables(btadmin::Table::FULL), std::exception);
#else
  EXPECT_DEATH_IF_SUPPORTED(tested.ListTables(btadmin::Table::FULL),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/**
 * @test Verify that `bigtable::TableAdmin::ListTables` handles too many
 * recoverable failures.
 */
TEST_F(TableAdminTest, ListTablesTooManyFailures) {
  using namespace ::testing;
  using namespace bigtable::chrono_literals;

  bigtable::TableAdmin tested(
      client_, "the-instance", bigtable::LimitedErrorCountRetryPolicy(3),
      bigtable::ExponentialBackoffPolicy(10_ms, 10_min));
  auto mock_recoverable_failure = [](grpc::ClientContext* ctx,
                                     btadmin::ListTablesRequest const& request,
                                     btadmin::ListTablesResponse* response) {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
  };
  EXPECT_CALL(*client_, ListTables(_, _, _))
      .WillRepeatedly(Invoke(mock_recoverable_failure));

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  // After all the setup, make the actual call we want to test.
  EXPECT_THROW(tested.ListTables(btadmin::Table::FULL), std::exception);
#else
  EXPECT_DEATH_IF_SUPPORTED(tested.ListTables(btadmin::Table::FULL),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify that `bigtable::TableAdmin::Create` works in the easy case.
TEST_F(TableAdminTest, CreateTableSimple) {
  using namespace ::testing;
  using namespace bigtable::chrono_literals;

  bigtable::TableAdmin tested(client_, "the-instance");

  std::string expected_text = R"""(
      parent: 'projects/the-project/instances/the-instance'
table_id: 'new-table'
table {
  column_families {
    key: 'f1'
    value { gc_rule { max_num_versions: 1 }}
  }
  column_families {
    key: 'f2'
    value { gc_rule { max_age { seconds: 1 }}}
  }
  granularity: TIMESTAMP_GRANULARITY_UNSPECIFIED
}
initial_splits { key: 'a' }
initial_splits { key: 'c' }
initial_splits { key: 'p' }
)""";
  auto mock_create_table =
      MockRpcFactory<btadmin::CreateTableRequest, btadmin::Table>::Create(
          expected_text);
  EXPECT_CALL(*client_, CreateTable(_, _, _))
      .WillOnce(Invoke(mock_create_table));

  // After all the setup, make the actual call we want to test.
  using GC = bigtable::GcRule;
  bigtable::TableConfig config(
      {{"f1", GC::MaxNumVersions(1)}, {"f2", GC::MaxAge(1_s)}},
      {"a", "c", "p"});
  tested.CreateTable("new-table", std::move(config));
}

/**
 * @test Verify that `bigtable::TableAdmin::CreateTable` supports
 * only one try and let client know request status.
 */
TEST_F(TableAdminTest, CreateTableFailure) {
  using namespace ::testing;

  bigtable::TableAdmin tested(client_, "the-instance");
  EXPECT_CALL(*client_, CreateTable(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  // After all the setup, make the actual call we want to test.
  EXPECT_THROW(tested.CreateTable("other-table", bigtable::TableConfig()),
               bigtable::GRpcError);
#else
  EXPECT_DEATH_IF_SUPPORTED(
      tested.CreateTable("other-table", bigtable::TableConfig()),
      "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/**
 * @test Verify that Copy Constructor and assignment operator
 * copies all properties.
 */
TEST_F(TableAdminTest, CopyConstructibleAssignableTest) {
  using namespace ::testing;

  bigtable::TableAdmin tested(client_, "the-copy-instance");
  bigtable::TableAdmin table_admin(tested);

  EXPECT_EQ(tested.instance_id(), table_admin.instance_id());
  EXPECT_EQ(tested.instance_name(), table_admin.instance_name());
  EXPECT_EQ(tested.project(), table_admin.project());

  bigtable::TableAdmin table_admin_assign(client_, "the-assign-instance");
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
  using namespace ::testing;
  using namespace bigtable::chrono_literals;

  bigtable::TableAdmin tested(
      client_, "the-construct-instance",
      bigtable::LimitedErrorCountRetryPolicy(3),
      bigtable::ExponentialBackoffPolicy(10_ms, 10_min));
  // Copy Constructor
  bigtable::TableAdmin table_admin(tested);
  // Create New Instance
  bigtable::TableAdmin table_admin_assign(client_, "the-assign-instance");
  // Copy assignable
  table_admin_assign = table_admin;

  EXPECT_CALL(*client_, GetTable(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")));

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  // After all the setup, make the actual call we want to test.
  EXPECT_THROW(table_admin.GetTable("other-table"), bigtable::GRpcError);
  EXPECT_THROW(table_admin_assign.GetTable("other-table"), bigtable::GRpcError);
#else
  EXPECT_DEATH_IF_SUPPORTED(table_admin.GetTable("other-table"),
                            "exceptions are disabled");
  EXPECT_DEATH_IF_SUPPORTED(table_admin_assign.GetTable("other-table"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify that `bigtable::TableAdmin::GetTable` works in the easy case.
TEST_F(TableAdminTest, GetTableSimple) {
  using namespace ::testing;
  using namespace bigtable::chrono_literals;

  bigtable::TableAdmin tested(client_, "the-instance");
  std::string expected_text = R"""(
      name: 'projects/the-project/instances/the-instance/tables/the-table'
      view: SCHEMA_VIEW
)""";
  auto mock = MockRpcFactory<btadmin::GetTableRequest, btadmin::Table>::Create(
      expected_text);
  EXPECT_CALL(*client_, GetTable(_, _, _))
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")))
      .WillOnce(Invoke(mock));

  // After all the setup, make the actual call we want to test.
  tested.GetTable("the-table");
}

/**
 * @test Verify that `bigtable::TableAdmin::GetTable` reports unrecoverable
 * failures.
 */
TEST_F(TableAdminTest, GetTableUnrecoverableFailures) {
  using namespace ::testing;
  using namespace bigtable::chrono_literals;

  bigtable::TableAdmin tested(client_, "the-instance");
  EXPECT_CALL(*client_, GetTable(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::NOT_FOUND, "uh oh")));

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  // After all the setup, make the actual call we want to test.
  EXPECT_THROW(tested.GetTable("other-table"), bigtable::GRpcError);
#else
  EXPECT_DEATH_IF_SUPPORTED(tested.GetTable("other-table"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/**
 * @test Verify that `bigtable::TableAdmin::GetTable` works with too many
 * recoverable failures.
 */
TEST_F(TableAdminTest, GetTableTooManyFailures) {
  using namespace ::testing;
  using namespace bigtable::chrono_literals;

  bigtable::TableAdmin tested(
      client_, "the-instance", bigtable::LimitedErrorCountRetryPolicy(3),
      bigtable::ExponentialBackoffPolicy(10_ms, 10_min));
  EXPECT_CALL(*client_, GetTable(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")));

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  // After all the setup, make the actual call we want to test.
  EXPECT_THROW(tested.GetTable("other-table"), bigtable::GRpcError);
#else
  EXPECT_DEATH_IF_SUPPORTED(tested.GetTable("other-table"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify that bigtable::TableAdmin::DeleteTable works as expected.
TEST_F(TableAdminTest, DeleteTable) {
  using namespace ::testing;
  using google::protobuf::Empty;

  bigtable::TableAdmin tested(client_, "the-instance");
  std::string expected_text = R"""(
      name: 'projects/the-project/instances/the-instance/tables/the-table'
)""";
  auto mock =
      MockRpcFactory<btadmin::DeleteTableRequest, Empty>::Create(expected_text);
  EXPECT_CALL(*client_, DeleteTable(_, _, _)).WillOnce(Invoke(mock));

  // After all the setup, make the actual call we want to test.
  tested.DeleteTable("the-table");
}

/**
 * @test Verify that `bigtable::TableAdmin::DeleteTable` supports
 * only one try and let client know request status.
 */
TEST_F(TableAdminTest, DeleteTableFailure) {
  using namespace ::testing;

  bigtable::TableAdmin tested(client_, "the-instance");
  EXPECT_CALL(*client_, DeleteTable(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  // After all the setup, make the actual call we want to test.
  EXPECT_THROW(tested.DeleteTable("other-table"), bigtable::GRpcError);
#else
  EXPECT_DEATH_IF_SUPPORTED(tested.DeleteTable("other-table"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/**
 * @test Verify that bigtable::TableAdmin::ModifyColumnFamilies works as
 * expected.
 */
TEST_F(TableAdminTest, ModifyColumnFamilies) {
  using namespace ::testing;
  using namespace bigtable::chrono_literals;
  using google::protobuf::Empty;

  bigtable::TableAdmin tested(client_, "the-instance");
  std::string expected_text = R"""(
      name: 'projects/the-project/instances/the-instance/tables/the-table'
modifications {
  id: 'foo'
  create { gc_rule { max_age { seconds: 172800 }}}
}
modifications {
  id: 'bar'
  update { gc_rule { max_age { seconds: 86400 }}}
}
)""";
  auto mock = MockRpcFactory<btadmin::ModifyColumnFamiliesRequest,
                             btadmin::Table>::Create(expected_text);
  EXPECT_CALL(*client_, ModifyColumnFamilies(_, _, _)).WillOnce(Invoke(mock));

  // After all the setup, make the actual call we want to test.
  using M = bigtable::ColumnFamilyModification;
  using GC = bigtable::GcRule;
  auto actual = tested.ModifyColumnFamilies(
      "the-table",
      {M::Create("foo", GC::MaxAge(48_h)), M::Update("bar", GC::MaxAge(24_h))});
}

/**
 * @test Verify that `bigtable::TableAdmin::ModifyColumnFamilies` makes only one
 * RPC attempt and reports errors on failure.
 */
TEST_F(TableAdminTest, ModifyColumnFamiliesFailure) {
  using namespace ::testing;
  using namespace bigtable::chrono_literals;

  bigtable::TableAdmin tested(client_, "the-instance");
  EXPECT_CALL(*client_, ModifyColumnFamilies(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

  using M = bigtable::ColumnFamilyModification;
  using GC = bigtable::GcRule;
  std::vector<M> changes{M::Create("foo", GC::MaxAge(48_h)),
                         M::Update("bar", GC::MaxAge(24_h))};

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  // After all the setup, make the actual call we want to test.
  EXPECT_THROW(tested.ModifyColumnFamilies("other-table", std::move(changes)),
               bigtable::GRpcError);
#else
  EXPECT_DEATH_IF_SUPPORTED(
      tested.ModifyColumnFamilies("other-table", std::move(changes)),
      "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify that bigtable::TableAdmin::DropRowsByPrefix works as expected.
TEST_F(TableAdminTest, DropRowsByPrefix) {
  using namespace ::testing;
  using google::protobuf::Empty;

  bigtable::TableAdmin tested(client_, "the-instance");
  std::string expected_text = R"""(
      name: 'projects/the-project/instances/the-instance/tables/the-table'
      row_key_prefix: 'foobar'
)""";
  auto mock = MockRpcFactory<btadmin::DropRowRangeRequest, Empty>::Create(
      expected_text);
  EXPECT_CALL(*client_, DropRowRange(_, _, _)).WillOnce(Invoke(mock));

  // After all the setup, make the actual call we want to test.
  tested.DropRowsByPrefix("the-table", "foobar");
}

/**
 * @test Verify that `bigtable::TableAdmin::DropRowsByPrefix` makes only one
 * RPC attempt and reports errors on failure.
 */
TEST_F(TableAdminTest, DropRowsByPrefixFailure) {
  using namespace ::testing;

  bigtable::TableAdmin tested(client_, "the-instance");
  EXPECT_CALL(*client_, DropRowRange(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  // After all the setup, make the actual call we want to test.
  EXPECT_THROW(tested.DropRowsByPrefix("other-table", "prefix"),
               bigtable::GRpcError);
#else
  EXPECT_DEATH_IF_SUPPORTED(tested.DropRowsByPrefix("other-table", "prefix"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify that bigtable::TableAdmin::DropRowsByPrefix works as expected.
TEST_F(TableAdminTest, DropAllRows) {
  using namespace ::testing;
  using google::protobuf::Empty;

  bigtable::TableAdmin tested(client_, "the-instance");
  std::string expected_text = R"""(
      name: 'projects/the-project/instances/the-instance/tables/the-table'
      delete_all_data_from_table: true
)""";
  auto mock = MockRpcFactory<btadmin::DropRowRangeRequest, Empty>::Create(
      expected_text);
  EXPECT_CALL(*client_, DropRowRange(_, _, _)).WillOnce(Invoke(mock));

  // After all the setup, make the actual call we want to test.
  tested.DropAllRows("the-table");
}

/**
 * @test Verify that `bigtable::TableAdmin::DropAllRows` makes only one
 * RPC attempt and reports errors on failure.
 */
TEST_F(TableAdminTest, DropAllRowsFailure) {
  using namespace ::testing;

  bigtable::TableAdmin tested(client_, "the-instance");
  EXPECT_CALL(*client_, DropRowRange(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  // After all the setup, make the actual call we want to test.
  EXPECT_THROW(tested.DropAllRows("other-table"), bigtable::GRpcError);
#else
  EXPECT_DEATH_IF_SUPPORTED(tested.DropAllRows("other-table"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/**
 * @test Verify that `bigtagble::TableAdmin::GenerateConsistencyToken` works as
 * expected.
 */
TEST_F(TableAdminTest, GenerateConsistencyTokenSimple) {
  using namespace ::testing;

  bigtable::TableAdmin tested(client_, "the-instance");
  std::string expected_text = R"""(
      name: 'projects/the-project/instances/the-instance/tables/the-table'
)""";
  auto mock = MockRpcFactory<
      btadmin::GenerateConsistencyTokenRequest,
      btadmin::GenerateConsistencyTokenResponse>::Create(expected_text);
  EXPECT_CALL(*client_, GenerateConsistencyToken(_, _, _))
      .WillOnce(Invoke(mock));

  // After all the setup, make the actual call we want to test.
  tested.GenerateConsistencyToken("the-table");
}

/**
 * @test Verify that `bigtable::TableAdmin::GenerateConsistencyToken` makes only
 * one RPC attempt and reports errors on failure.
 */
TEST_F(TableAdminTest, GenerateConsistencyTokenFailure) {
  using namespace ::testing;

  bigtable::TableAdmin tested(client_, "the-instance");
  EXPECT_CALL(*client_, GenerateConsistencyToken(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  // After all the setup, make the actual call we want to test.
  EXPECT_THROW(tested.GenerateConsistencyToken("other-table"),
               bigtable::GRpcError);
#else
  EXPECT_DEATH_IF_SUPPORTED(tested.GenerateConsistencyToken("other-table"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/**
 * @test Verify that `bigtagble::TableAdmin::CheckConsistency` works as
 * expected.
 */
TEST_F(TableAdminTest, CheckConsistencySimple) {
  using namespace ::testing;

  bigtable::TableAdmin tested(client_, "the-instance");
  std::string expected_text = R"""(
      name: 'projects/the-project/instances/the-instance/tables/the-table'
      consistency_token: 'test-token'
)""";
  auto mock =
      MockRpcFactory<btadmin::CheckConsistencyRequest,
                     btadmin::CheckConsistencyResponse>::Create(expected_text);
  EXPECT_CALL(*client_, CheckConsistency(_, _, _)).WillOnce(Invoke(mock));

  bigtable::TableId table_id("the-table");
  bigtable::ConsistencyToken consistency_token("test-token");
  // After all the setup, make the actual call we want to test.
  tested.CheckConsistency(table_id, consistency_token);
}

/**
 * @test Verify that `bigtable::TableAdmin::CheckConsistency` makes only
 * one RPC attempt and reports errors on failure.
 */
TEST_F(TableAdminTest, CheckConsistencyFailure) {
  using namespace ::testing;

  bigtable::TableAdmin tested(client_, "the-instance");
  EXPECT_CALL(*client_, CheckConsistency(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

  bigtable::TableId table_id("other-table");
  bigtable::ConsistencyToken consistency_token("test-token");
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  // After all the setup, make the actual call we want to test.
  EXPECT_THROW(tested.CheckConsistency(table_id, consistency_token),
               bigtable::GRpcError);
#else
  EXPECT_DEATH_IF_SUPPORTED(
      tested.CheckConsistency(table_id, consistency_token),
      "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/**
 * @test Verify that `bigtagble::TableAdmin::CheckConsistency` works as
 * expected, with multiple asynchronous calls.
 */
TEST_F(TableAdminTest, AsyncCheckConsistencySimple) {
  using namespace ::testing;
  using namespace bigtable::chrono_literals;

  bigtable::TableAdmin tested(client_, "the-async-instance");
  std::string expected_text = R"""(
      name: 'projects/the-project/instances/the-async-instance/tables/the-async-table'
      consistency_token: 'test-async-token'
)""";

  auto mock_for_false = MockRpcMultiCallFactory<
      btadmin::CheckConsistencyRequest,
      btadmin::CheckConsistencyResponse>::Create(expected_text, false);
  auto mock_for_true = MockRpcMultiCallFactory<
      btadmin::CheckConsistencyRequest,
      btadmin::CheckConsistencyResponse>::Create(expected_text, true);

  EXPECT_CALL(*client_, CheckConsistency(_, _, _))
      .WillOnce(Invoke(mock_for_false))
      .WillOnce(Invoke(mock_for_false))
      .WillOnce(Invoke(mock_for_false))
      .WillOnce(Invoke(mock_for_false))
      .WillOnce(Invoke(mock_for_true));

  bigtable::TableId table_id("the-async-table");
  bigtable::ConsistencyToken consistency_token("test-async-token");
  // After all the setup, make the actual call we want to test.
  std::future<bool> result =
      tested.WaitForConsistencyCheck(table_id, consistency_token);
  EXPECT_TRUE(result.get());
}

/**
 * @test Verify that `bigtable::TableAdmin::CheckConsistency` makes only
 * one RPC attempt and reports errors on failure.
 */
TEST_F(TableAdminTest, AsyncCheckConsistencyFailure) {
  using namespace ::testing;
  using namespace bigtable::chrono_literals;

  bigtable::TableAdmin tested(client_, "the-async-instance");
  EXPECT_CALL(*client_, CheckConsistency(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

  bigtable::TableId table_id("other-async-table");
  bigtable::ConsistencyToken consistency_token("test-async-token");

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  // After all the setup, make the actual call we want to test.
  std::future<bool> result =
      tested.WaitForConsistencyCheck(table_id, consistency_token);
  EXPECT_THROW(result.get(), bigtable::GRpcError);
#else
  EXPECT_DEATH_IF_SUPPORTED(
      tested.WaitForConsistencyCheck(table_id, consistency_token),
      "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/**
 * @test Verify that `bigtable::TableAdmin::GetSnapshot` works in the easy case.
 */
TEST_F(TableAdminTest, GetSnapshotSimple) {
  using namespace ::testing;
  using namespace bigtable::chrono_literals;

  bigtable::TableAdmin tested(client_, "the-instance");
  std::string expected_text = R"""(
      name: 'projects/the-project/instances/the-instance/clusters/the-cluster/snapshots/random-snapshot'
)""";
  auto mock =
      MockRpcFactory<btadmin::GetSnapshotRequest, btadmin::Snapshot>::Create(
          expected_text);
  EXPECT_CALL(*client_, GetSnapshot(_, _, _))
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")))
      .WillOnce(Invoke(mock));
  bigtable::ClusterId cluster_id("the-cluster");
  bigtable::SnapshotId snapshot_id("random-snapshot");
  tested.GetSnapshot(cluster_id, snapshot_id);
}

/**
 * @test Verify that `bigtable::TableAdmin::GetSnapshot` reports unrecoverable
 * failures.
 */
TEST_F(TableAdminTest, GetSnapshotUnrecoverableFailures) {
  using namespace ::testing;
  using namespace bigtable::chrono_literals;

  bigtable::TableAdmin tested(client_, "the-instance");
  EXPECT_CALL(*client_, GetSnapshot(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::NOT_FOUND, "No snapshot.")));
  bigtable::ClusterId cluster_id("other-cluster");
  bigtable::SnapshotId snapshot_id("other-snapshot");
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  // After all the setup, make the actual call we want to test.
  EXPECT_THROW(tested.GetSnapshot(cluster_id, snapshot_id),
               bigtable::GRpcError);
#else
  EXPECT_DEATH_IF_SUPPORTED(tested.GetSnapshot(cluster_id, snapshot_id),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/**
 * @test Verify that `bigtable::TableAdmin::GetSnapshot` works with too many
 * recoverable failures.
 */
TEST_F(TableAdminTest, GetSnapshotTooManyFailures) {
  using namespace ::testing;
  using namespace bigtable::chrono_literals;

  bigtable::TableAdmin tested(
      client_, "the-instance", bigtable::LimitedErrorCountRetryPolicy(3),
      bigtable::ExponentialBackoffPolicy(10_ms, 10_min));
  EXPECT_CALL(*client_, GetSnapshot(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")));
  bigtable::ClusterId cluster_id("other-cluster");
  bigtable::SnapshotId snapshot_id("other-snapshot");
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  // After all the setup, make the actual call we want to test.
  EXPECT_THROW(tested.GetSnapshot(cluster_id, snapshot_id),
               bigtable::GRpcError);
#else
  EXPECT_DEATH_IF_SUPPORTED(tested.GetSnapshot(cluster_id, snapshot_id),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify that bigtable::TableAdmin::DeleteSnapshot works as expected.
TEST_F(TableAdminTest, DeleteSnapshotSimple) {
  using namespace ::testing;
  using google::protobuf::Empty;

  bigtable::TableAdmin tested(client_, "the-instance");
  std::string expected_text = R"""(
      name: 'projects/the-project/instances/the-instance/clusters/the-cluster/snapshots/random-snapshot'
)""";
  auto mock = MockRpcFactory<btadmin::DeleteSnapshotRequest, Empty>::Create(
      expected_text);
  EXPECT_CALL(*client_, DeleteSnapshot(_, _, _)).WillOnce(Invoke(mock));

  // After all the setup, make the actual call we want to test.
  bigtable::ClusterId cluster_id("the-cluster");
  bigtable::SnapshotId snapshot_id("random-snapshot");
  tested.DeleteSnapshot(cluster_id, snapshot_id);
}

/**
 * @test Verify that `bigtable::TableAdmin::DeleteSnapshot` supports
 * only one try and let client know request status.
 */
TEST_F(TableAdminTest, DeleteSnapshotFailure) {
  using namespace ::testing;

  bigtable::TableAdmin tested(client_, "the-instance");
  EXPECT_CALL(*client_, DeleteSnapshot(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));
  bigtable::ClusterId cluster_id("other-cluster");
  bigtable::SnapshotId snapshot_id("other-snapshot");

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  // After all the setup, make the actual call we want to test.
  EXPECT_THROW(tested.DeleteSnapshot(cluster_id, snapshot_id),
               bigtable::GRpcError);
#else
  EXPECT_DEATH_IF_SUPPORTED(tested.DeleteSnapshot(cluster_id, snapshot_id),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/**
 * @test Verify that `bigtable::TableAdmin::ListSnapshots` works in the easy
 * case.
 */
TEST_F(TableAdminTest, ListSnapshots_Simple) {
  using namespace ::testing;
  bigtable::TableAdmin tested(client_, kInstanceId);
  auto mock_list_snapshots = create_list_snapshots_lambda("", "", {"s0", "s1"});
  EXPECT_CALL(*client_, ListSnapshots(_, _, _))
      .WillOnce(Invoke(mock_list_snapshots));

  bigtable::ClusterId cluster_id("the-cluster");
  auto actual_snapshots = tested.ListSnapshots(cluster_id);
  ASSERT_EQ(2UL, actual_snapshots.size());
  std::string instance_name = tested.instance_name();
  EXPECT_EQ(instance_name + "/clusters/the-cluster/snapshots/s0",
            actual_snapshots[0].name());
  EXPECT_EQ(instance_name + "/clusters/the-cluster/snapshots/s1",
            actual_snapshots[1].name());
}

/**
 * @test Verify that `bigtable::TableAdmin::ListSnapshots` works for std::list
 * container.
 */
TEST_F(TableAdminTest, ListSnapshots_SimpleList) {
  using namespace ::testing;
  bigtable::TableAdmin tested(client_, kInstanceId);
  auto mock_list_snapshots = create_list_snapshots_lambda("", "", {"s0", "s1"});
  EXPECT_CALL(*client_, ListSnapshots(_, _, _))
      .WillOnce(Invoke(mock_list_snapshots));

  bigtable::ClusterId cluster_id("the-cluster");
  std::list<btadmin::Snapshot> actual_snapshots =
      tested.ListSnapshots<std::list>(cluster_id);
  ASSERT_EQ(2UL, actual_snapshots.size());
  std::string instance_name = tested.instance_name();
  std::list<btadmin::Snapshot>::iterator it = actual_snapshots.begin();
  EXPECT_EQ(instance_name + "/clusters/the-cluster/snapshots/s0", it->name());
  it++;
  EXPECT_EQ(instance_name + "/clusters/the-cluster/snapshots/s1", it->name());
  it++;
  EXPECT_EQ(actual_snapshots.end(), it);
}

/**
 * @test Verify that `bigtable::TableAdmin::ListSnapshots` handles failures.
 */
TEST_F(TableAdminTest, ListSnapshots_RecoverableFailure) {
  using namespace ::testing;
  using namespace bigtable::chrono_literals;

  bigtable::TableAdmin tested(client_, "the-instance");
  auto mock_recoverable_failure =
      [](grpc::ClientContext* ctx, btadmin::ListSnapshotsRequest const& request,
         btadmin::ListSnapshotsResponse* response) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
      };

  auto list0 = create_list_snapshots_lambda("", "token-001", {"s0", "s1"});
  auto list1 = create_list_snapshots_lambda("token-001", "", {"s2", "s3"});
  EXPECT_CALL(*client_, ListSnapshots(_, _, _))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(list0))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(list1));

  bigtable::ClusterId cluster_id("the-cluster");
  auto actual_snapshots = tested.ListSnapshots(cluster_id);
  ASSERT_EQ(4UL, actual_snapshots.size());
  std::string instance_name = tested.instance_name();
  EXPECT_EQ(instance_name + "/clusters/the-cluster/snapshots/s0",
            actual_snapshots[0].name());
  EXPECT_EQ(instance_name + "/clusters/the-cluster/snapshots/s1",
            actual_snapshots[1].name());
  EXPECT_EQ(instance_name + "/clusters/the-cluster/snapshots/s2",
            actual_snapshots[2].name());
  EXPECT_EQ(instance_name + "/clusters/the-cluster/snapshots/s3",
            actual_snapshots[3].name());
}

/**
 * @test Verify that `bigtable::TableAdmin::ListSnapshots` handles unrecoverable
 * failure.
 */
TEST_F(TableAdminTest, ListSnapshots_UnrecoverableFailures) {
  using namespace ::testing;

  bigtable::TableAdmin tested(client_, "the-instance");
  EXPECT_CALL(*client_, ListSnapshots(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh-oh")));

  bigtable::ClusterId cluster_id("other-cluster");
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  // After all the setup, make the actual call we want to test.
  EXPECT_THROW(tested.ListSnapshots(cluster_id), bigtable::GRpcError);
#else
  EXPECT_DEATH_IF_SUPPORTED(tested.ListSnapshots(cluster_id),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}
