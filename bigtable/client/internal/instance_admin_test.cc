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

#include "bigtable/client/internal/instance_admin.h"
#include <google/bigtable/admin/v2/bigtable_instance_admin_mock.grpc.pb.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <gmock/gmock.h>

namespace {
namespace btproto = ::google::bigtable::admin::v2;

class MockAdminClient : public bigtable::InstanceAdminClient {
 public:
  MOCK_CONST_METHOD0(project, std::string const&());
  MOCK_METHOD0(
      Stub, std::shared_ptr<btproto::BigtableInstanceAdmin::StubInterface>());
  MOCK_METHOD1(on_completion, void(grpc::Status const& status));
  MOCK_METHOD0(reset, void());
};

std::string const kProjectId = "the-project";

/// A fixture for the bigtable::InstanceAdmin tests.
class InstanceAdminTest : public ::testing::Test {
 protected:
  void SetUp() override {
    using namespace ::testing;

    EXPECT_CALL(*client_, project()).WillRepeatedly(ReturnRef(kProjectId));
    EXPECT_CALL(*client_, Stub()).WillRepeatedly(Return(instance_admin_stub_));
  }

  std::shared_ptr<MockAdminClient> client_ =
      std::make_shared<MockAdminClient>();
  std::shared_ptr<btproto::MockBigtableInstanceAdminStub> instance_admin_stub_ =
      std::make_shared<btproto::MockBigtableInstanceAdminStub>();
};

// A lambda to create lambdas.  Basically we would be rewriting the same
// lambda twice without this thing.
auto create_list_instances_lambda = [](std::string expected_token,
                                       std::string returned_token,
                                       std::vector<std::string> instance_ids) {
  return [expected_token, returned_token, instance_ids](
      grpc::ClientContext* ctx, btproto::ListInstancesRequest const& request,
      btproto::ListInstancesResponse* response) {
    auto const project_name = "projects/" + kProjectId;
    EXPECT_EQ(project_name, request.parent());
    EXPECT_EQ(expected_token, request.page_token());

    EXPECT_NE(nullptr, response);
    for (auto const& instance_id : instance_ids) {
      auto& instance = *response->add_instances();
      instance.set_name(project_name + "/instances/" + instance_id);
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

/// @test Verify basic functionality in the `bigtable::InstanceAdmin` class.
TEST_F(InstanceAdminTest, Default) {
  using namespace ::testing;

  bigtable::noex::InstanceAdmin tested(client_);
  EXPECT_EQ("the-project", tested.project_id());
}

/// @test Verify that `bigtable::InstanceAdmin::ListInstances` works in the easy
/// case.
TEST_F(InstanceAdminTest, ListInstances) {
  using namespace ::testing;

  bigtable::noex::InstanceAdmin tested(client_);
  auto mock_list_instances = create_list_instances_lambda("", "", {"t0", "t1"});
  EXPECT_CALL(*instance_admin_stub_, ListInstances(_, _, _))
      .WillOnce(Invoke(mock_list_instances));
  EXPECT_CALL(*client_, on_completion(_)).Times(1);

  // After all the setup, make the actual call we want to test.
  grpc::Status status;
  auto actual = tested.ListInstances(status);
  EXPECT_TRUE(status.ok());
  std::string instance_name = tested.project_name();
  ASSERT_EQ(2UL, actual.size());
  EXPECT_EQ(instance_name + "/instances/t0", actual[0].name());
  EXPECT_EQ(instance_name + "/instances/t1", actual[1].name());
}

/// @test Verify that `bigtable::InstanceAdmin::ListInstances` handles failures.
TEST_F(InstanceAdminTest, ListInstancesRecoverableFailures) {
  using namespace ::testing;

  bigtable::noex::InstanceAdmin tested(client_);
  auto mock_recoverable_failure = [](
      grpc::ClientContext* ctx, btproto::ListInstancesRequest const& request,
      btproto::ListInstancesResponse* response) {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
  };
  auto batch0 = create_list_instances_lambda("", "token-001", {"t0", "t1"});
  auto batch1 = create_list_instances_lambda("token-001", "", {"t2", "t3"});
  EXPECT_CALL(*instance_admin_stub_, ListInstances(_, _, _))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(batch0))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(batch1));
  // We expect the InstanceAdmin to make 5 calls and to let the client know
  // about them.
  EXPECT_CALL(*client_, on_completion(_)).Times(5);

  // After all the setup, make the actual call we want to test.
  grpc::Status status;
  auto actual = tested.ListInstances(status);
  EXPECT_TRUE(status.ok());
  std::string project_name = tested.project_name();
  ASSERT_EQ(4UL, actual.size());
  EXPECT_EQ(project_name + "/instances/t0", actual[0].name());
  EXPECT_EQ(project_name + "/instances/t1", actual[1].name());
  EXPECT_EQ(project_name + "/instances/t2", actual[2].name());
  EXPECT_EQ(project_name + "/instances/t3", actual[3].name());
}

/**
 * @test Verify that `bigtable::InstanceAdmin::ListInstances` handles
 * unrecoverable failures.
 */
TEST_F(InstanceAdminTest, ListInstancesUnrecoverableFailures) {
  using namespace ::testing;

  bigtable::noex::InstanceAdmin tested(client_);
  EXPECT_CALL(*instance_admin_stub_, ListInstances(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

  // After all the setup, make the actual call we want to test.
  // We expect the InstanceAdmin to make a call to let the client know the
  // request failed.
  grpc::Status status;
  EXPECT_CALL(*client_, on_completion(_)).Times(1);
  tested.ListInstances(status);
  EXPECT_FALSE(status.ok());
}
