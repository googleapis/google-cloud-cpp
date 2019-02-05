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
namespace btadmin = ::google::bigtable::admin::v2;
namespace bigtable = google::cloud::bigtable;

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

// A lambda to create lambdas. Basically we would be rewriting the same
// lambda twice without this thing.
auto create_list_instances_lambda =
    [](std::string expected_token, std::string returned_token,
       std::vector<std::string> instance_ids,
       std::vector<std::string> failed_locations = {}) {
      return [expected_token, returned_token, instance_ids, failed_locations](
                 grpc::ClientContext* ctx,
                 btadmin::ListInstancesRequest const& request,
                 btadmin::ListInstancesResponse* response) {
        auto const project_name = "projects/" + kProjectId;
        EXPECT_EQ(project_name, request.parent());
        EXPECT_EQ(expected_token, request.page_token());

        EXPECT_NE(nullptr, response);
        for (auto const& instance_id : instance_ids) {
          auto& instance = *response->add_instances();
          instance.set_name(project_name + "/instances/" + instance_id);
        }
        for (auto const& failed_location : failed_locations) {
          response->add_failed_locations(std::move(failed_location));
        }
        // Return the right token.
        response->set_next_page_token(returned_token);
        return grpc::Status::OK;
      };
    };

// A lambda to create lambdas. Basically we would be rewriting the same
// lambda twice without this thing.
auto create_instance = [](std::string expected_token,
                          std::string returned_token) {
  return
      [expected_token, returned_token](
          grpc::ClientContext* ctx, btadmin::GetInstanceRequest const& request,
          btadmin::Instance* response) {
        EXPECT_NE(nullptr, response);
        response->set_name(request.name());
        return grpc::Status::OK;
      };
};

// A lambda to create lambdas. Basically we would be rewriting the same lambda
// twice without using this thing.
auto create_cluster = []() {
  return [](grpc::ClientContext* ctx, btadmin::GetClusterRequest const& request,
            btadmin::Cluster* response) {
    EXPECT_NE(nullptr, response);
    response->set_name(request.name());
    return grpc::Status::OK;
  };
};

auto create_policy = []() {
  return [](grpc::ClientContext* ctx,
            ::google::iam::v1::GetIamPolicyRequest const& request,
            ::google::iam::v1::Policy* response) {
    EXPECT_NE(nullptr, response);
    response->set_version(3);
    response->set_etag("random-tag");
    return grpc::Status::OK;
  };
};

auto create_policy_with_params = []() {
  return [](grpc::ClientContext* ctx,
            ::google::iam::v1::SetIamPolicyRequest const& request,
            ::google::iam::v1::Policy* response) {
    EXPECT_NE(nullptr, response);
    *response = request.policy();
    return grpc::Status::OK;
  };
};

