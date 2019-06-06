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
#include "google/cloud/bigtable/testing/mock_completion_queue.h"
#include "google/cloud/bigtable/testing/mock_response_reader.h"
#include "google/cloud/internal/make_unique.h"
#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <gmock/gmock.h>
#include <chrono>

namespace {
namespace btadmin = ::google::bigtable::admin::v2;
namespace bigtable = google::cloud::bigtable;
using namespace google::cloud::testing_util::chrono_literals;
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
  return [expected_token, returned_token, table_names](
             grpc::ClientContext*, btadmin::ListTablesRequest const& request,
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
        [expected_request](grpc::ClientContext*, RequestType const& request,
                           ResponseType* response) {
          if (response == nullptr) {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                                "invalid call to MockRpcFactory::Create()");
          }
          RequestType expected;
          // Cannot use ASSERT_TRUE() here, it has an embedded "return;"
          EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(
              expected_request, &expected));
          std::string delta;
          google::protobuf::util::MessageDifferencer differencer;
          differencer.ReportDifferencesToString(&delta);
          EXPECT_TRUE(differencer.Compare(expected, request)) << delta;

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
        [expected_request, expected_result](grpc::ClientContext*,
                                            RequestType const& request,
                                            ResponseType* response) {
          if (response == nullptr) {
            return grpc::Status(
                grpc::StatusCode::INVALID_ARGUMENT,
                "invalid call to MockRpcMultiCallFactory::Create()");
          }
          RequestType expected;
          response->clear_consistent();
          // Cannot use ASSERT_TRUE() here, it has an embedded "return;"
          EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(
              expected_request, &expected));
          std::string delta;
          google::protobuf::util::MessageDifferencer differencer;
          differencer.ReportDifferencesToString(&delta);
          EXPECT_TRUE(differencer.Compare(expected, request)) << delta;

          response->set_consistent(expected_result);

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
  ASSERT_STATUS_OK(actual);
  auto const& v = *actual;
  std::string instance_name = tested.instance_name();
  ASSERT_EQ(2UL, v.size());
  EXPECT_EQ(instance_name + "/tables/t0", v[0].name());
  EXPECT_EQ(instance_name + "/tables/t1", v[1].name());
}

