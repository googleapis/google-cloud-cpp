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

#include "google/cloud/bigtable/internal/instance_admin.h"
#include "google/cloud/bigtable/testing/mock_instance_admin_client.h"
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <gmock/gmock.h>

namespace {
namespace btproto = ::google::bigtable::admin::v2;

using MockAdminClient = bigtable::testing::MockInstanceAdminClient;

std::string const kProjectId = "the-project";

/// A fixture for the bigtable::InstanceAdmin tests.
class InstanceAdminTest : public ::testing::Test {
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
auto create_list_instances_lambda = [](std::string expected_token,
                                       std::string returned_token,
                                       std::vector<std::string> instance_ids) {
  return [expected_token, returned_token, instance_ids](
             grpc::ClientContext* ctx,
             btproto::ListInstancesRequest const& request,
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

// A lambda to create lambdas.  Basically we would be rewriting the same
// lambda twice without this thing.
auto create_instance = [](std::string expected_token,
                          std::string returned_token) {
  return
      [expected_token, returned_token](
          grpc::ClientContext* ctx, btproto::GetInstanceRequest const& request,
          btproto::Instance* response) {
        EXPECT_NE(nullptr, response);
        response->set_name(request.name());
        return grpc::Status::OK;
      };
};

// A lambda to create lambdas.  Basically we would be rewriting the same
// lambda twice without this thing.
auto create_list_clusters_lambda =
    [](std::string expected_token, std::string returned_token,
       std::string instance_id, std::vector<std::string> cluster_ids) {
      return [expected_token, returned_token, instance_id, cluster_ids](
                 grpc::ClientContext* ctx,
                 btproto::ListClustersRequest const& request,
                 btproto::ListClustersResponse* response) {
        auto const instance_name =
            "projects/" + kProjectId + "/instances/" + instance_id;
        EXPECT_EQ(instance_name, request.parent());
        EXPECT_EQ(expected_token, request.page_token());

        EXPECT_NE(nullptr, response);
        for (auto const& cluster_id : cluster_ids) {
          auto& cluster = *response->add_clusters();
          cluster.set_name(instance_name + "/clusters/" + cluster_id);
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

using namespace ::testing;

/// @test Verify basic functionality in the `bigtable::InstanceAdmin` class.
TEST_F(InstanceAdminTest, Default) {
  bigtable::noex::InstanceAdmin tested(client_);
  EXPECT_EQ("the-project", tested.project_id());
}

TEST_F(InstanceAdminTest, CopyConstructor) {
  bigtable::noex::InstanceAdmin source(client_);
  std::string expected = source.project_id();
  bigtable::noex::InstanceAdmin copy(source);
  EXPECT_EQ(expected, copy.project_id());
}

TEST_F(InstanceAdminTest, MoveConstructor) {
  bigtable::noex::InstanceAdmin source(client_);
  std::string expected = source.project_id();
  bigtable::noex::InstanceAdmin copy(std::move(source));
  EXPECT_EQ(expected, copy.project_id());
}

TEST_F(InstanceAdminTest, CopyAssignment) {
  std::shared_ptr<MockAdminClient> other_client =
      std::make_shared<MockAdminClient>();
  std::string other_project = "other-project";
  EXPECT_CALL(*other_client, project())
      .WillRepeatedly(testing::ReturnRef(other_project));

  bigtable::noex::InstanceAdmin source(client_);
  std::string expected = source.project_id();
  bigtable::noex::InstanceAdmin dest(other_client);
  EXPECT_NE(expected, dest.project_id());
  dest = source;
  EXPECT_EQ(expected, dest.project_id());
}

TEST_F(InstanceAdminTest, MoveAssignment) {
  std::shared_ptr<MockAdminClient> other_client =
      std::make_shared<MockAdminClient>();
  std::string other_project = "other-project";
  EXPECT_CALL(*other_client, project())
      .WillRepeatedly(testing::ReturnRef(other_project));

  bigtable::noex::InstanceAdmin source(client_);
  std::string expected = source.project_id();
  bigtable::noex::InstanceAdmin dest(other_client);
  EXPECT_NE(expected, dest.project_id());
  dest = std::move(source);
  EXPECT_EQ(expected, dest.project_id());
}

/// @test Verify that `bigtable::InstanceAdmin::ListInstances` works in the easy
/// case.
TEST_F(InstanceAdminTest, ListInstances) {
  bigtable::noex::InstanceAdmin tested(client_);
  auto mock_list_instances = create_list_instances_lambda("", "", {"t0", "t1"});
  EXPECT_CALL(*client_, ListInstances(_, _, _))
      .WillOnce(Invoke(mock_list_instances));

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
  bigtable::noex::InstanceAdmin tested(client_);
  auto mock_recoverable_failure =
      [](grpc::ClientContext* ctx, btproto::ListInstancesRequest const& request,
         btproto::ListInstancesResponse* response) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
      };
  auto batch0 = create_list_instances_lambda("", "token-001", {"t0", "t1"});
  auto batch1 = create_list_instances_lambda("token-001", "", {"t2", "t3"});
  EXPECT_CALL(*client_, ListInstances(_, _, _))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(batch0))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(batch1));

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
  bigtable::noex::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, ListInstances(_, _, _))
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

  // After all the setup, make the actual call we want to test.
  // We expect the InstanceAdmin to make a call to let the client know the
  // request failed.
  grpc::Status status;
  tested.ListInstances(status);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("uh oh"));
}

/// @test Verify that `bigtable::InstanceAdmin::GetInstance` works in the simple
/// case.
TEST_F(InstanceAdminTest, GetInstance) {
  bigtable::noex::InstanceAdmin tested(client_);
  auto mock_instances = create_instance("", "");
  EXPECT_CALL(*client_, GetInstance(_, _, _)).WillOnce(Invoke(mock_instances));

  // After all the setup, make the actual call we want to test.
  grpc::Status status;
  std::string instance_id = "t0";
  auto actual = tested.GetInstance(instance_id, status);
  EXPECT_TRUE(status.ok());
  EXPECT_EQ("projects/the-project/instances/t0", actual.name());
}

/// @test Verify recoverable errors for GetInstance
TEST_F(InstanceAdminTest, GetInstanceRecoverableFailures) {
  bigtable::noex::InstanceAdmin tested(client_);
  auto mock_recoverable_failure = [](grpc::ClientContext* ctx,
                                     btproto::GetInstanceRequest const& request,
                                     btproto::Instance* response) {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
  };

  auto mock_instances = create_instance("", "");
  EXPECT_CALL(*client_, GetInstance(_, _, _))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(mock_instances));