// A lambda to create lambdas. Basically we would be rewriting the same
// lambda twice without this thing.
auto create_list_clusters_lambda =
    [](std::string expected_token, std::string returned_token,
       std::string instance_id, std::vector<std::string> cluster_ids,
       std::vector<std::string> failed_locations = {}) {
      return [expected_token, returned_token, instance_id, cluster_ids,
              failed_locations](grpc::ClientContext* ctx,
                                btadmin::ListClustersRequest const& request,
                                btadmin::ListClustersResponse* response) {
        auto const instance_name =
            "projects/" + kProjectId + "/instances/" + instance_id;
        EXPECT_EQ(instance_name, request.parent());
        EXPECT_EQ(expected_token, request.page_token());

        EXPECT_NE(nullptr, response);
        for (auto const& cluster_id : cluster_ids) {
          auto& cluster = *response->add_clusters();
          cluster.set_name(instance_name + "/clusters/" + cluster_id);
        }
        for (auto const& failed_location : failed_locations) {
          response->add_failed_locations(std::move(failed_location));
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
  EXPECT_TRUE(actual.failed_locations.empty());
  std::string instance_name = tested.project_name();
  ASSERT_EQ(2UL, actual.instances.size());
  EXPECT_EQ(instance_name + "/instances/t0", actual.instances[0].name());
  EXPECT_EQ(instance_name + "/instances/t1", actual.instances[1].name());
}

/// @test Verify that `bigtable::InstanceAdmin::ListInstances` handles failures.
TEST_F(InstanceAdminTest, ListInstancesRecoverableFailures) {
  bigtable::noex::InstanceAdmin tested(client_);
  auto mock_recoverable_failure =
      [](grpc::ClientContext* ctx, btadmin::ListInstancesRequest const& request,
         btadmin::ListInstancesResponse* response) {
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
  EXPECT_TRUE(actual.failed_locations.empty());
  std::string project_name = tested.project_name();
  ASSERT_EQ(4UL, actual.instances.size());
  EXPECT_EQ(project_name + "/instances/t0", actual.instances[0].name());
  EXPECT_EQ(project_name + "/instances/t1", actual.instances[1].name());
  EXPECT_EQ(project_name + "/instances/t2", actual.instances[2].name());
  EXPECT_EQ(project_name + "/instances/t3", actual.instances[3].name());
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

/// @test Verify that `bigtable::InstanceAdmin::ListInstances` accumulates
/// failed locations.
TEST_F(InstanceAdminTest, ListInstancesFailedLocations) {
  bigtable::noex::InstanceAdmin tested(client_);
  auto batch0 =
      create_list_instances_lambda("", "token-001", {"t0"}, {"loc1", "loc2"});
  auto batch1 =
      create_list_instances_lambda("token-001", "token-002", {"t1"}, {});
  auto batch2 =
      create_list_instances_lambda("token-002", "", {"t2"}, {"loc1", "loc3"});
  EXPECT_CALL(*client_, ListInstances(_, _, _))
      .WillOnce(Invoke(batch0))
      .WillOnce(Invoke(batch1))
      .WillOnce(Invoke(batch2));

  // After all the setup, make the actual call we want to test.
  grpc::Status status;
  auto actual = tested.ListInstances(status);
  EXPECT_TRUE(status.ok());
  std::string project_name = tested.project_name();
  ASSERT_EQ(3UL, actual.instances.size());
  EXPECT_EQ(project_name + "/instances/t0", actual.instances[0].name());
  EXPECT_EQ(project_name + "/instances/t1", actual.instances[1].name());
  EXPECT_EQ(project_name + "/instances/t2", actual.instances[2].name());
  std::sort(actual.failed_locations.begin(), actual.failed_locations.end());
  std::vector<std::string> expected_failed_locations{"loc1", "loc2", "loc3"};
  EXPECT_EQ(expected_failed_locations, actual.failed_locations);
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
                                     btadmin::GetInstanceRequest const& request,
                                     btadmin::Instance* response) {
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
  auto mock = MockRpcFactory<btadmin::DeleteInstanceRequest, Empty>::Create(
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
  EXPECT_TRUE(actual.failed_locations.empty());
  std::string instance_name = tested.InstanceName(instance_id);
  ASSERT_EQ(2UL, actual.clusters.size());
  EXPECT_EQ(instance_name + "/clusters/t0", actual.clusters[0].name());
  EXPECT_EQ(instance_name + "/clusters/t1", actual.clusters[1].name());
}

/// @test Verify that `bigtable::InstanceAdmin::ListClusters` handles failures.
TEST_F(InstanceAdminTest, ListClustersRecoverableFailures) {
  auto const instance_id = "the-instance";

  bigtable::noex::InstanceAdmin tested(client_);
  auto mock_recoverable_failure =
      [](grpc::ClientContext* ctx, btadmin::ListClustersRequest const& request,
         btadmin::ListClustersResponse* response) {
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
  EXPECT_TRUE(actual.failed_locations.empty());
  std::string instance_name = tested.InstanceName(instance_id);
  ASSERT_EQ(4UL, actual.clusters.size());
  EXPECT_EQ(instance_name + "/clusters/t0", actual.clusters[0].name());
  EXPECT_EQ(instance_name + "/clusters/t1", actual.clusters[1].name());
  EXPECT_EQ(instance_name + "/clusters/t2", actual.clusters[2].name());
  EXPECT_EQ(instance_name + "/clusters/t3", actual.clusters[3].name());
}

/// @test Verify that `bigtable::InstanceAdmin::ListClusters` accumulates failed
/// locations.
TEST_F(InstanceAdminTest, ListClustersFailedLocations) {
  bigtable::noex::InstanceAdmin tested(client_);
  std::string const& instance_id = "the-instance";
  auto batch0 = create_list_clusters_lambda("", "token-001", instance_id,
                                            {"t0"}, {"loc1", "loc2"});
  auto batch1 = create_list_clusters_lambda("token-001", "token-002",
                                            instance_id, {"t1"}, {});
  auto batch2 = create_list_clusters_lambda("token-002", "", instance_id,
                                            {"t2"}, {"loc1", "loc3"});
  EXPECT_CALL(*client_, ListClusters(_, _, _))
      .WillOnce(Invoke(batch0))
      .WillOnce(Invoke(batch1))
      .WillOnce(Invoke(batch2));

  // After all the setup, make the actual call we want to test.
  grpc::Status status;
  auto actual = tested.ListClusters(instance_id, status);
  EXPECT_TRUE(status.ok());
  std::string instance_name = tested.InstanceName(instance_id);
  ASSERT_EQ(3UL, actual.clusters.size());
  EXPECT_EQ(instance_name + "/clusters/t0", actual.clusters[0].name());
  EXPECT_EQ(instance_name + "/clusters/t1", actual.clusters[1].name());
  EXPECT_EQ(instance_name + "/clusters/t2", actual.clusters[2].name());
  std::sort(actual.failed_locations.begin(), actual.failed_locations.end());
  std::vector<std::string> expected_failed_locations{"loc1", "loc2", "loc3"};
  EXPECT_EQ(expected_failed_locations, actual.failed_locations);
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

/// @test Verify positive scenario for GetCluster
TEST_F(InstanceAdminTest, GetCluster) {
  bigtable::noex::InstanceAdmin tested(client_);
  auto mock = create_cluster();
  EXPECT_CALL(*client_, GetCluster(_, _, _)).WillOnce(Invoke(mock));
  grpc::Status status;
  bigtable::InstanceId instance_id("the-instance");
  bigtable::ClusterId cluster_id("the-cluster");
  // After all the setup, make the actual call we want to test.
  auto cluster = tested.GetCluster(instance_id, cluster_id, status);
  EXPECT_EQ("projects/the-project/instances/the-instance/clusters/the-cluster",
            cluster.name());
  EXPECT_TRUE(status.ok());
}

/// @test Verify unrecoverable error for GetCluster
TEST_F(InstanceAdminTest, GetClusterUnrecoverableError) {
  bigtable::noex::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, GetCluster(_, _, _))
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));
  grpc::Status status;
  bigtable::InstanceId instance_id("other-instance");
  bigtable::ClusterId cluster_id("other-cluster");
  // After all the setup, make the actual call we want to test.
  tested.GetCluster(instance_id, cluster_id, status);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("uh oh"));
}

/// @test Verify recoverable errors for GetCluster
TEST_F(InstanceAdminTest, GetClusterRecoverableError) {
  bigtable::noex::InstanceAdmin tested(client_);
  auto mock_recoverable_failure = [](grpc::ClientContext* ctx,
                                     btadmin::GetClusterRequest const& request,
                                     btadmin::Cluster* response) {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
  };

  auto mock_cluster = create_cluster();
  EXPECT_CALL(*client_, GetCluster(_, _, _))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(mock_cluster));

  // After all the setup, make the actual call we want to test.
  grpc::Status status;
  bigtable::InstanceId instance_id("the-instance");
  bigtable::ClusterId cluster_id("the-cluster");
  // After all the setup, make the actual call we want to test.
  auto cluster = tested.GetCluster(instance_id, cluster_id, status);
  EXPECT_EQ("projects/the-project/instances/the-instance/clusters/the-cluster",
            cluster.name());
}

/// @test Verify positive scenario for DeleteCluster
TEST_F(InstanceAdminTest, DeleteCluster) {
  using google::protobuf::Empty;
  bigtable::noex::InstanceAdmin tested(client_);
  std::string expected_text = R"""(
  name: 'projects/the-project/instances/the-instance/clusters/the-cluster'
      )""";
  auto mock = MockRpcFactory<btadmin::DeleteClusterRequest, Empty>::Create(
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

/// @test Verify that `bigtable::noex::InstanceAdmin::CreateAppProfile` works.
TEST_F(InstanceAdminTest, CreateAppProfile) {
  using ::testing::_;
  using ::testing::Invoke;

  bigtable::noex::InstanceAdmin tested(client_);

  std::string expected_text = R"(
      name: 'projects/my-project/instances/test-instance/appProfiles/my-profile'
      etag: 'abc123='
      multi_cluster_routing_use_any { }
  )";

  btadmin::AppProfile expected;
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(expected_text, &expected));
  EXPECT_CALL(*client_, CreateAppProfile(_, _, _))
      .WillOnce(
          Invoke([&expected](grpc::ClientContext*,
                             btadmin::CreateAppProfileRequest const& request,
                             btadmin::AppProfile* response) {
            auto const project_name =
                "projects/" + kProjectId + "/instances/test-instance";
            EXPECT_EQ(project_name, request.parent());
            *response = expected;
            return grpc::Status::OK;
          }));

  grpc::Status status;
  auto actual =
      tested.CreateAppProfile(bigtable::InstanceId("test-instance"),
                              bigtable::AppProfileConfig::MultiClusterUseAny(
                                  bigtable::AppProfileId("my-profile")),
                              status);

  std::string delta;
  google::protobuf::util::MessageDifferencer differencer;
  differencer.ReportDifferencesToString(&delta);
  EXPECT_TRUE(differencer.Compare(expected, actual)) << delta;
}

/// @test Verify that `bigtable::noex::InstanceAdmin::GetAppProfile` works.
TEST_F(InstanceAdminTest, GetAppProfile) {
  using ::testing::_;
  using ::testing::Invoke;

  bigtable::noex::InstanceAdmin tested(client_);

  std::string expected_text = R"(
      name: 'projects/my-project/instances/test-instance/appProfiles/my-profile'
      etag: 'abc123='
      multi_cluster_routing_use_any { }
  )";

  btadmin::AppProfile expected;
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(expected_text, &expected));
  EXPECT_CALL(*client_, GetAppProfile(_, _, _))
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")))
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")))
      .WillOnce(Invoke([&expected](grpc::ClientContext*,
                                   btadmin::GetAppProfileRequest const& request,
                                   btadmin::AppProfile* response) {
        auto const profile_name =
            "projects/" + kProjectId +
            "/instances/test-instance/appProfiles/my-profile";
        EXPECT_EQ(profile_name, request.name());
        *response = expected;
        return grpc::Status::OK;
      }));

  grpc::Status status;
  auto actual =
      tested.GetAppProfile(bigtable::InstanceId("test-instance"),
                           bigtable::AppProfileId("my-profile"), status);

  std::string delta;
  google::protobuf::util::MessageDifferencer differencer;
  differencer.ReportDifferencesToString(&delta);
  EXPECT_TRUE(differencer.Compare(expected, actual)) << delta;
}