/// @test Verify that `bigtable::TableAdmin::ListTables` handles failures.
TEST_F(TableAdminTest, ListTablesRecoverableFailures) {
  using namespace ::testing;

  bigtable::TableAdmin tested(client_, "the-instance");
  auto mock_recoverable_failure = [](grpc::ClientContext*,
                                     btadmin::ListTablesRequest const&,
                                     btadmin::ListTablesResponse*) {
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
  using namespace ::testing;

  bigtable::TableAdmin tested(client_, "the-instance");
  EXPECT_CALL(*client_, ListTables(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

  EXPECT_FALSE(tested.ListTables(btadmin::Table::FULL));
}

/**
 * @test Verify that `bigtable::TableAdmin::ListTables` handles too many
 * recoverable failures.
 */
TEST_F(TableAdminTest, ListTablesTooManyFailures) {
  using namespace ::testing;
  using namespace google::cloud::testing_util::chrono_literals;

  bigtable::TableAdmin tested(
      client_, "the-instance", bigtable::LimitedErrorCountRetryPolicy(3),
      bigtable::ExponentialBackoffPolicy(10_ms, 10_min));
  auto mock_recoverable_failure = [](grpc::ClientContext*,
                                     btadmin::ListTablesRequest const&,
                                     btadmin::ListTablesResponse*) {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
  };
  EXPECT_CALL(*client_, ListTables(_, _, _))
      .WillRepeatedly(Invoke(mock_recoverable_failure));

  EXPECT_FALSE(tested.ListTables(btadmin::Table::FULL));
}

/// @test Verify that `bigtable::TableAdmin::Create` works in the easy case.
TEST_F(TableAdminTest, CreateTableSimple) {
  using namespace ::testing;
  using namespace google::cloud::testing_util::chrono_literals;

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
  auto table = tested.CreateTable("new-table", std::move(config));
  EXPECT_STATUS_OK(table);
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

  EXPECT_FALSE(tested.CreateTable("other-table", bigtable::TableConfig()));
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
  using namespace google::cloud::testing_util::chrono_literals;

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

  EXPECT_FALSE(table_admin.GetTable("other-table"));
  EXPECT_FALSE(table_admin_assign.GetTable("other-table"));
}

/// @test Verify that `bigtable::TableAdmin::GetTable` works in the easy case.
TEST_F(TableAdminTest, GetTableSimple) {
  using namespace ::testing;
  using namespace google::cloud::testing_util::chrono_literals;

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
  using namespace google::cloud::testing_util::chrono_literals;

  bigtable::TableAdmin tested(client_, "the-instance");
  EXPECT_CALL(*client_, GetTable(_, _, _))
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
  using namespace ::testing;
  using namespace google::cloud::testing_util::chrono_literals;

  bigtable::TableAdmin tested(
      client_, "the-instance", bigtable::LimitedErrorCountRetryPolicy(3),
      bigtable::ExponentialBackoffPolicy(10_ms, 10_min));
  EXPECT_CALL(*client_, GetTable(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")));

  // After all the setup, make the actual call we want to test.
  EXPECT_FALSE(tested.GetTable("other-table"));
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
  EXPECT_STATUS_OK(tested.DeleteTable("the-table"));
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

  // After all the setup, make the actual call we want to test.
  EXPECT_FALSE(tested.DeleteTable("other-table").ok());
}

/**
 * @test Verify that bigtable::TableAdmin::ModifyColumnFamilies works as
 * expected.
 */
TEST_F(TableAdminTest, ModifyColumnFamilies) {
  using namespace ::testing;
  using namespace google::cloud::testing_util::chrono_literals;
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
  using namespace google::cloud::testing_util::chrono_literals;

  bigtable::TableAdmin tested(client_, "the-instance");
  EXPECT_CALL(*client_, ModifyColumnFamilies(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

  using M = bigtable::ColumnFamilyModification;
  using GC = bigtable::GcRule;
  std::vector<M> changes{M::Create("foo", GC::MaxAge(48_h)),
                         M::Update("bar", GC::MaxAge(24_h))};

  EXPECT_FALSE(tested.ModifyColumnFamilies("other-table", std::move(changes)));
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
  EXPECT_STATUS_OK(tested.DropRowsByPrefix("the-table", "foobar"));
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

  EXPECT_FALSE(tested.DropRowsByPrefix("other-table", "prefix").ok());
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
  EXPECT_STATUS_OK(tested.DropAllRows("the-table"));
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

  // After all the setup, make the actual call we want to test.
  EXPECT_FALSE(tested.DropAllRows("other-table").ok());
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

  // After all the setup, make the actual call we want to test.
  EXPECT_FALSE(tested.GenerateConsistencyToken("other-table"));
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

  // After all the setup, make the actual call we want to test.
  auto result = tested.CheckConsistency("the-table", "test-token");
  ASSERT_STATUS_OK(result);
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

  // After all the setup, make the actual call we want to test.
  EXPECT_FALSE(tested.CheckConsistency("other-table", "test-token"));
}

using MockAsyncCheckConsistencyResponse =
    google::cloud::bigtable::testing::MockAsyncResponseReader<
        ::google::bigtable::admin::v2::CheckConsistencyResponse>;

/**
 * @test Verify that `bigtagble::TableAdmin::AsyncWaitForConsistency` works as
 * expected, with multiple asynchronous calls.
 */
TEST_F(TableAdminTest, AsyncWaitForConsistency_Simple) {
  using namespace ::testing;
  using namespace google::cloud::testing_util::chrono_literals;
  using google::cloud::internal::make_unique;

  bigtable::TableAdmin tested(client_, "test-instance");

  auto r1 = make_unique<MockAsyncCheckConsistencyResponse>();
  EXPECT_CALL(*r1, Finish(_, _, _))
      .WillOnce(Invoke([](btadmin::CheckConsistencyResponse* response,
                          grpc::Status* status, void*) {
        ASSERT_NE(nullptr, response);
        *status = grpc::Status(grpc::StatusCode::UNAVAILABLE, "try again");
      }));
  auto r2 = make_unique<MockAsyncCheckConsistencyResponse>();
  EXPECT_CALL(*r2, Finish(_, _, _))
      .WillOnce(Invoke([](btadmin::CheckConsistencyResponse* response,
                          grpc::Status* status, void*) {
        ASSERT_NE(nullptr, response);
        response->set_consistent(false);
        *status = grpc::Status::OK;
      }));
  auto r3 = make_unique<MockAsyncCheckConsistencyResponse>();
  EXPECT_CALL(*r3, Finish(_, _, _))
      .WillOnce(Invoke([](btadmin::CheckConsistencyResponse* response,
                          grpc::Status* status, void*) {
        ASSERT_NE(nullptr, response);
        response->set_consistent(true);
        *status = grpc::Status::OK;
      }));

  auto make_invoke = [](std::unique_ptr<MockAsyncCheckConsistencyResponse>& r) {
    return [&r](grpc::ClientContext*,
                btadmin::CheckConsistencyRequest const& request,
                grpc::CompletionQueue*) {
      EXPECT_EQ(
          "projects/the-project/instances/test-instance/tables/test-table",
          request.name());
      // This is safe, see comments in MockAsyncResponseReader.
      return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
          ::btadmin::CheckConsistencyResponse>>(r.get());
    };
  };

  EXPECT_CALL(*client_, project()).WillRepeatedly(ReturnRef(kProjectId));
  EXPECT_CALL(*client_, AsyncCheckConsistency(_, _, _))
      .WillOnce(Invoke(make_invoke(r1)))
      .WillOnce(Invoke(make_invoke(r2)))
      .WillOnce(Invoke(make_invoke(r3)));

  std::shared_ptr<bigtable::testing::MockCompletionQueue> cq_impl(
      new bigtable::testing::MockCompletionQueue);
  bigtable::CompletionQueue cq(cq_impl);

  google::cloud::future<google::cloud::StatusOr<bigtable::Consistency>> result =
      tested.AsyncWaitForConsistency(cq, "test-table", "test-async-token");

  // The future is not ready yet.
  auto future_status = result.wait_for(0_ms);
  EXPECT_EQ(std::future_status::timeout, future_status);

  // Simulate the completions for each event.

  // AsyncCheckConsistency() -> TRANSIENT
  cq_impl->SimulateCompletion(cq, true);
  future_status = result.wait_for(0_ms);
  EXPECT_EQ(std::future_status::timeout, future_status);

  // timer
  cq_impl->SimulateCompletion(cq, true);
  future_status = result.wait_for(0_ms);
  EXPECT_EQ(std::future_status::timeout, future_status);

  // AsyncCheckConsistency() -> !consistent
  cq_impl->SimulateCompletion(cq, true);
  future_status = result.wait_for(0_ms);
  EXPECT_EQ(std::future_status::timeout, future_status);

  // timer
  cq_impl->SimulateCompletion(cq, true);
  future_status = result.wait_for(0_ms);
  EXPECT_EQ(std::future_status::timeout, future_status);

  // AsyncCheckConsistency() -> consistent
  cq_impl->SimulateCompletion(cq, true);
  future_status = result.wait_for(0_ms);
  EXPECT_EQ(std::future_status::ready, future_status);

  // The future becomes ready on the first request that completes with a
  // permanent error.
  auto consistent = result.get();
  ASSERT_STATUS_OK(consistent);

  EXPECT_EQ(bigtable::Consistency::kConsistent, *consistent);
}

/**
 * @test Verify that `bigtable::TableAdmin::AsyncWaitForConsistency` makes only
 * one RPC attempt and reports errors on failure.
 */
TEST_F(TableAdminTest, AsyncWaitForConsistency_Failure) {
  using namespace ::testing;
  using namespace google::cloud::testing_util::chrono_literals;
  using google::cloud::internal::make_unique;

  bigtable::TableAdmin tested(client_, "test-instance");
  auto reader = make_unique<MockAsyncCheckConsistencyResponse>();
  EXPECT_CALL(*reader, Finish(_, _, _))
      .WillOnce(Invoke([](btadmin::CheckConsistencyResponse* response,
                          grpc::Status* status, void*) {
        ASSERT_NE(nullptr, response);
        *status = grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "oh no");
      }));
  EXPECT_CALL(*client_, project()).WillRepeatedly(ReturnRef(kProjectId));
  EXPECT_CALL(*client_, AsyncCheckConsistency(_, _, _))
      .WillOnce(Invoke([&](grpc::ClientContext*,
                           btadmin::CheckConsistencyRequest const& request,
                           grpc::CompletionQueue*) {
        EXPECT_EQ(
            "projects/the-project/instances/test-instance/tables/test-table",
            request.name());
        // This is safe, see comments in MockAsyncResponseReader.
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            ::btadmin::CheckConsistencyResponse>>(reader.get());
      }));

  std::shared_ptr<bigtable::testing::MockCompletionQueue> cq_impl(
      new bigtable::testing::MockCompletionQueue);
  bigtable::CompletionQueue cq(cq_impl);

  google::cloud::future<google::cloud::StatusOr<bigtable::Consistency>> result =
      tested.AsyncWaitForConsistency(cq, "test-table", "test-async-token");

  // The future is not ready yet.
  auto future_status = result.wait_for(0_ms);
  EXPECT_EQ(std::future_status::timeout, future_status);
  cq_impl->SimulateCompletion(cq, true);

  // The future becomes ready on the first request that completes with a
  // permanent error.
  future_status = result.wait_for(0_ms);
  EXPECT_EQ(std::future_status::ready, future_status);

  auto consistent = result.get();
  EXPECT_FALSE(consistent.ok());

  EXPECT_EQ(google::cloud::StatusCode::kPermissionDenied,
            consistent.status().code());
}
