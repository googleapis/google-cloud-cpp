// Copyright 2018 Google Inc.
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

#include "bigtable/client/internal/table_admin.h"
#include "bigtable/client/grpc_error.h"
#include "bigtable/client/testing/chrono_literals.h"
#include <google/bigtable/admin/v2/bigtable_table_admin_mock.grpc.pb.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <gmock/gmock.h>

namespace {
namespace btproto = ::google::bigtable::admin::v2;

class MockAdminClient : public bigtable::AdminClient {
 public:
  MOCK_CONST_METHOD0(project, std::string const&());
  MOCK_METHOD0(Stub,
               std::shared_ptr<btproto::BigtableTableAdmin::StubInterface>());
  MOCK_METHOD1(on_completion, void(grpc::Status const& status));
  MOCK_METHOD0(reset, void());
};

std::string const kProjectId = "the-project";
std::string const kInstanceId = "the-instance";

/// A fixture for the bigtable::noex::TableAdmin tests.
class TableAdminTest : public ::testing::Test {
 protected:
  void SetUp() override {
    using namespace ::testing;

    EXPECT_CALL(*client_, project()).WillRepeatedly(ReturnRef(kProjectId));
    EXPECT_CALL(*client_, Stub()).WillRepeatedly(Return(table_admin_stub_));
  }