/// @test Verify that `GetAppProfile` handles unrecoverable errors.
TEST_F(InstanceAdminTest, GetAppProfileUnrecoverableError) {
  bigtable::noex::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, GetAppProfile(_, _, _))
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));
  grpc::Status status;
  bigtable::InstanceId instance_id("other-instance");
  bigtable::AppProfileId profile_id("a-profile");
  // After all the setup, make the actual call we want to test.
  tested.GetAppProfile(instance_id, profile_id, status);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("uh oh"));
}

auto create_list_app_profiles_lambda =
    [](std::string expected_token, std::string const& returned_token,
       std::string const& instance_id,
       std::vector<std::string> app_profile_ids) {
      return [expected_token, returned_token, instance_id, app_profile_ids](
                 grpc::ClientContext* ctx,
                 btadmin::ListAppProfilesRequest const& request,
                 btadmin::ListAppProfilesResponse* response) {
        auto const instance_name =
            "projects/" + kProjectId + "/instances/" + instance_id;
        EXPECT_EQ(instance_name, request.parent());
        EXPECT_EQ(expected_token, request.page_token());

        EXPECT_NE(nullptr, response);
        for (auto const& app_profile_id : app_profile_ids) {
          auto& app_profile = *response->add_app_profiles();
          auto name = instance_name;
          name += "/appProfiles/";
          name += app_profile_id;
          app_profile.set_name(name);
        }
        // Return the right token.
        response->set_next_page_token(returned_token);
        return grpc::Status::OK;
      };
    };