  // After all the setup, make the actual call we want to test.
  grpc::Status status;
  std::string instance_id = "t0";
  auto actual = tested.GetInstance(instance_id, status);
  EXPECT_TRUE(status.ok());
  EXPECT_EQ("projects/the-project/instances/t0", actual.name());
}

/// @test Verify unrecoverable error for GetInstance
TEST_F(InstanceAdminTest, GetInstanceUnrecoverableFailures) {
  bigtable::noex::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, GetInstance(_, _, _))
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

  // After all the setup, make the actual call we want to test.
  grpc::Status status;
  std::string instance_id = "t0";
  auto actual = tested.GetInstance(instance_id, status);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("uh oh"));
}

/// @test Verify positive scenario for DeleteInstance
TEST_F(InstanceAdminTest, DeleteInstance) {
  using google::protobuf::Empty;
  bigtable::noex::InstanceAdmin tested(client_);
  std::string expected_text = R"""(
  name: 'projects/the-project/instances/the-instance'
      )""";
  auto mock = MockRpcFactory<btproto::DeleteInstanceRequest, Empty>::Create(
      expected_text);
  EXPECT_CALL(*client_, DeleteInstance(_, _, _)).WillOnce(Invoke(mock));
  grpc::Status status;
  // After all the setup, make the actual call we want to test.
  tested.DeleteInstance("the-instance", status);
  EXPECT_TRUE(status.ok());
}

/// @test Verify unrecoverable error for DeleteInstance
TEST_F(InstanceAdminTest, DeleteInstanceUnrecoverableError) {
  bigtable::noex::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, DeleteInstance(_, _, _))
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));
  // After all the setup, make the actual call we want to test.
  grpc::Status status;
  tested.DeleteInstance("the-instance", status);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("uh oh"));
}

/// @test Verify recoverable errors for DeleteInstance
TEST_F(InstanceAdminTest, DeleteInstanceRecoverableError) {
  bigtable::noex::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, DeleteInstance(_, _, _))
      .WillOnce(Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "uh oh")));
  // After all the setup, make the actual call we want to test.
  grpc::Status status;
  tested.DeleteInstance("the-instance", status);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("uh oh"));
}