  std::shared_ptr<MockAdminClient> client_ =
      std::make_shared<MockAdminClient>();
  std::shared_ptr<btproto::MockBigtableTableAdminStub> table_admin_stub_ =
      std::make_shared<btproto::MockBigtableTableAdminStub>();
};

// A lambda to create lambdas.  Basically we would be rewriting the same
// lambda twice without this thing.
auto create_list_tables_lambda = [](std::string expected_token,
                                    std::string returned_token,
                                    std::vector<std::string> table_names) {
  return [expected_token, returned_token, table_names](
      grpc::ClientContext* ctx, btproto::ListTablesRequest const& request,
      btproto::ListTablesResponse* response) {
    auto const instance_name =
        "projects/" + kProjectId + "/instances/" + kInstanceId;
    EXPECT_EQ(instance_name, request.parent());
    EXPECT_EQ(btproto::Table::FULL, request.view());
    EXPECT_EQ(expected_token, request.page_token());

    EXPECT_NE(nullptr, response);
    for (auto const& table_name : table_names) {
      auto& table = *response->add_tables();
      table.set_name(instance_name + "/tables/" + table_name);
      table.set_granularity(btproto::Table::MILLIS);
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

}  // anonymous namespace

/// @test Verify basic functionality in the `bigtable::TableAdmin` class.
TEST_F(TableAdminTest, Default) {
  using namespace ::testing;

  bigtable::noex::TableAdmin tested(client_, "the-instance");
  EXPECT_EQ("the-instance", tested.instance_id());
  EXPECT_EQ("projects/the-project/instances/the-instance",
            tested.instance_name());
}

/// @test Verify that `bigtable::TableAdmin::ListTables` works in the easy case.
TEST_F(TableAdminTest, ListTables) {
  using namespace ::testing;

  bigtable::noex::TableAdmin tested(client_, kInstanceId);
  auto mock_list_tables = create_list_tables_lambda("", "", {"t0", "t1"});
  EXPECT_CALL(*table_admin_stub_, ListTables(_, _, _))
      .WillOnce(Invoke(mock_list_tables));
  EXPECT_CALL(*client_, on_completion(_)).Times(1);

  // After all the setup, make the actual call we want to test.
  grpc::Status status;
  auto actual = tested.ListTables(btproto::Table::FULL, status);
  EXPECT_TRUE(status.ok());
  std::string instance_name = tested.instance_name();
  ASSERT_EQ(2UL, actual.size());
  EXPECT_EQ(instance_name + "/tables/t0", actual[0].name());
  EXPECT_EQ(instance_name + "/tables/t1", actual[1].name());
}

/// @test Verify that `bigtable::TableAdmin::ListTables` handles failures.
TEST_F(TableAdminTest, ListTablesRecoverableFailures) {
  using namespace ::testing;

  bigtable::noex::TableAdmin tested(client_, "the-instance");
  auto mock_recoverable_failure = [](grpc::ClientContext* ctx,
                                     btproto::ListTablesRequest const& request,
                                     btproto::ListTablesResponse* response) {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
  };
  auto batch0 = create_list_tables_lambda("", "token-001", {"t0", "t1"});
  auto batch1 = create_list_tables_lambda("token-001", "", {"t2", "t3"});
  EXPECT_CALL(*table_admin_stub_, ListTables(_, _, _))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(batch0))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(batch1));
  // We expect the TableAdmin to make 5 calls and to let the client know about
  // them.
  EXPECT_CALL(*client_, on_completion(_)).Times(5);

  // After all the setup, make the actual call we want to test.
  grpc::Status status;
  auto actual = tested.ListTables(btproto::Table::FULL, status);
  EXPECT_TRUE(status.ok());
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

  bigtable::noex::TableAdmin tested(client_, "the-instance");
  EXPECT_CALL(*table_admin_stub_, ListTables(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

  // After all the setup, make the actual call we want to test.
  grpc::Status status;
  EXPECT_CALL(*client_, on_completion(_)).Times(1);
  tested.ListTables(btproto::Table::FULL, status);
  EXPECT_FALSE(status.ok());
}

/**
 * @test Verify that `bigtable::TableAdmin::ListTables` handles too many
 * recoverable failures.
 */
TEST_F(TableAdminTest, ListTablesTooManyFailures) {
  using namespace ::testing;
  using namespace bigtable::chrono_literals;

  bigtable::noex::TableAdmin tested(
      client_, "the-instance", bigtable::LimitedErrorCountRetryPolicy(3),
      bigtable::ExponentialBackoffPolicy(10_ms, 10_min));
  auto mock_recoverable_failure = [](grpc::ClientContext* ctx,
                                     btproto::ListTablesRequest const& request,
                                     btproto::ListTablesResponse* response) {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
  };
  EXPECT_CALL(*table_admin_stub_, ListTables(_, _, _))
      .WillRepeatedly(Invoke(mock_recoverable_failure));
  grpc::Status status;
  // We expect the TableAdmin to make a call to let the client know the request
  // failed. Notice that it is prepared to tolerate 3 failures, so it is the
  // fourth failure that actually raises an error.
  EXPECT_CALL(*client_, on_completion(_)).Times(4);

  // After all the setup, make the actual call we want to test.
  tested.ListTables(btproto::Table::FULL, status);
  EXPECT_FALSE(status.ok());
}

/// @test Verify that `bigtable::TableAdmin::Create` works in the easy case.
TEST_F(TableAdminTest, CreateTableSimple) {
  using namespace ::testing;
  using namespace bigtable::chrono_literals;

  bigtable::noex::TableAdmin tested(client_, "the-instance");

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
      MockRpcFactory<btproto::CreateTableRequest, btproto::Table>::Create(
          expected_text);
  EXPECT_CALL(*table_admin_stub_, CreateTable(_, _, _))
      .WillOnce(Invoke(mock_create_table));
  EXPECT_CALL(*client_, on_completion(_)).Times(1);

  // After all the setup, make the actual call we want to test.
  using GC = bigtable::GcRule;
  bigtable::TableConfig config(
      {{"f1", GC::MaxNumVersions(1)}, {"f2", GC::MaxAge(1_s)}},
      {"a", "c", "p"});
  grpc::Status status;
  tested.CreateTable("new-table", std::move(config), status);
  EXPECT_TRUE(status.ok());
}

/**
 * @test Verify that `bigtable::TableAdmin::CreateTable` supports
 * only one try and let client know request status.
 */
TEST_F(TableAdminTest, CreateTableFailure) {
  using namespace ::testing;

  bigtable::noex::TableAdmin tested(client_, "the-instance");
  EXPECT_CALL(*table_admin_stub_, CreateTable(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));
  grpc::Status status;
  // We expect the TableAdmin to make a call to let the client know the request
  // failed.
  EXPECT_CALL(*client_, on_completion(_)).Times(1);
  // After all the setup, make the actual call we want to test.
  tested.CreateTable("other-table", bigtable::TableConfig(), status);
  EXPECT_FALSE(status.ok());
}

/// @test Verify that `bigtable::TableAdmin::GetTable` works in the easy case.
TEST_F(TableAdminTest, GetTableSimple) {
  using namespace ::testing;
  using namespace bigtable::chrono_literals;

  bigtable::noex::TableAdmin tested(client_, "the-instance");
  std::string expected_text = R"""(
name: 'projects/the-project/instances/the-instance/tables/the-table'
view: SCHEMA_VIEW
    )""";
  auto mock = MockRpcFactory<btproto::GetTableRequest, btproto::Table>::Create(
      expected_text);
  EXPECT_CALL(*table_admin_stub_, GetTable(_, _, _))
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")))
      .WillOnce(Invoke(mock));
  EXPECT_CALL(*client_, on_completion(_)).Times(2);
  grpc::Status status;
  // After all the setup, make the actual call we want to test.
  tested.GetTable("the-table", status);
  EXPECT_TRUE(status.ok());
}

/**
 * @test Verify that `bigtable::TableAdmin::GetTable` reports unrecoverable
 * failures.
 */
TEST_F(TableAdminTest, GetTableUnrecoverableFailures) {
  using namespace ::testing;
  using namespace bigtable::chrono_literals;

  bigtable::noex::TableAdmin tested(client_, "the-instance");
  EXPECT_CALL(*table_admin_stub_, GetTable(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::NOT_FOUND, "uh oh")));

  EXPECT_CALL(*client_, on_completion(_)).Times(1);
  grpc::Status status;
  // After all the setup, make the actual call we want to test.
  tested.GetTable("other-table", status);
  EXPECT_FALSE(status.ok());
}

/**
 * @test Verify that `bigtable::TableAdmin::GetTable` works with too many
 * recoverable failures.
 */
TEST_F(TableAdminTest, GetTableTooManyFailures) {
  using namespace ::testing;
  using namespace bigtable::chrono_literals;

  bigtable::noex::TableAdmin tested(
      client_, "the-instance", bigtable::LimitedErrorCountRetryPolicy(3),
      bigtable::ExponentialBackoffPolicy(10_ms, 10_min));
  EXPECT_CALL(*table_admin_stub_, GetTable(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")));

  // We expect the TableAdmin to make a call to let the client know the request
  // failed. Notice that it is prepared to tolerate 3 failures, so it is the
  // fourth failure that actually raises an error.
  EXPECT_CALL(*client_, on_completion(_)).Times(4);
  grpc::Status status;
  // After all the setup, make the actual call we want to test.
  tested.GetTable("other-table", status);
  EXPECT_FALSE(status.ok());
}

/// @test Verify that bigtable::TableAdmin::DeleteTable works as expected.
TEST_F(TableAdminTest, DeleteTable) {
  using namespace ::testing;
  using google::protobuf::Empty;

  bigtable::noex::TableAdmin tested(client_, "the-instance");
  std::string expected_text = R"""(
name: 'projects/the-project/instances/the-instance/tables/the-table'
    )""";
  auto mock =
      MockRpcFactory<btproto::DeleteTableRequest, Empty>::Create(expected_text);
  EXPECT_CALL(*table_admin_stub_, DeleteTable(_, _, _)).WillOnce(Invoke(mock));
  EXPECT_CALL(*client_, on_completion(_)).Times(1);

  grpc::Status status;
  // After all the setup, make the actual call we want to test.
  tested.DeleteTable("the-table", status);
  EXPECT_TRUE(status.ok());
}

/**
 * @test Verify that `bigtable::TableAdmin::CreateTable` supports
 * only one try and let client know request status.
 */
TEST_F(TableAdminTest, DeleteTableFailure) {
  using namespace ::testing;

  bigtable::noex::TableAdmin tested(client_, "the-instance");
  EXPECT_CALL(*table_admin_stub_, CreateTable(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

  // We expect the TableAdmin to make a call to let the client know the request
  // failed.
  EXPECT_CALL(*client_, on_completion(_)).Times(1);
  // After all the setup, make the actual call we want to test.
  grpc::Status status;
  tested.CreateTable("other-table", bigtable::TableConfig(), status);
  EXPECT_FALSE(status.ok());
}

/**
 * @test Verify that bigtable::TableAdmin::ModifyColumnFamilies works as
 * expected.
 */
TEST_F(TableAdminTest, ModifyColumnFamilies) {
  using namespace ::testing;
  using namespace bigtable::chrono_literals;
  using google::protobuf::Empty;

  bigtable::noex::TableAdmin tested(client_, "the-instance");
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
  auto mock = MockRpcFactory<btproto::ModifyColumnFamiliesRequest,
                             btproto::Table>::Create(expected_text);
  EXPECT_CALL(*table_admin_stub_, ModifyColumnFamilies(_, _, _))
      .WillOnce(Invoke(mock));
  EXPECT_CALL(*client_, on_completion(_)).Times(1);

  // After all the setup, make the actual call we want to test.
  using M = bigtable::ColumnFamilyModification;
  using GC = bigtable::GcRule;
  grpc::Status status;
  auto actual = tested.ModifyColumnFamilies(
      "the-table",
      {M::Create("foo", GC::MaxAge(48_h)), M::Update("bar", GC::MaxAge(24_h))},
      status);
  EXPECT_TRUE(status.ok());
}

/**
 * @test Verify that `bigtable::TableAdmin::ModifyColumnFamilies` makes only one
 * RPC attempt and reports errors on failure.
 */
TEST_F(TableAdminTest, ModifyColumnFamiliesFailure) {
  using namespace ::testing;
  using namespace bigtable::chrono_literals;

  bigtable::noex::TableAdmin tested(client_, "the-instance");
  EXPECT_CALL(*table_admin_stub_, ModifyColumnFamilies(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

  using M = bigtable::ColumnFamilyModification;
  using GC = bigtable::GcRule;
  std::vector<M> changes{M::Create("foo", GC::MaxAge(48_h)),
                         M::Update("bar", GC::MaxAge(24_h))};

  // We expect the TableAdmin to make a call to let the client know the request
  // failed.
  EXPECT_CALL(*client_, on_completion(_)).Times(1);
  // After all the setup, make the actual call we want to test.
  grpc::Status status;
  tested.ModifyColumnFamilies("other-table", std::move(changes), status);
  EXPECT_FALSE(status.ok());
}

/// @test Verify that bigtable::TableAdmin::DropRowsByPrefix works as expected.
TEST_F(TableAdminTest, DropRowsByPrefix) {
  using namespace ::testing;
  using google::protobuf::Empty;

  bigtable::noex::TableAdmin tested(client_, "the-instance");
  std::string expected_text = R"""(
name: 'projects/the-project/instances/the-instance/tables/the-table'
row_key_prefix: 'foobar'
    )""";
  auto mock = MockRpcFactory<btproto::DropRowRangeRequest, Empty>::Create(
      expected_text);
  EXPECT_CALL(*table_admin_stub_, DropRowRange(_, _, _)).WillOnce(Invoke(mock));
  EXPECT_CALL(*client_, on_completion(_)).Times(1);
  grpc::Status status;
  // After all the setup, make the actual call we want to test.
  tested.DropRowsByPrefix("the-table", "foobar", status);
  EXPECT_TRUE(status.ok());
}

/**
 * @test Verify that `bigtable::TableAdmin::DropRowsByPrefix` makes only one
 * RPC attempt and reports errors on failure.
 */
TEST_F(TableAdminTest, DropRowsByPrefixFailure) {
  using namespace ::testing;

  bigtable::noex::TableAdmin tested(client_, "the-instance");
  EXPECT_CALL(*table_admin_stub_, DropRowRange(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

  // We expect the TableAdmin to make a call to let the client know the request
  // failed.
  EXPECT_CALL(*client_, on_completion(_)).Times(1);
  // After all the setup, make the actual call we want to test.
  grpc::Status status;
  tested.DropRowsByPrefix("other-table", "prefix", status);
  EXPECT_FALSE(status.ok());
}

/// @test Verify that bigtable::TableAdmin::DropRowsByPrefix works as expected.
TEST_F(TableAdminTest, DropAllRows) {
  using namespace ::testing;
  using google::protobuf::Empty;

  bigtable::noex::TableAdmin tested(client_, "the-instance");
  std::string expected_text = R"""(
name: 'projects/the-project/instances/the-instance/tables/the-table'
delete_all_data_from_table: true
    )""";
  auto mock = MockRpcFactory<btproto::DropRowRangeRequest, Empty>::Create(
      expected_text);
  EXPECT_CALL(*table_admin_stub_, DropRowRange(_, _, _)).WillOnce(Invoke(mock));
  EXPECT_CALL(*client_, on_completion(_)).Times(1);
  grpc::Status status;
  // After all the setup, make the actual call we want to test.
  tested.DropAllRows("the-table", status);
  EXPECT_TRUE(status.ok());
}

/**
 * @test Verify that `bigtable::TableAdmin::DropAllRows` makes only one
 * RPC attempt and reports errors on failure.
 */
TEST_F(TableAdminTest, DropAllRowsFailure) {
  using namespace ::testing;

  bigtable::noex::TableAdmin tested(client_, "the-instance");
  EXPECT_CALL(*table_admin_stub_, DropRowRange(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

  // We expect the TableAdmin to make a call to let the client know the request
  // failed.
  grpc::Status status;
  EXPECT_CALL(*client_, on_completion(_)).Times(1);
  // After all the setup, make the actual call we want to test.
  tested.DropAllRows("other-table", status);
  EXPECT_FALSE(status.ok());
}