/// @test Verify that `bigtable::InstanceAdmin::ListAppProfiles` works in the
/// easy case.
TEST_F(InstanceAdminTest, ListAppProfiles) {
  bigtable::noex::InstanceAdmin tested(client_);
  std::string const& instance_id = "the-instance";
  auto mock_list_app_profiles =
      create_list_app_profiles_lambda("", "", instance_id, {"p0", "p1"});
  EXPECT_CALL(*client_, ListAppProfiles(_, _, _))
      .WillOnce(Invoke(mock_list_app_profiles));

  // After all the setup, make the actual call we want to test.
  grpc::Status status;
  auto actual = tested.ListAppProfiles(instance_id, status);
  EXPECT_TRUE(status.ok());
  std::string instance_name = tested.InstanceName(instance_id);
  ASSERT_EQ(2UL, actual.size());
  EXPECT_EQ(instance_name + "/appProfiles/p0", actual[0].name());
  EXPECT_EQ(instance_name + "/appProfiles/p1", actual[1].name());
}

/**
 * @test Verify that `bigtable::InstanceAdmin::ListAppProfiles` handles
 * recoverable failures.
 */
TEST_F(InstanceAdminTest, ListAppProfilesRecoverableFailures) {
  auto const instance_id = "the-instance";

  bigtable::noex::InstanceAdmin tested(client_);
  auto mock_recoverable_failure =
      [](grpc::ClientContext* ctx,
         btadmin::ListAppProfilesRequest const& request,
         btadmin::ListAppProfilesResponse* response) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
      };
  auto batch0 = create_list_app_profiles_lambda("", "token-001", instance_id,
                                                {"p0", "p1"});
  auto batch1 = create_list_app_profiles_lambda("token-001", "", instance_id,
                                                {"p2", "p3"});
  EXPECT_CALL(*client_, ListAppProfiles(_, _, _))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(batch0))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(batch1));

  // After all the setup, make the actual call we want to test.
  grpc::Status status;
  auto actual = tested.ListAppProfiles(instance_id, status);
  EXPECT_TRUE(status.ok());
  std::string instance_name = tested.InstanceName(instance_id);
  ASSERT_EQ(4UL, actual.size());
  EXPECT_EQ(instance_name + "/appProfiles/p0", actual[0].name());
  EXPECT_EQ(instance_name + "/appProfiles/p1", actual[1].name());
  EXPECT_EQ(instance_name + "/appProfiles/p2", actual[2].name());
  EXPECT_EQ(instance_name + "/appProfiles/p3", actual[3].name());
}

