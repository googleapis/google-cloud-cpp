//   Copyright 2017 Google Inc.
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.

#include "bigtable/admin/table_admin.h"

#include <gmock/gmock.h>

#include <google/bigtable/admin/v2/bigtable_table_admin_mock.grpc.pb.h>

#include <absl/memory/memory.h>

namespace {
namespace btproto = ::google::bigtable::admin::v2;

class MockAdminClient : public bigtable::AdminClient {
 public:
  MOCK_CONST_METHOD0(project, std::string const&());
  MOCK_METHOD1(on_completion, void(grpc::Status const& status));
  MOCK_METHOD0(table_admin, btproto::BigtableTableAdmin::StubInterface&());
};

/// A fixture for the bigtable::TableAdmin tests.
class TableAdminTest : public ::testing::Test {
 protected:
  void SetUp() override {
    using namespace ::testing;

    EXPECT_CALL(*client_, project()).WillRepeatedly(ReturnRef(project_id_));
    EXPECT_CALL(*client_, table_admin())
        .WillRepeatedly(
            Invoke([this]() -> btproto::BigtableTableAdmin::StubInterface& {
              return *table_admin_stub_;
            }));
  }

  std::string const project_id_ = "the-project";
  std::shared_ptr<MockAdminClient> client_ =
      std::make_shared<MockAdminClient>();
  std::unique_ptr<btproto::MockBigtableTableAdminStub> table_admin_stub_ =
      absl::make_unique<btproto::MockBigtableTableAdminStub>();
};

// A lambda to create lambdas.  Basically we would be rewriting the same
// lambda twice without this thing.
auto create_list_tables_lambda = [](std::string expected_token,
                                    std::string returned_token) {
  return [expected_token, returned_token](
      grpc::ClientContext* ctx, btproto::ListTablesRequest const& request,
      btproto::ListTablesResponse* response) {
    EXPECT_EQ("projects/the-project/instances/the-instance", request.parent());
    EXPECT_EQ(btproto::Table::FULL, request.view());
    EXPECT_EQ(expected_token, request.page_token());

    EXPECT_NE(nullptr, response);
    auto& t0 = *response->add_tables();
    t0.set_name("projects/the-project/instances/the-instance/tables/t0");
    t0.set_granularity(btproto::Table::MILLIS);
    auto& t1 = *response->add_tables();
    t1.set_name("projects/the-project/instances/the-instance/tables/t1");
    t1.set_granularity(btproto::Table::MILLIS);
    // Return the right token.
    response->set_next_page_token(returned_token);
    return grpc::Status::OK;
  };
};
}  // namespace

/// @test Verify basic functionality in the bigtable::TableAdmin class.
TEST_F(TableAdminTest, Simple) {
  using namespace ::testing;

  bigtable::TableAdmin tested(client_, "the-instance");
  EXPECT_EQ("the-instance", tested.instance_id());
  EXPECT_EQ("projects/the-project/instances/the-instance",
            tested.instance_name());
}

/// @test Verify that bigtable::TableAdmin::ListTables works in the easy case.
TEST_F(TableAdminTest, ListTables) {
  using namespace ::testing;

  bigtable::TableAdmin tested(client_, "the-instance");
  auto mock_list_tables = create_list_tables_lambda("", "");
  EXPECT_CALL(*table_admin_stub_, ListTables(_, _, _))
      .WillOnce(Invoke(mock_list_tables));
  EXPECT_CALL(*client_, on_completion(_)).Times(1);

  // After all the setup, make the actual call we want to test.
  auto actual = tested.ListTables(btproto::Table::FULL);
  std::string instance_name = static_cast<std::string>(tested.instance_name());
  ASSERT_EQ(2UL, actual.size());
  EXPECT_EQ(instance_name + "/tables/t0", actual[0].name());
  EXPECT_EQ(instance_name + "/tables/t1", actual[1].name());
}

/// @test Verify that bigtable::TableAdmin::ListTables handles failures.
TEST_F(TableAdminTest, ListTablesRecoverableFailures) {
  using namespace ::testing;

  bigtable::TableAdmin tested(client_, "the-instance");
  auto mock_recoverable_failure = [](grpc::ClientContext* ctx,
                                     btproto::ListTablesRequest const& request,
                                     btproto::ListTablesResponse* response) {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
  };
  auto batch0 = create_list_tables_lambda("", "but-wait-there-is-more");
  auto batch1 = create_list_tables_lambda("but-wait-there-is-more", "");
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
  auto actual = tested.ListTables(btproto::Table::FULL);
  std::string instance_name = static_cast<std::string>(tested.instance_name());
  ASSERT_EQ(4UL, actual.size());
  EXPECT_EQ(instance_name + "/tables/t0", actual[0].name());
  EXPECT_EQ(instance_name + "/tables/t1", actual[1].name());
  EXPECT_EQ(instance_name + "/tables/t0", actual[2].name());
  EXPECT_EQ(instance_name + "/tables/t1", actual[3].name());
}