/// @test Verify that `bigtable::InstanceAdmin::ListClusters` works in the easy
/// case.
TEST_F(InstanceAdminTest, ListClusters) {
  bigtable::noex::InstanceAdmin tested(client_);
  std::string const& instance_id = "the-instance";
  auto mock_list_clusters =
      create_list_clusters_lambda("", "", instance_id, {"t0", "t1"});
  EXPECT_CALL(*client_, ListClusters(_, _, _))
      .WillOnce(Invoke(mock_list_clusters));

  // After all the setup, make the actual call we want to test.
  grpc::Status status;
  auto actual = tested.ListClusters(instance_id, status);
  EXPECT_TRUE(status.ok());
  std::string instance_name = tested.InstanceName(instance_id);
  ASSERT_EQ(2UL, actual.size());
  EXPECT_EQ(instance_name + "/clusters/t0", actual[0].name());
  EXPECT_EQ(instance_name + "/clusters/t1", actual[1].name());
}

/// @test Verify that `bigtable::InstanceAdmin::ListClusters` handles failures.
TEST_F(InstanceAdminTest, ListClustersRecoverableFailures) {
  auto const instance_id = "the-instance";

  bigtable::noex::InstanceAdmin tested(client_);
  auto mock_recoverable_failure =
      [](grpc::ClientContext* ctx, btproto::ListClustersRequest const& request,
         btproto::ListClustersResponse* response) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
      };
  auto batch0 =
      create_list_clusters_lambda("", "token-001", instance_id, {"t0", "t1"});
  auto batch1 =
      create_list_clusters_lambda("token-001", "", instance_id, {"t2", "t3"});
  EXPECT_CALL(*client_, ListClusters(_, _, _))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(batch0))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(batch1));

  // After all the setup, make the actual call we want to test.
  grpc::Status status;
  auto actual = tested.ListClusters(instance_id, status);
  EXPECT_TRUE(status.ok());
  std::string instance_name = tested.InstanceName(instance_id);
  ASSERT_EQ(4UL, actual.size());
  EXPECT_EQ(instance_name + "/clusters/t0", actual[0].name());
  EXPECT_EQ(instance_name + "/clusters/t1", actual[1].name());
  EXPECT_EQ(instance_name + "/clusters/t2", actual[2].name());
  EXPECT_EQ(instance_name + "/clusters/t3", actual[3].name());
}

/**
 * @test Verify that `bigtable::InstanceAdmin::ListClusters` handles
 * unrecoverable failures.
 */
TEST_F(InstanceAdminTest, ListClustersUnrecoverableFailures) {
  bigtable::noex::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, ListClusters(_, _, _))
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

  // After all the setup, make the actual call we want to test.
  // We expect the InstanceAdmin to make a call to let the client know the
  // request failed.
  grpc::Status status;
  tested.ListClusters("the-instance", status);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("uh oh"));
}

/// @test Verify positive scenario for DeleteCluster
TEST_F(InstanceAdminTest, DeleteCluster) {
  using google::protobuf::Empty;
  bigtable::noex::InstanceAdmin tested(client_);
  std::string expected_text = R"""(
  name: 'projects/the-project/instances/the-instance/clusters/the-cluster'
      )""";
  auto mock = MockRpcFactory<btproto::DeleteClusterRequest, Empty>::Create(
      expected_text);
  EXPECT_CALL(*client_, DeleteCluster(_, _, _)).WillOnce(Invoke(mock));
  grpc::Status status;
  bigtable::InstanceId instance_id("the-instance");
  bigtable::ClusterId cluster_id("the-cluster");
  // After all the setup, make the actual call we want to test.
  tested.DeleteCluster(instance_id, cluster_id, status);
  EXPECT_TRUE(status.ok());
}

/// @test Verify unrecoverable error for DeleteCluster
TEST_F(InstanceAdminTest, DeleteClusterUnrecoverableError) {
  bigtable::noex::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, DeleteCluster(_, _, _))
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));
  grpc::Status status;
  bigtable::InstanceId instance_id("other-instance");
  bigtable::ClusterId cluster_id("other-cluster");
  // After all the setup, make the actual call we want to test.
  tested.DeleteCluster(instance_id, cluster_id, status);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("uh oh"));
}

/// @test Verify recoverable errors for DeleteCluster
TEST_F(InstanceAdminTest, DeleteClusterRecoverableError) {
  bigtable::noex::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, DeleteCluster(_, _, _))
      .WillOnce(Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "uh oh")));
  grpc::Status status;
  bigtable::InstanceId instance_id("other-instance");
  bigtable::ClusterId cluster_id("other-cluster");
  // After all the setup, make the actual call we want to test.
  tested.DeleteCluster(instance_id, cluster_id, status);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("uh oh"));
}