/**
 * @test Verify that `bigtable::InstanceAdmin::ListAppProfiles` handles
 * unrecoverable failures.
 */
TEST_F(InstanceAdminTest, ListAppProfilesUnrecoverableFailures) {
  bigtable::noex::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, ListAppProfiles(_, _, _))
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

  // After all the setup, make the actual call we want to test.
  // We expect the InstanceAdmin to make a call to let the client know the
  // request failed.
  grpc::Status status;
  tested.ListAppProfiles("the-instance", status);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("uh oh"));
}

/// @test Verify that `bigtable::noex::InstanceAdmin::DeleteAppProfile` works.
TEST_F(InstanceAdminTest, DeleteAppProfile) {
  using ::testing::_;
  using ::testing::Invoke;

  bigtable::noex::InstanceAdmin tested(client_);

  EXPECT_CALL(*client_, DeleteAppProfile(_, _, _))
      .WillOnce(Invoke([](grpc::ClientContext*,
                          btadmin::DeleteAppProfileRequest const& request,
                          google::protobuf::Empty*) {
        auto const profile_name =
            "projects/" + kProjectId +
            "/instances/test-instance/appProfiles/my-profile";
        EXPECT_EQ(profile_name, request.name());
        EXPECT_TRUE(request.ignore_warnings());
        return grpc::Status::OK;
      }));

  grpc::Status status;
  tested.DeleteAppProfile(bigtable::InstanceId("test-instance"),
                          bigtable::AppProfileId("my-profile"), true, status);
}

/// @test Verify that `DeleteAppProfile` stops on any errors.
TEST_F(InstanceAdminTest, DeleteAppProfileUnrecoverableError) {
  bigtable::noex::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, DeleteAppProfile(_, _, _))
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")));
  grpc::Status status;
  bigtable::InstanceId instance_id("other-instance");
  bigtable::AppProfileId profile_id("a-profile");
  tested.DeleteAppProfile(instance_id, profile_id, false, status);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("try-again"));
}

/// @test Verify positive scenario for InstanceAdmin::GetIamPolicy.
TEST_F(InstanceAdminTest, GetIamPolicy) {
  bigtable::noex::InstanceAdmin tested(client_);
  auto mock_policy = create_policy();
  EXPECT_CALL(*client_, GetIamPolicy(_, _, _)).WillOnce(Invoke(mock_policy));

  grpc::Status status;
  std::string resource = "test-resource";
  tested.GetIamPolicy(resource, status);

  EXPECT_TRUE(status.ok());
}

/// @test Verify unrecoverable errors for InstanceAdmin::GetIamPolicy/
TEST_F(InstanceAdminTest, GetIamPolicyUnrecoverableError) {
  bigtable::noex::InstanceAdmin tested(client_);

  EXPECT_CALL(*client_, GetIamPolicy(_, _, _))
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "err!")));

  grpc::Status status;
  std::string resource = "other-resource";
  tested.GetIamPolicy(resource, status);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("err!"));
}

/// @test Verify recoverable errors for InstanceAdmin::GetIamPolicy.
TEST_F(InstanceAdminTest, GetIamPolicyRecoverableError) {
  namespace iamproto = ::google::iam::v1;
  bigtable::noex::InstanceAdmin tested(client_);

  auto mock_recoverable_failure =
      [](grpc::ClientContext* ctx, iamproto::GetIamPolicyRequest const& request,
         iamproto::Policy* response) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
      };
  auto mock_policy = create_policy();

  EXPECT_CALL(*client_, GetIamPolicy(_, _, _))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(mock_policy));

  grpc::Status status;
  std::string resource = "test-resource";
  tested.GetIamPolicy(resource, status);
  EXPECT_TRUE(status.ok());
}

/// @test Verify positive scenario for InstanceAdmin::SetIamPolicy.
TEST_F(InstanceAdminTest, SetIamPolicy) {
  bigtable::noex::InstanceAdmin tested(client_);
  auto mock_policy = create_policy_with_params();
  EXPECT_CALL(*client_, SetIamPolicy(_, _, _)).WillOnce(Invoke(mock_policy));

  grpc::Status status;
  std::string resource = "test-resource";
  google::cloud::IamBindings iam_bindings =
      google::cloud::IamBindings("writer", {"abc@gmail.com", "xyz@gmail.com"});
  auto policy = tested.SetIamPolicy(resource, iam_bindings, "test-tag", status);

  EXPECT_TRUE(status.ok());
  EXPECT_EQ(1U, policy.bindings.size());
  EXPECT_EQ("test-tag", policy.etag);
}

/// @test Verify unrecoverable errors for InstanceAdmin::SetIamPolicy/
TEST_F(InstanceAdminTest, SetIamPolicyUnrecoverableError) {
  bigtable::noex::InstanceAdmin tested(client_);

  EXPECT_CALL(*client_, SetIamPolicy(_, _, _))
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "err!")));

  grpc::Status status;
  std::string resource = "test-resource";
  google::cloud::IamBindings iam_bindings =
      google::cloud::IamBindings("writer", {"abc@gmail.com", "xyz@gmail.com"});
  tested.SetIamPolicy(resource, iam_bindings, "test-tag", status);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("err!"));
}

/// @test Verify recoverable errors for InstanceAdmin::SetIamPolicy.
TEST_F(InstanceAdminTest, SetIamPolicyRecoverableError) {
  namespace iamproto = ::google::iam::v1;
  bigtable::noex::InstanceAdmin tested(client_);

  auto mock_recoverable_failure =
      [](grpc::ClientContext* ctx, iamproto::SetIamPolicyRequest const& request,
         iamproto::Policy* response) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
      };
  auto mock_policy = create_policy_with_params();

  EXPECT_CALL(*client_, SetIamPolicy(_, _, _))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(mock_policy));

  grpc::Status status;
  std::string resource = "test-resource";
  google::cloud::IamBindings iam_bindings =
      google::cloud::IamBindings("writer", {"abc@gmail.com", "xyz@gmail.com"});
  auto policy = tested.SetIamPolicy(resource, iam_bindings, "test-tag", status);

  EXPECT_TRUE(status.ok());
  EXPECT_EQ(1U, policy.bindings.size());
  EXPECT_EQ("test-tag", policy.etag);
}

/// @test Verify that InstanceAdmin::TestIamPermissions works in simple case.
TEST_F(InstanceAdminTest, TestIamPermissions) {
  namespace iamproto = ::google::iam::v1;
  bigtable::noex::InstanceAdmin tested(client_);

  auto mock_permission_set =
      [](grpc::ClientContext* ctx,
         iamproto::TestIamPermissionsRequest const& request,
         iamproto::TestIamPermissionsResponse* response) {
        EXPECT_NE(nullptr, response);
        std::vector<std::string> permissions = {"writer", "reader"};
        response->add_permissions(permissions[0]);
        response->add_permissions(permissions[1]);
        return grpc::Status::OK;
      };

  EXPECT_CALL(*client_, TestIamPermissions(_, _, _))
      .WillOnce(Invoke(mock_permission_set));

  grpc::Status status;
  std::string resource = "the-resource";
  auto permission_set = tested.TestIamPermissions(
      resource, {"reader", "writer", "owner"}, status);

  EXPECT_TRUE(status.ok());
  EXPECT_EQ(2U, permission_set.size());
}

/// @test Test for unrecoverable errors for InstanceAdmin::TestIamPermissions.
TEST_F(InstanceAdminTest, TestIamPermissionsUnrecoverableError) {
  bigtable::noex::InstanceAdmin tested(client_);

  EXPECT_CALL(*client_, TestIamPermissions(_, _, _))
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "err!")));

  grpc::Status status;
  std::string resource = "other-resource";
  tested.TestIamPermissions(resource, {"reader", "writer", "owner"}, status);

  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("err!"));
}

/// @test Test for recoverable errors for InstanceAdmin::TestIamPermissions.
TEST_F(InstanceAdminTest, TestIamPermissionsRecoverableError) {
  namespace iamproto = ::google::iam::v1;
  bigtable::noex::InstanceAdmin tested(client_);

  auto mock_recoverable_failure =
      [](grpc::ClientContext* ctx,
         iamproto::TestIamPermissionsRequest const& request,
         iamproto::TestIamPermissionsResponse* response) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
      };

  auto mock_permission_set =
      [](grpc::ClientContext* ctx,
         iamproto::TestIamPermissionsRequest const& request,
         iamproto::TestIamPermissionsResponse* response) {
        EXPECT_NE(nullptr, response);
        std::vector<std::string> permissions = {"writer", "reader"};
        response->add_permissions(permissions[0]);
        response->add_permissions(permissions[1]);
        return grpc::Status::OK;
      };
  EXPECT_CALL(*client_, TestIamPermissions(_, _, _))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(mock_permission_set));

  grpc::Status status;
  std::string resource = "the-resource";
  auto permission_set = tested.TestIamPermissions(
      resource, {"writer", "reader", "owner"}, status);

  EXPECT_TRUE(status.ok());
  EXPECT_EQ(2U, permission_set.size());
}
