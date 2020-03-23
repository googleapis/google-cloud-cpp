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

#include "google/cloud/bigtable/instance_admin.h"
#include "google/cloud/bigtable/testing/mock_async_failing_rpc_factory.h"
#include "google/cloud/bigtable/testing/mock_instance_admin_client.h"
#include "google/cloud/bigtable/testing/mock_response_reader.h"
#include "google/cloud/bigtable/testing/validate_metadata.h"
#include "google/cloud/internal/make_unique.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/mock_completion_queue.h"
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <gmock/gmock.h>

namespace {
namespace btadmin = google::bigtable::admin::v2;
namespace bigtable = google::cloud::bigtable;

using MockAdminClient = bigtable::testing::MockInstanceAdminClient;
using namespace google::cloud::testing_util::chrono_literals;
using google::cloud::testing_util::MockCompletionQueue;

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
             grpc::ClientContext* context,
             btadmin::ListInstancesRequest const& request,
             btadmin::ListInstancesResponse* response) {
    EXPECT_STATUS_OK(google::cloud::bigtable::testing::IsContextMDValid(
        *context,
        "google.bigtable.admin.v2.BigtableInstanceAdmin.ListInstances"));
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

// A lambda to create lambdas. Basically we would be rewriting the same lambda
// twice without using this thing.
auto create_get_cluster_mock = []() {
  return [](grpc::ClientContext* context,
            btadmin::GetClusterRequest const& request,
            btadmin::Cluster* response) {
    EXPECT_STATUS_OK(google::cloud::bigtable::testing::IsContextMDValid(
        *context, "google.bigtable.admin.v2.BigtableInstanceAdmin.GetCluster"));
    EXPECT_NE(nullptr, response);
    response->set_name(request.name());
    return grpc::Status::OK;
  };
};

auto create_get_policy_mock = []() {
  return [](grpc::ClientContext* context,
            ::google::iam::v1::GetIamPolicyRequest const&,
            ::google::iam::v1::Policy* response) {
    EXPECT_STATUS_OK(google::cloud::bigtable::testing::IsContextMDValid(
        *context,
        "google.bigtable.admin.v2.BigtableInstanceAdmin.GetIamPolicy"));
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
    EXPECT_STATUS_OK(google::cloud::bigtable::testing::IsContextMDValid(
        *context,
        "google.bigtable.admin.v2.BigtableInstanceAdmin.SetIamPolicy"));
    EXPECT_NE(nullptr, response);
    *response = request.policy();
    return grpc::Status::OK;
  };
};

// A lambda to create lambdas.  Basically we would be rewriting the same
// lambda twice without this thing.
auto create_list_clusters_lambda =
    [](std::string expected_token, std::string returned_token,
       std::string instance_id, std::vector<std::string> cluster_ids) {
      return [expected_token, returned_token, instance_id, cluster_ids](
                 grpc::ClientContext* context,
                 btadmin::ListClustersRequest const& request,
                 btadmin::ListClustersResponse* response) {
        EXPECT_STATUS_OK(google::cloud::bigtable::testing::IsContextMDValid(
            *context,
            "google.bigtable.admin.v2.BigtableInstanceAdmin.ListClusters"));
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
  static std::function<SignatureType> Create(std::string expected_request,
                                             std::string method) {
    return std::function<SignatureType>(
        [expected_request, method](grpc::ClientContext* context,
                                   RequestType const& request,
                                   ResponseType* response) {
          EXPECT_STATUS_OK(google::cloud::bigtable::testing::IsContextMDValid(
              *context, method));
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

  bigtable::InstanceAdmin tested(client_);
  EXPECT_EQ("the-project", tested.project_id());
}

TEST_F(InstanceAdminTest, CopyConstructor) {
  bigtable::InstanceAdmin source(client_);
  std::string expected = source.project_id();
  bigtable::InstanceAdmin copy(source);
  EXPECT_EQ(expected, copy.project_id());
}

TEST_F(InstanceAdminTest, MoveConstructor) {
  bigtable::InstanceAdmin source(client_);
  std::string expected = source.project_id();
  bigtable::InstanceAdmin copy(std::move(source));
  EXPECT_EQ(expected, copy.project_id());
}

TEST_F(InstanceAdminTest, CopyAssignment) {
  std::shared_ptr<MockAdminClient> other_client =
      std::make_shared<MockAdminClient>();
  std::string other_project = "other-project";
  EXPECT_CALL(*other_client, project())
      .WillRepeatedly(testing::ReturnRef(other_project));

  bigtable::InstanceAdmin source(client_);
  std::string expected = source.project_id();
  bigtable::InstanceAdmin dest(other_client);
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

  bigtable::InstanceAdmin source(client_);
  std::string expected = source.project_id();
  bigtable::InstanceAdmin dest(other_client);
  EXPECT_NE(expected, dest.project_id());
  dest = std::move(source);
  EXPECT_EQ(expected, dest.project_id());
}

/// @test Verify that `bigtable::InstanceAdmin::ListInstances` works in the easy
/// case.
TEST_F(InstanceAdminTest, ListInstances) {
  using namespace ::testing;

  bigtable::InstanceAdmin tested(client_);
  auto mock_list_instances = create_list_instances_lambda("", "", {"t0", "t1"});
  EXPECT_CALL(*client_, ListInstances(_, _, _))
      .WillOnce(Invoke(mock_list_instances));

  // After all the setup, make the actual call we want to test.
  auto actual = tested.ListInstances();
  ASSERT_STATUS_OK(actual);
  EXPECT_TRUE(actual->failed_locations.empty());
  std::string instance_name = tested.project_name();
  ASSERT_EQ(2UL, actual->instances.size());
  EXPECT_EQ(instance_name + "/instances/t0", actual->instances[0].name());
  EXPECT_EQ(instance_name + "/instances/t1", actual->instances[1].name());
}

/// @test Verify that `bigtable::InstanceAdmin::ListInstances` handles failures.
TEST_F(InstanceAdminTest, ListInstancesRecoverableFailures) {
  using namespace ::testing;

  bigtable::InstanceAdmin tested(client_);
  auto mock_recoverable_failure = [](grpc::ClientContext* context,
                                     btadmin::ListInstancesRequest const&,
                                     btadmin::ListInstancesResponse*) {
    EXPECT_STATUS_OK(google::cloud::bigtable::testing::IsContextMDValid(
        *context,
        "google.bigtable.admin.v2.BigtableInstanceAdmin.ListInstances"));
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
  auto actual = tested.ListInstances();
  ASSERT_STATUS_OK(actual);
  EXPECT_TRUE(actual->failed_locations.empty());
  std::string project_name = tested.project_name();
  ASSERT_EQ(4UL, actual->instances.size());
  EXPECT_EQ(project_name + "/instances/t0", actual->instances[0].name());
  EXPECT_EQ(project_name + "/instances/t1", actual->instances[1].name());
  EXPECT_EQ(project_name + "/instances/t2", actual->instances[2].name());
  EXPECT_EQ(project_name + "/instances/t3", actual->instances[3].name());
}

/**
 * @test Verify that `bigtable::InstanceAdmin::ListInstances` handles
 * unrecoverable failures.
 */
TEST_F(InstanceAdminTest, ListInstancesUnrecoverableFailures) {
  using namespace ::testing;

  bigtable::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, ListInstances(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

  // After all the setup, make the actual call we want to test.
  EXPECT_FALSE(tested.ListInstances());
}

/// @test Verify that DeleteInstance works in the positive case.
TEST_F(InstanceAdminTest, DeleteInstance) {
  using namespace ::testing;
  using google::protobuf::Empty;
  bigtable::InstanceAdmin tested(client_);
  std::string expected_text = R"""(
  name: 'projects/the-project/instances/the-instance'
      )""";
  auto mock = MockRpcFactory<btadmin::DeleteInstanceRequest, Empty>::Create(
      expected_text,
      "google.bigtable.admin.v2.BigtableInstanceAdmin.DeleteInstance");
  EXPECT_CALL(*client_, DeleteInstance(_, _, _)).WillOnce(Invoke(mock));
  // After all the setup, make the actual call we want to test.
  ASSERT_STATUS_OK(tested.DeleteInstance("the-instance"));
}

/// @test Verify unrecoverable error for DeleteInstance
TEST_F(InstanceAdminTest, DeleteInstanceUnrecoverableError) {
  using namespace ::testing;
  bigtable::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, DeleteInstance(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));
  // After all the setup, make the actual call we want to test.
  EXPECT_FALSE(tested.DeleteInstance("other-instance").ok());
}

/// @test Verify that recoverable error for DeleteInstance
TEST_F(InstanceAdminTest, DeleteInstanceRecoverableError) {
  using namespace ::testing;
  bigtable::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, DeleteInstance(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")));

  // After all the setup, make the actual call we want to test.
  EXPECT_FALSE(tested.DeleteInstance("other-instance").ok());
}

/// @test Verify that `bigtable::InstanceAdmin::ListClusters` works in the easy
/// case.
TEST_F(InstanceAdminTest, ListClusters) {
  using namespace ::testing;

  bigtable::InstanceAdmin tested(client_);
  std::string const& instance_id = "the-instance";
  auto mock_list_clusters =
      create_list_clusters_lambda("", "", instance_id, {"t0", "t1"});
  EXPECT_CALL(*client_, ListClusters(_, _, _))
      .WillOnce(Invoke(mock_list_clusters));

  // After all the setup, make the actual call we want to test.
  auto actual = tested.ListClusters(instance_id);
  EXPECT_TRUE(actual->failed_locations.empty());
  std::string instance_name = tested.InstanceName(instance_id);
  ASSERT_EQ(2UL, actual->clusters.size());
  EXPECT_EQ(instance_name + "/clusters/t0", actual->clusters[0].name());
  EXPECT_EQ(instance_name + "/clusters/t1", actual->clusters[1].name());
}

/// @test Verify that `bigtable::InstanceAdmin::ListClusters` handles failures.
TEST_F(InstanceAdminTest, ListClustersRecoverableFailures) {
  using namespace ::testing;

  bigtable::InstanceAdmin tested(client_);
  auto mock_recoverable_failure = [](grpc::ClientContext* context,
                                     btadmin::ListClustersRequest const&,
                                     btadmin::ListClustersResponse*) {
    EXPECT_STATUS_OK(google::cloud::bigtable::testing::IsContextMDValid(
        *context,
        "google.bigtable.admin.v2.BigtableInstanceAdmin.ListClusters"));
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
  };
  std::string const& instance_id = "the-instance";
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
  auto actual = tested.ListClusters(instance_id);
  ASSERT_STATUS_OK(actual);
  EXPECT_TRUE(actual->failed_locations.empty());
  std::string instance_name = tested.InstanceName(instance_id);
  ASSERT_EQ(4UL, actual->clusters.size());
  EXPECT_EQ(instance_name + "/clusters/t0", actual->clusters[0].name());
  EXPECT_EQ(instance_name + "/clusters/t1", actual->clusters[1].name());
  EXPECT_EQ(instance_name + "/clusters/t2", actual->clusters[2].name());
  EXPECT_EQ(instance_name + "/clusters/t3", actual->clusters[3].name());
}

/**
 * @test Verify that `bigtable::InstanceAdmin::ListClusters` handles
 * unrecoverable failures.
 */
TEST_F(InstanceAdminTest, ListClustersUnrecoverableFailures) {
  using namespace ::testing;

  bigtable::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, ListClusters(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

  std::string const& instance_id = "the-instance";
  // After all the setup, make the actual call we want to test.
  EXPECT_FALSE(tested.ListClusters(instance_id));
}

/// @test Verify positive scenario for GetCluster
TEST_F(InstanceAdminTest, GetCluster) {
  using namespace ::testing;

  bigtable::InstanceAdmin tested(client_);
  auto mock = create_get_cluster_mock();
  EXPECT_CALL(*client_, GetCluster(_, _, _)).WillOnce(Invoke(mock));
  // After all the setup, make the actual call we want to test.
  auto cluster = tested.GetCluster("the-instance", "the-cluster");
  ASSERT_STATUS_OK(cluster);
  EXPECT_EQ("projects/the-project/instances/the-instance/clusters/the-cluster",
            cluster->name());
}

/// @test Verify unrecoverable error for GetCluster
TEST_F(InstanceAdminTest, GetClusterUnrecoverableError) {
  using namespace ::testing;

  bigtable::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, GetCluster(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));
  ASSERT_FALSE(tested.GetCluster("other-instance", "the-cluster"));
}

/// @test Verify recoverable errors for GetCluster
TEST_F(InstanceAdminTest, GetClusterRecoverableError) {
  using namespace ::testing;

  bigtable::InstanceAdmin tested(client_);
  auto mock_recoverable_failure = [](grpc::ClientContext* context,
                                     btadmin::GetClusterRequest const&,
                                     btadmin::Cluster*) {
    EXPECT_STATUS_OK(google::cloud::bigtable::testing::IsContextMDValid(
        *context, "google.bigtable.admin.v2.BigtableInstanceAdmin.GetCluster"));
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
  };

  auto mock_cluster = create_get_cluster_mock();
  EXPECT_CALL(*client_, GetCluster(_, _, _))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(mock_cluster));

  // After all the setup, make the actual call we want to test.
  auto cluster = tested.GetCluster("the-instance", "the-cluster");
  ASSERT_STATUS_OK(cluster);
  EXPECT_EQ("projects/the-project/instances/the-instance/clusters/the-cluster",
            cluster->name());
}

/// @test Verify that DeleteCluster works in the positive case.
TEST_F(InstanceAdminTest, DeleteCluster) {
  using namespace ::testing;
  using google::protobuf::Empty;
  bigtable::InstanceAdmin tested(client_);
  std::string expected_text = R"""(
  name: 'projects/the-project/instances/the-instance/clusters/the-cluster'
      )""";
  auto mock = MockRpcFactory<btadmin::DeleteClusterRequest, Empty>::Create(
      expected_text,
      "google.bigtable.admin.v2.BigtableInstanceAdmin.DeleteCluster");
  EXPECT_CALL(*client_, DeleteCluster(_, _, _)).WillOnce(Invoke(mock));
  // After all the setup, make the actual call we want to test.
  ASSERT_STATUS_OK(tested.DeleteCluster("the-instance", "the-cluster"));
}

/// @test Verify unrecoverable error for DeleteCluster
TEST_F(InstanceAdminTest, DeleteClusterUnrecoverableError) {
  using namespace ::testing;
  bigtable::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, DeleteCluster(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));
  // After all the setup, make the actual call we want to test.
  EXPECT_FALSE(tested.DeleteCluster("other-instance", "other-cluster").ok());
}

/// @test Verify that recoverable error for DeleteCluster
TEST_F(InstanceAdminTest, DeleteClusterRecoverableError) {
  using namespace ::testing;
  bigtable::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, DeleteCluster(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")));

  // After all the setup, make the actual call we want to test.
  EXPECT_FALSE(tested.DeleteCluster("other-instance", "other-cluster").ok());
}

/// @test Verify positive scenario for InstanceAdmin::GetIamPolicy.
TEST_F(InstanceAdminTest, GetIamPolicy) {
  using ::testing::_;
  using ::testing::Invoke;

  bigtable::InstanceAdmin tested(client_);
  auto mock_policy = create_get_policy_mock();
  EXPECT_CALL(*client_, GetIamPolicy(_, _, _)).WillOnce(Invoke(mock_policy));

  std::string resource = "test-resource";
  tested.GetIamPolicy(resource);
}

/// @test Verify that IamPolicies with conditions cause failures.
TEST_F(InstanceAdminTest, GetIamPolicyWithConditionsFails) {
  using ::testing::_;
  using ::testing::Invoke;

  bigtable::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, GetIamPolicy(_, _, _))
      .WillOnce(Invoke([](grpc::ClientContext* context,
                          ::google::iam::v1::GetIamPolicyRequest const&,
                          ::google::iam::v1::Policy* response) {
        EXPECT_STATUS_OK(google::cloud::bigtable::testing::IsContextMDValid(
            *context,
            "google.bigtable.admin.v2.BigtableInstanceAdmin.GetIamPolicy"));
        EXPECT_NE(nullptr, response);
        response->set_version(3);
        response->set_etag("random-tag");
        auto new_binding = response->add_bindings();
        new_binding->set_role("writer");
        new_binding->add_members("abc@gmail.com");
        new_binding->add_members("xyz@gmail.com");
        new_binding->set_allocated_condition(new google::type::Expr);

        return grpc::Status::OK;
      }));

  std::string resource = "test-resource";
  auto res = tested.GetIamPolicy(resource);
  ASSERT_FALSE(res);
  EXPECT_EQ(google::cloud::StatusCode::kUnimplemented, res.status().code());
}

/// @test Verify unrecoverable errors for InstanceAdmin::GetIamPolicy.
TEST_F(InstanceAdminTest, GetIamPolicyUnrecoverableError) {
  using ::testing::_;
  using ::testing::Return;

  bigtable::InstanceAdmin tested(client_);

  EXPECT_CALL(*client_, GetIamPolicy(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "err!")));

  std::string resource = "other-resource";

  EXPECT_FALSE(tested.GetIamPolicy(resource));
}

/// @test Verify recoverable errors for InstanceAdmin::GetIamPolicy.
TEST_F(InstanceAdminTest, GetIamPolicyRecoverableError) {
  using ::testing::_;
  using ::testing::Invoke;
  namespace iamproto = ::google::iam::v1;

  bigtable::InstanceAdmin tested(client_);

  auto mock_recoverable_failure = [](grpc::ClientContext* context,
                                     iamproto::GetIamPolicyRequest const&,
                                     iamproto::Policy*) {
    EXPECT_STATUS_OK(google::cloud::bigtable::testing::IsContextMDValid(
        *context,
        "google.bigtable.admin.v2.BigtableInstanceAdmin.GetIamPolicy"));
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
  };
  auto mock_policy = create_get_policy_mock();

  EXPECT_CALL(*client_, GetIamPolicy(_, _, _))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(mock_policy));

  std::string resource = "test-resource";
  tested.GetIamPolicy(resource);
}

/// @test Verify positive scenario for InstanceAdmin::GetNativeIamPolicy.
TEST_F(InstanceAdminTest, GetNativeIamPolicy) {
  using ::testing::_;
  using ::testing::Invoke;

  bigtable::InstanceAdmin tested(client_);
  auto mock_policy = create_get_policy_mock();
  EXPECT_CALL(*client_, GetIamPolicy(_, _, _)).WillOnce(Invoke(mock_policy));

  std::string resource = "test-resource";
  auto policy = tested.GetNativeIamPolicy(resource);
  ASSERT_STATUS_OK(policy);
  EXPECT_EQ(3, policy->version());
  EXPECT_EQ("random-tag", policy->etag());
}

/// @test Verify unrecoverable errors for InstanceAdmin::GetNativeIamPolicy.
TEST_F(InstanceAdminTest, GetNativeIamPolicyUnrecoverableError) {
  using ::testing::_;
  using ::testing::Return;

  bigtable::InstanceAdmin tested(client_);

  EXPECT_CALL(*client_, GetIamPolicy(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "err!")));

  std::string resource = "other-resource";

  EXPECT_FALSE(tested.GetNativeIamPolicy(resource));
}

/// @test Verify recoverable errors for InstanceAdmin::GetNativeIamPolicy.
TEST_F(InstanceAdminTest, GetNativeIamPolicyRecoverableError) {
  using ::testing::_;
  using ::testing::Invoke;
  namespace iamproto = ::google::iam::v1;

  bigtable::InstanceAdmin tested(client_);

  auto mock_recoverable_failure = [](grpc::ClientContext* context,
                                     iamproto::GetIamPolicyRequest const&,
                                     iamproto::Policy*) {
    EXPECT_STATUS_OK(google::cloud::bigtable::testing::IsContextMDValid(
        *context,
        "google.bigtable.admin.v2.BigtableInstanceAdmin.GetIamPolicy"));
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
  };
  auto mock_policy = create_get_policy_mock();

  EXPECT_CALL(*client_, GetIamPolicy(_, _, _))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(mock_policy));

  std::string resource = "test-resource";
  auto policy = tested.GetNativeIamPolicy(resource);
  ASSERT_STATUS_OK(policy);
  EXPECT_EQ(3, policy->version());
  EXPECT_EQ("random-tag", policy->etag());
}

using MockAsyncIamPolicyReader =
    google::cloud::bigtable::testing::MockAsyncResponseReader<
        ::google::iam::v1::Policy>;

class AsyncGetIamPolicyTest : public ::testing::Test {
 public:
  AsyncGetIamPolicyTest()
      : cq_impl_(new MockCompletionQueue),
        cq_(cq_impl_),
        client_(new bigtable::testing::MockInstanceAdminClient),
        reader_(new MockAsyncIamPolicyReader) {
    using namespace ::testing;
    EXPECT_CALL(*client_, project()).WillRepeatedly(ReturnRef(kProjectId));
    EXPECT_CALL(*client_, AsyncGetIamPolicy(_, _, _))
        .WillOnce(Invoke([this](grpc::ClientContext* context,
                                ::google::iam::v1::GetIamPolicyRequest const&
                                    request,
                                grpc::CompletionQueue*) {
          EXPECT_STATUS_OK(google::cloud::bigtable::testing::IsContextMDValid(
              *context,
              "google.bigtable.admin.v2.BigtableInstanceAdmin.GetIamPolicy"));
          EXPECT_EQ("projects/the-project/instances/test-instance",
                    request.resource());
          // This is safe, see comments in MockAsyncResponseReader.
          return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
              ::google::iam::v1::Policy>>(reader_.get());
        }));
  }

 protected:
  void Start() {
    bigtable::InstanceAdmin instance_admin(client_);
    user_future_ = instance_admin.AsyncGetIamPolicy(cq_, "test-instance");
  }
  void StartNative() {
    bigtable::InstanceAdmin instance_admin(client_);
    user_native_future_ =
        instance_admin.AsyncGetNativeIamPolicy(cq_, "test-instance");
  }

  std::shared_ptr<MockCompletionQueue> cq_impl_;
  bigtable::CompletionQueue cq_;
  std::shared_ptr<bigtable::testing::MockInstanceAdminClient> client_;
  google::cloud::future<google::cloud::StatusOr<google::cloud::IamPolicy>>
      user_future_;
  google::cloud::future<google::cloud::StatusOr<google::iam::v1::Policy>>
      user_native_future_;
  std::unique_ptr<MockAsyncIamPolicyReader> reader_;
};

/// @test Verify that AsyncGetIamPolicy works in simple case.
TEST_F(AsyncGetIamPolicyTest, AsyncGetIamPolicy) {
  using ::testing::_;
  using ::testing::Invoke;
  namespace iamproto = ::google::iam::v1;
  bigtable::InstanceAdmin tested(client_);

  EXPECT_CALL(*reader_, Finish(_, _, _))
      .WillOnce(
          Invoke([](iamproto::Policy* response, grpc::Status* status, void*) {
            EXPECT_NE(nullptr, response);
            response->set_version(3);
            response->set_etag("random-tag");
            *status = grpc::Status::OK;
          }));

  Start();
  EXPECT_EQ(std::future_status::timeout, user_future_.wait_for(1_ms));
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);
  auto policy = user_future_.get();
  ASSERT_STATUS_OK(policy);
  EXPECT_EQ(3, policy->version);
  EXPECT_EQ("random-tag", policy->etag);
}

/// @test Test unrecoverable errors for InstanceAdmin::AsyncGetIamPolicy.
TEST_F(AsyncGetIamPolicyTest, AsyncGetIamPolicyUnrecoverableError) {
  using ::testing::_;
  using ::testing::Invoke;
  namespace iamproto = ::google::iam::v1;
  bigtable::InstanceAdmin tested(client_);

  EXPECT_CALL(*reader_, Finish(_, _, _))
      .WillOnce(
          Invoke([](iamproto::Policy* response, grpc::Status* status, void*) {
            EXPECT_NE(nullptr, response);
            *status = grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "nooo");
          }));

  Start();
  EXPECT_EQ(std::future_status::timeout, user_future_.wait_for(1_ms));
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);

  auto policy = user_future_.get();
  ASSERT_FALSE(policy);
  ASSERT_EQ(google::cloud::StatusCode::kPermissionDenied,
            policy.status().code());
}

/// @test Verify that AsyncGetNativeIamPolicy works in simple case.
TEST_F(AsyncGetIamPolicyTest, AsyncGetNativeIamPolicy) {
  using ::testing::_;
  using ::testing::Invoke;
  namespace iamproto = ::google::iam::v1;
  bigtable::InstanceAdmin tested(client_);

  EXPECT_CALL(*reader_, Finish(_, _, _))
      .WillOnce(
          Invoke([](iamproto::Policy* response, grpc::Status* status, void*) {
            EXPECT_NE(nullptr, response);
            response->set_version(3);
            response->set_etag("random-tag");
            *status = grpc::Status::OK;
          }));

  StartNative();
  EXPECT_EQ(std::future_status::timeout, user_native_future_.wait_for(1_ms));
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);
  auto policy = user_native_future_.get();
  ASSERT_STATUS_OK(policy);
  EXPECT_EQ(3, policy->version());
  EXPECT_EQ("random-tag", policy->etag());
}

/// @test Test unrecoverable errors for InstanceAdmin::AsyncGetNativeIamPolicy.
TEST_F(AsyncGetIamPolicyTest, AsyncGetNativeIamPolicyUnrecoverableError) {
  using ::testing::_;
  using ::testing::Invoke;
  namespace iamproto = ::google::iam::v1;
  bigtable::InstanceAdmin tested(client_);

  EXPECT_CALL(*reader_, Finish(_, _, _))
      .WillOnce(
          Invoke([](iamproto::Policy* response, grpc::Status* status, void*) {
            EXPECT_NE(nullptr, response);
            *status = grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "nooo");
          }));

  StartNative();
  EXPECT_EQ(std::future_status::timeout, user_native_future_.wait_for(1_ms));
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);

  auto policy = user_native_future_.get();
  ASSERT_FALSE(policy);
  ASSERT_EQ(google::cloud::StatusCode::kPermissionDenied,
            policy.status().code());
}

/// @test Verify positive scenario for InstanceAdmin::SetIamPolicy.
TEST_F(InstanceAdminTest, SetIamPolicy) {
  using ::testing::_;
  using ::testing::Invoke;

  bigtable::InstanceAdmin tested(client_);
  auto mock_policy = create_policy_with_params();
  EXPECT_CALL(*client_, SetIamPolicy(_, _, _)).WillOnce(Invoke(mock_policy));

  std::string resource = "test-resource";
  google::cloud::IamBindings iam_bindings =
      google::cloud::IamBindings("writer", {"abc@gmail.com", "xyz@gmail.com"});
  auto policy = tested.SetIamPolicy(resource, iam_bindings, "test-tag");
  ASSERT_STATUS_OK(policy);

  EXPECT_EQ(1, policy->bindings.size());
  EXPECT_EQ("test-tag", policy->etag);
}

/// @test Verify unrecoverable errors for InstanceAdmin::SetIamPolicy.
TEST_F(InstanceAdminTest, SetIamPolicyUnrecoverableError) {
  using ::testing::_;
  using ::testing::Return;

  bigtable::InstanceAdmin tested(client_);

  EXPECT_CALL(*client_, SetIamPolicy(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "err!")));

  std::string resource = "test-resource";
  google::cloud::IamBindings iam_bindings =
      google::cloud::IamBindings("writer", {"abc@gmail.com", "xyz@gmail.com"});
  EXPECT_FALSE(tested.SetIamPolicy(resource, iam_bindings, "test-tag"));
}

/// @test Verify recoverable errors for InstanceAdmin::SetIamPolicy.
TEST_F(InstanceAdminTest, SetIamPolicyRecoverableError) {
  using ::testing::_;
  using ::testing::Invoke;
  namespace iamproto = ::google::iam::v1;

  bigtable::InstanceAdmin tested(client_);

  auto mock_recoverable_failure = [](grpc::ClientContext* context,
                                     iamproto::SetIamPolicyRequest const&,
                                     iamproto::Policy*) {
    EXPECT_STATUS_OK(google::cloud::bigtable::testing::IsContextMDValid(
        *context,
        "google.bigtable.admin.v2.BigtableInstanceAdmin.SetIamPolicy"));
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
  };
  auto mock_policy = create_policy_with_params();

  EXPECT_CALL(*client_, SetIamPolicy(_, _, _))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(mock_policy));

  std::string resource = "test-resource";
  google::cloud::IamBindings iam_bindings =
      google::cloud::IamBindings("writer", {"abc@gmail.com", "xyz@gmail.com"});
  auto policy = tested.SetIamPolicy(resource, iam_bindings, "test-tag");
  ASSERT_STATUS_OK(policy);

  EXPECT_EQ(1, policy->bindings.size());
  EXPECT_EQ("test-tag", policy->etag);
}

/// @test Verify positive scenario for InstanceAdmin::SetIamPolicy (native).
TEST_F(InstanceAdminTest, SetNativeIamPolicy) {
  using ::testing::_;
  using ::testing::Invoke;

  bigtable::InstanceAdmin tested(client_);
  auto mock_policy = create_policy_with_params();
  EXPECT_CALL(*client_, SetIamPolicy(_, _, _)).WillOnce(Invoke(mock_policy));

  std::string resource = "test-resource";
  auto iam_policy = bigtable::IamPolicy(
      {bigtable::IamBinding("writer", {"abc@gmail.com", "xyz@gmail.com"})},
      "test-tag", 0);
  auto policy = tested.SetIamPolicy(resource, iam_policy);
  ASSERT_STATUS_OK(policy);

  EXPECT_EQ(1, policy->bindings().size());
  EXPECT_EQ("test-tag", policy->etag());
}

/// @test Verify unrecoverable errors for InstanceAdmin::SetIamPolicy (native).
TEST_F(InstanceAdminTest, SetNativeIamPolicyUnrecoverableError) {
  using ::testing::_;
  using ::testing::Return;

  bigtable::InstanceAdmin tested(client_);

  EXPECT_CALL(*client_, SetIamPolicy(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "err!")));

  std::string resource = "test-resource";
  auto iam_policy = bigtable::IamPolicy(
      {bigtable::IamBinding("writer", {"abc@gmail.com", "xyz@gmail.com"})},
      "test-tag", 0);
  EXPECT_FALSE(tested.SetIamPolicy(resource, iam_policy));
}

/// @test Verify recoverable errors for InstanceAdmin::SetIamPolicy (native).
TEST_F(InstanceAdminTest, SetNativeIamPolicyRecoverableError) {
  using ::testing::_;
  using ::testing::Invoke;
  namespace iamproto = ::google::iam::v1;

  bigtable::InstanceAdmin tested(client_);

  auto mock_recoverable_failure = [](grpc::ClientContext* context,
                                     iamproto::SetIamPolicyRequest const&,
                                     iamproto::Policy*) {
    EXPECT_STATUS_OK(google::cloud::bigtable::testing::IsContextMDValid(
        *context,
        "google.bigtable.admin.v2.BigtableInstanceAdmin.SetIamPolicy"));
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
  };
  auto mock_policy = create_policy_with_params();

  EXPECT_CALL(*client_, SetIamPolicy(_, _, _))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(mock_policy));

  std::string resource = "test-resource";
  auto iam_policy = bigtable::IamPolicy(
      {bigtable::IamBinding("writer", {"abc@gmail.com", "xyz@gmail.com"})},
      "test-tag", 0);
  auto policy = tested.SetIamPolicy(resource, iam_policy);
  ASSERT_STATUS_OK(policy);

  EXPECT_EQ(1, policy->bindings().size());
  EXPECT_EQ("test-tag", policy->etag());
}
/// @test Verify that InstanceAdmin::TestIamPermissions works in simple case.
TEST_F(InstanceAdminTest, TestIamPermissions) {
  using ::testing::_;
  using ::testing::Invoke;
  namespace iamproto = ::google::iam::v1;
  bigtable::InstanceAdmin tested(client_);

  auto mock_permission_set = [](grpc::ClientContext* context,
                                iamproto::TestIamPermissionsRequest const&,
                                iamproto::TestIamPermissionsResponse*
                                    response) {
    EXPECT_STATUS_OK(google::cloud::bigtable::testing::IsContextMDValid(
        *context,
        "google.bigtable.admin.v2.BigtableInstanceAdmin.TestIamPermissions"));
    EXPECT_NE(nullptr, response);
    std::vector<std::string> permissions = {"writer", "reader"};
    response->add_permissions(permissions[0]);
    response->add_permissions(permissions[1]);
    return grpc::Status::OK;
  };

  EXPECT_CALL(*client_, TestIamPermissions(_, _, _))
      .WillOnce(Invoke(mock_permission_set));

  std::string resource = "the-resource";
  auto permission_set =
      tested.TestIamPermissions(resource, {"reader", "writer", "owner"});
  ASSERT_STATUS_OK(permission_set);

  EXPECT_EQ(2, permission_set->size());
}

/// @test Test for unrecoverable errors for InstanceAdmin::TestIamPermissions.
TEST_F(InstanceAdminTest, TestIamPermissionsUnrecoverableError) {
  using ::testing::_;
  using ::testing::Return;

  bigtable::InstanceAdmin tested(client_);

  EXPECT_CALL(*client_, TestIamPermissions(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "err!")));

  std::string resource = "other-resource";

  EXPECT_FALSE(
      tested.TestIamPermissions(resource, {"reader", "writer", "owner"}));
}

/// @test Test for recoverable errors for InstanceAdmin::TestIamPermissions.
TEST_F(InstanceAdminTest, TestIamPermissionsRecoverableError) {
  using ::testing::_;
  using ::testing::Invoke;
  namespace iamproto = ::google::iam::v1;
  bigtable::InstanceAdmin tested(client_);

  auto mock_recoverable_failure = [](grpc::ClientContext* context,
                                     iamproto::TestIamPermissionsRequest const&,
                                     iamproto::TestIamPermissionsResponse*) {
    EXPECT_STATUS_OK(google::cloud::bigtable::testing::IsContextMDValid(
        *context,
        "google.bigtable.admin.v2.BigtableInstanceAdmin.TestIamPermissions"));
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
  };

  auto mock_permission_set = [](grpc::ClientContext* context,
                                iamproto::TestIamPermissionsRequest const&,
                                iamproto::TestIamPermissionsResponse*
                                    response) {
    EXPECT_STATUS_OK(google::cloud::bigtable::testing::IsContextMDValid(
        *context,
        "google.bigtable.admin.v2.BigtableInstanceAdmin.TestIamPermissions"));
    EXPECT_NE(nullptr, response);
    std::vector<std::string> permissions = {"writer", "reader"};
    response->add_permissions(permissions[0]);
    response->add_permissions(permissions[1]);
    return grpc::Status::OK;
  };
  EXPECT_CALL(*client_, TestIamPermissions(_, _, _))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(mock_permission_set));

  std::string resource = "the-resource";
  auto permission_set =
      tested.TestIamPermissions(resource, {"writer", "reader", "owner"});
  ASSERT_STATUS_OK(permission_set);

  EXPECT_EQ(2, permission_set->size());
}

using MockAsyncDeleteClusterReader =
    google::cloud::bigtable::testing::MockAsyncResponseReader<
        ::google::protobuf::Empty>;

class AsyncDeleteClusterTest : public ::testing::Test {
 public:
  AsyncDeleteClusterTest()
      : cq_impl_(new MockCompletionQueue),
        cq_(cq_impl_),
        client_(new bigtable::testing::MockInstanceAdminClient),
        reader_(new MockAsyncDeleteClusterReader) {
    using namespace ::testing;
    EXPECT_CALL(*client_, project()).WillRepeatedly(ReturnRef(kProjectId));
    EXPECT_CALL(*client_, AsyncDeleteCluster(_, _, _))
        .WillOnce(Invoke([this](grpc::ClientContext* context,
                                btadmin::DeleteClusterRequest const& request,
                                grpc::CompletionQueue*) {
          EXPECT_STATUS_OK(google::cloud::bigtable::testing::IsContextMDValid(
              *context,
              "google.bigtable.admin.v2.BigtableInstanceAdmin.DeleteCluster"));
          EXPECT_EQ(
              "projects/the-project/instances/test-instance/clusters/"
              "the-cluster",
              request.name());
          // This is safe, see comments in MockAsyncResponseReader.
          return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
              ::google::protobuf::Empty>>(reader_.get());
        }));
  }

 protected:
  void Start() {
    bigtable::InstanceAdmin instance_admin(client_);
    user_future_ =
        instance_admin.AsyncDeleteCluster(cq_, "test-instance", "the-cluster");
  }

  std::shared_ptr<MockCompletionQueue> cq_impl_;
  bigtable::CompletionQueue cq_;
  std::shared_ptr<bigtable::testing::MockInstanceAdminClient> client_;
  google::cloud::future<google::cloud::Status> user_future_;
  std::unique_ptr<MockAsyncDeleteClusterReader> reader_;
};

/// @test Verify that AsyncDeleteCluster works in simple case.
TEST_F(AsyncDeleteClusterTest, AsyncDeleteCluster) {
  using ::testing::_;
  using ::testing::Invoke;
  bigtable::InstanceAdmin tested(client_);

  EXPECT_CALL(*reader_, Finish(_, _, _))
      .WillOnce(Invoke(
          [](::google::protobuf::Empty* response, grpc::Status* status, void*) {
            EXPECT_NE(nullptr, response);
            *status = grpc::Status::OK;
          }));

  Start();
  EXPECT_EQ(std::future_status::timeout, user_future_.wait_for(1_ms));
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);
  auto status = user_future_.get();
  ASSERT_STATUS_OK(status);
}

/// @test Test unrecoverable errors for InstanceAdmin::AsyncDeleteCluster.
TEST_F(AsyncDeleteClusterTest, AsyncDeleteClusterUnrecoverableError) {
  using ::testing::_;
  using ::testing::Invoke;
  bigtable::InstanceAdmin tested(client_);

  EXPECT_CALL(*reader_, Finish(_, _, _))
      .WillOnce(Invoke(
          [](::google::protobuf::Empty* response, grpc::Status* status, void*) {
            EXPECT_NE(nullptr, response);
            *status = grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "nooo");
          }));

  Start();
  EXPECT_EQ(std::future_status::timeout, user_future_.wait_for(1_ms));
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);

  auto status = user_future_.get();
  ASSERT_EQ(google::cloud::StatusCode::kPermissionDenied, status.code());
}

using MockAsyncSetIamPolicyReader =
    google::cloud::bigtable::testing::MockAsyncResponseReader<
        ::google::iam::v1::Policy>;

class AsyncSetIamPolicyTest : public ::testing::Test {
 public:
  AsyncSetIamPolicyTest()
      : cq_impl_(new MockCompletionQueue),
        cq_(cq_impl_),
        client_(new bigtable::testing::MockInstanceAdminClient),
        reader_(new MockAsyncSetIamPolicyReader) {
    using namespace ::testing;
    EXPECT_CALL(*client_, project()).WillRepeatedly(ReturnRef(kProjectId));
    EXPECT_CALL(*client_, AsyncSetIamPolicy(_, _, _))
        .WillOnce(Invoke([this](grpc::ClientContext* context,
                                ::google::iam::v1::SetIamPolicyRequest const&
                                    request,
                                grpc::CompletionQueue*) {
          EXPECT_STATUS_OK(google::cloud::bigtable::testing::IsContextMDValid(
              *context,
              "google.bigtable.admin.v2.BigtableInstanceAdmin.SetIamPolicy"));
          EXPECT_EQ("projects/the-project/instances/test-instance",
                    request.resource());
          // This is safe, see comments in MockAsyncResponseReader.
          return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
              ::google::iam::v1::Policy>>(reader_.get());
        }));
  }

 protected:
  void Start() {
    bigtable::InstanceAdmin instance_admin(client_);
    user_future_ = instance_admin.AsyncSetIamPolicy(
        cq_, "test-instance",
        google::cloud::IamBindings("writer",
                                   {"abc@gmail.com", "xyz@gmail.com"}),
        "test-tag");
  }

  void StartNative() {
    bigtable::InstanceAdmin instance_admin(client_);
    user_native_future_ = instance_admin.AsyncSetIamPolicy(
        cq_, "test-instance",
        bigtable::IamPolicy({bigtable::IamBinding(
                                "writer", {"abc@gmail.com", "xyz@gmail.com"})},
                            "test-tag", 0));
  }

  std::shared_ptr<MockCompletionQueue> cq_impl_;
  bigtable::CompletionQueue cq_;
  std::shared_ptr<bigtable::testing::MockInstanceAdminClient> client_;
  google::cloud::future<google::cloud::StatusOr<google::cloud::IamPolicy>>
      user_future_;
  google::cloud::future<google::cloud::StatusOr<google::iam::v1::Policy>>
      user_native_future_;
  std::unique_ptr<MockAsyncSetIamPolicyReader> reader_;
};

/// @test Verify that AsyncSetIamPolicy works in simple case.
TEST_F(AsyncSetIamPolicyTest, AsyncSetIamPolicy) {
  using ::testing::_;
  using ::testing::Invoke;
  namespace iamproto = ::google::iam::v1;
  bigtable::InstanceAdmin tested(client_);

  EXPECT_CALL(*reader_, Finish(_, _, _))
      .WillOnce(
          Invoke([](iamproto::Policy* response, grpc::Status* status, void*) {
            EXPECT_NE(nullptr, response);

            auto new_binding = response->add_bindings();
            new_binding->set_role("writer");
            new_binding->add_members("abc@gmail.com");
            new_binding->add_members("xyz@gmail.com");
            response->set_etag("test-tag");
            *status = grpc::Status::OK;
          }));

  Start();
  EXPECT_EQ(std::future_status::timeout, user_future_.wait_for(1_ms));
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);
  auto policy = user_future_.get();
  ASSERT_STATUS_OK(policy);

  EXPECT_EQ(1, policy->bindings.size());
  EXPECT_EQ("test-tag", policy->etag);
}

/// @test Test unrecoverable errors for InstanceAdmin::AsyncSetIamPolicy.
TEST_F(AsyncSetIamPolicyTest, AsyncSetIamPolicyUnrecoverableError) {
  using ::testing::_;
  using ::testing::Invoke;
  namespace iamproto = ::google::iam::v1;
  bigtable::InstanceAdmin tested(client_);

  EXPECT_CALL(*reader_, Finish(_, _, _))
      .WillOnce(
          Invoke([](iamproto::Policy* response, grpc::Status* status, void*) {
            EXPECT_NE(nullptr, response);
            *status = grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "nooo");
          }));

  Start();
  EXPECT_EQ(std::future_status::timeout, user_future_.wait_for(1_ms));
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);

  auto policy = user_future_.get();
  ASSERT_FALSE(policy);
  ASSERT_EQ(google::cloud::StatusCode::kPermissionDenied,
            policy.status().code());
}

/// @test Verify that AsyncSetIamPolicy works in simple case (native).
TEST_F(AsyncSetIamPolicyTest, AsyncSetNativeIamPolicy) {
  using ::testing::_;
  using ::testing::Invoke;
  namespace iamproto = ::google::iam::v1;
  bigtable::InstanceAdmin tested(client_);

  EXPECT_CALL(*reader_, Finish(_, _, _))
      .WillOnce(
          Invoke([](iamproto::Policy* response, grpc::Status* status, void*) {
            EXPECT_NE(nullptr, response);

            auto new_binding = response->add_bindings();
            new_binding->set_role("writer");
            new_binding->add_members("abc@gmail.com");
            new_binding->add_members("xyz@gmail.com");
            response->set_etag("test-tag");
            *status = grpc::Status::OK;
          }));

  StartNative();
  EXPECT_EQ(std::future_status::timeout, user_native_future_.wait_for(1_ms));
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);
  auto policy = user_native_future_.get();
  ASSERT_STATUS_OK(policy);

  EXPECT_EQ(1, policy->bindings().size());
  EXPECT_EQ("test-tag", policy->etag());
}

/// @test Test unrecoverable errors for InstanceAdmin::AsyncSetIamPolicy native.
TEST_F(AsyncSetIamPolicyTest, AsyncSetNativeIamPolicyUnrecoverableError) {
  using ::testing::_;
  using ::testing::Invoke;
  namespace iamproto = ::google::iam::v1;
  bigtable::InstanceAdmin tested(client_);

  EXPECT_CALL(*reader_, Finish(_, _, _))
      .WillOnce(
          Invoke([](iamproto::Policy* response, grpc::Status* status, void*) {
            EXPECT_NE(nullptr, response);
            *status = grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "nooo");
          }));

  StartNative();
  EXPECT_EQ(std::future_status::timeout, user_native_future_.wait_for(1_ms));
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);

  auto policy = user_native_future_.get();
  ASSERT_FALSE(policy);
  ASSERT_EQ(google::cloud::StatusCode::kPermissionDenied,
            policy.status().code());
}

using MockAsyncTestIamPermissionsReader =
    google::cloud::bigtable::testing::MockAsyncResponseReader<
        ::google::iam::v1::TestIamPermissionsResponse>;

class AsyncTestIamPermissionsTest : public ::testing::Test {
 public:
  AsyncTestIamPermissionsTest()
      : cq_impl_(new MockCompletionQueue),
        cq_(cq_impl_),
        client_(new bigtable::testing::MockInstanceAdminClient),
        reader_(new MockAsyncTestIamPermissionsReader) {
    using namespace ::testing;
    EXPECT_CALL(*client_, project()).WillRepeatedly(ReturnRef(kProjectId));
    EXPECT_CALL(*client_, AsyncTestIamPermissions(_, _, _))
        .WillOnce(Invoke(
            [this](grpc::ClientContext* context,
                   ::google::iam::v1::TestIamPermissionsRequest const& request,
                   grpc::CompletionQueue*) {
              EXPECT_STATUS_OK(
                  google::cloud::bigtable::testing::IsContextMDValid(
                      *context,
                      "google.bigtable.admin.v2.BigtableInstanceAdmin."
                      "TestIamPermissions"));
              EXPECT_EQ("projects/the-project/instances/the-resource",
                        request.resource());
              // This is safe, see comments in MockAsyncResponseReader.
              return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                  ::google::iam::v1::TestIamPermissionsResponse>>(
                  reader_.get());
            }));
  }

 protected:
  void Start(std::vector<std::string> permissions) {
    bigtable::InstanceAdmin instance_admin(client_);
    user_future_ = instance_admin.AsyncTestIamPermissions(
        cq_, "the-resource", std::move(permissions));
  }

  std::shared_ptr<MockCompletionQueue> cq_impl_;
  bigtable::CompletionQueue cq_;
  std::shared_ptr<bigtable::testing::MockInstanceAdminClient> client_;
  google::cloud::future<google::cloud::StatusOr<std::vector<std::string>>>
      user_future_;
  std::unique_ptr<MockAsyncTestIamPermissionsReader> reader_;
};

/// @test Verify that AsyncTestIamPermissions works in simple case.
TEST_F(AsyncTestIamPermissionsTest, AsyncTestIamPermissions) {
  using ::testing::_;
  using ::testing::Invoke;
  namespace iamproto = ::google::iam::v1;
  bigtable::InstanceAdmin tested(client_);

  EXPECT_CALL(*reader_, Finish(_, _, _))
      .WillOnce(Invoke([](iamproto::TestIamPermissionsResponse* response,
                          grpc::Status* status, void*) {
        EXPECT_NE(nullptr, response);
        response->add_permissions("writer");
        response->add_permissions("reader");
        *status = grpc::Status::OK;
      }));

  Start({"reader", "writer", "owner"});
  EXPECT_EQ(std::future_status::timeout, user_future_.wait_for(1_ms));
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);
  auto permission_set = user_future_.get();
  ASSERT_STATUS_OK(permission_set);
  EXPECT_EQ(2, permission_set->size());
}

/// @test Test unrecoverable errors for InstanceAdmin::AsyncTestIamPermissions.
TEST_F(AsyncTestIamPermissionsTest, AsyncTestIamPermissionsUnrecoverableError) {
  using ::testing::_;
  using ::testing::Invoke;
  namespace iamproto = ::google::iam::v1;
  bigtable::InstanceAdmin tested(client_);

  EXPECT_CALL(*reader_, Finish(_, _, _))
      .WillOnce(Invoke([](iamproto::TestIamPermissionsResponse* response,
                          grpc::Status* status, void*) {
        EXPECT_NE(nullptr, response);
        *status = grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "nooo");
      }));

  Start({"reader", "writer", "owner"});
  EXPECT_EQ(std::future_status::timeout, user_future_.wait_for(1_ms));
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);

  auto permission_set = user_future_.get();
  ASSERT_FALSE(permission_set);
  ASSERT_EQ(google::cloud::StatusCode::kPermissionDenied,
            permission_set.status().code());
}

class ValidContextMdAsyncTest : public ::testing::Test {
 public:
  ValidContextMdAsyncTest()
      : cq_impl_(new MockCompletionQueue),
        cq_(cq_impl_),
        client_(new bigtable::testing::MockInstanceAdminClient) {
    EXPECT_CALL(*client_, project())
        .WillRepeatedly(::testing::ReturnRef(kProjectId));
    instance_admin_ =
        google::cloud::internal::make_unique<bigtable::InstanceAdmin>(client_);
  }

 protected:
  template <typename ResultType>
  void FinishTest(ResultType res_future) {
    EXPECT_EQ(1U, cq_impl_->size());
    cq_impl_->SimulateCompletion(true);
    EXPECT_EQ(0U, cq_impl_->size());
    auto res = res_future.get();
    EXPECT_FALSE(res);
    EXPECT_EQ(google::cloud::StatusCode::kPermissionDenied,
              res.status().code());
  }

  std::shared_ptr<MockCompletionQueue> cq_impl_;
  bigtable::CompletionQueue cq_;
  std::shared_ptr<bigtable::testing::MockInstanceAdminClient> client_;
  std::unique_ptr<bigtable::InstanceAdmin> instance_admin_;
};

TEST_F(ValidContextMdAsyncTest, AsyncCreateAppProfile) {
  using ::testing::_;
  bigtable::testing::MockAsyncFailingRpcFactory<
      btadmin::CreateAppProfileRequest, btadmin::AppProfile>
      rpc_factory;
  EXPECT_CALL(*client_, AsyncCreateAppProfile(_, _, _))
      .WillOnce(::testing::Invoke(rpc_factory.Create(
          R"""(
              parent: "projects/the-project/instances/the-instance"
              app_profile_id: "prof"
              app_profile: { multi_cluster_routing_use_any { } }
          )""",
          "google.bigtable.admin.v2.BigtableInstanceAdmin.CreateAppProfile")));
  FinishTest(instance_admin_->AsyncCreateAppProfile(
      cq_, "the-instance",
      bigtable::AppProfileConfig::MultiClusterUseAny("prof")));
}

TEST_F(ValidContextMdAsyncTest, AsyncDeleteAppProfile) {
  using ::testing::_;
  bigtable::testing::MockAsyncFailingRpcFactory<
      btadmin::DeleteAppProfileRequest, google::protobuf::Empty>
      rpc_factory;
  EXPECT_CALL(*client_, AsyncDeleteAppProfile(_, _, _))
      .WillOnce(::testing::Invoke(rpc_factory.Create(
          R"""(
              name: "projects/the-project/instances/the-instance/appProfiles/the-profile"
              ignore_warnings: true
          )""",
          "google.bigtable.admin.v2.BigtableInstanceAdmin.DeleteAppProfile")));
  auto res_future = instance_admin_->AsyncDeleteAppProfile(cq_, "the-instance",
                                                           "the-profile");
  EXPECT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);
  EXPECT_EQ(0U, cq_impl_->size());
  auto res = res_future.get();
  EXPECT_EQ(google::cloud::StatusCode::kPermissionDenied, res.code());
}

TEST_F(ValidContextMdAsyncTest, AsyncDeleteInstance) {
  using ::testing::_;
  bigtable::testing::MockAsyncFailingRpcFactory<btadmin::DeleteInstanceRequest,
                                                google::protobuf::Empty>
      rpc_factory;
  EXPECT_CALL(*client_, AsyncDeleteInstance(_, _, _))
      .WillOnce(::testing::Invoke(rpc_factory.Create(
          R"""(
              name: "projects/the-project/instances/the-instance"
          )""",
          "google.bigtable.admin.v2.BigtableInstanceAdmin.DeleteInstance")));
  auto res_future = instance_admin_->AsyncDeleteInstance("the-instance", cq_);
  EXPECT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);
  EXPECT_EQ(0U, cq_impl_->size());
  auto res = res_future.get();
  EXPECT_EQ(google::cloud::StatusCode::kPermissionDenied, res.code());
}

TEST_F(ValidContextMdAsyncTest, AsyncGetAppProfile) {
  using ::testing::_;
  bigtable::testing::MockAsyncFailingRpcFactory<btadmin::GetAppProfileRequest,
                                                btadmin::AppProfile>
      rpc_factory;
  EXPECT_CALL(*client_, AsyncGetAppProfile(_, _, _))
      .WillOnce(::testing::Invoke(rpc_factory.Create(
          R"""(
              name: "projects/the-project/instances/the-instance/appProfiles/the-profile"
          )""",
          "google.bigtable.admin.v2.BigtableInstanceAdmin.GetAppProfile")));
  FinishTest(
      instance_admin_->AsyncGetAppProfile(cq_, "the-instance", "the-profile"));
}

TEST_F(ValidContextMdAsyncTest, AsyncGetCluster) {
  using ::testing::_;
  bigtable::testing::MockAsyncFailingRpcFactory<btadmin::GetClusterRequest,
                                                btadmin::Cluster>
      rpc_factory;
  EXPECT_CALL(*client_, AsyncGetCluster(_, _, _))
      .WillOnce(::testing::Invoke(rpc_factory.Create(
          R"""(
              name: "projects/the-project/instances/the-instance/clusters/the-cluster"
          )""",
          "google.bigtable.admin.v2.BigtableInstanceAdmin.GetCluster")));
  FinishTest(
      instance_admin_->AsyncGetCluster(cq_, "the-instance", "the-cluster"));
}

TEST_F(ValidContextMdAsyncTest, AsyncGetInstance) {
  using ::testing::_;
  bigtable::testing::MockAsyncFailingRpcFactory<btadmin::GetInstanceRequest,
                                                btadmin::Instance>
      rpc_factory;
  EXPECT_CALL(*client_, AsyncGetInstance(_, _, _))
      .WillOnce(::testing::Invoke(rpc_factory.Create(
          R"""(
              name: "projects/the-project/instances/the-instance"
          )""",
          "google.bigtable.admin.v2.BigtableInstanceAdmin.GetInstance")));
  FinishTest(instance_admin_->AsyncGetInstance(cq_, "the-instance"));
}

TEST_F(ValidContextMdAsyncTest, AsyncCreateCluster) {
  using ::testing::_;
  bigtable::testing::MockAsyncFailingRpcFactory<btadmin::CreateClusterRequest,
                                                google::longrunning::Operation>
      rpc_factory;
  EXPECT_CALL(*client_, AsyncCreateCluster(_, _, _))
      .WillOnce(::testing::Invoke(rpc_factory.Create(
          R"""(
              parent: "projects/the-project/instances/the-instance"
              cluster_id: "the-cluster"
              cluster: {
                  location: "projects/the-project/locations/loc1"
                  serve_nodes: 3
                  default_storage_type: SSD
              }
          )""",
          "google.bigtable.admin.v2.BigtableInstanceAdmin.CreateCluster")));
  FinishTest(instance_admin_->AsyncCreateCluster(
      cq_, bigtable::ClusterConfig("loc1", 3, bigtable::ClusterConfig::SSD),
      "the-instance", "the-cluster"));
}

TEST_F(ValidContextMdAsyncTest, AsyncCreateInstance) {
  using ::testing::_;
  bigtable::testing::MockAsyncFailingRpcFactory<btadmin::CreateInstanceRequest,
                                                google::longrunning::Operation>
      rpc_factory;
  EXPECT_CALL(*client_, AsyncCreateInstance(_, _, _))
      .WillOnce(::testing::Invoke(rpc_factory.Create(
          R"""(
              parent: "projects/the-project"
              instance_id: "the-instance"
              instance: { display_name: "Displayed instance" }
          )""",
          "google.bigtable.admin.v2.BigtableInstanceAdmin.CreateInstance")));
  FinishTest(instance_admin_->AsyncCreateInstance(
      cq_, bigtable::InstanceConfig("the-instance", "Displayed instance", {})));
}

TEST_F(ValidContextMdAsyncTest, AsyncUpdateAppProfile) {
  using ::testing::_;
  bigtable::testing::MockAsyncFailingRpcFactory<
      btadmin::UpdateAppProfileRequest, google::longrunning::Operation>
      rpc_factory;
  EXPECT_CALL(*client_, AsyncUpdateAppProfile(_, _, _))
      .WillOnce(::testing::Invoke(rpc_factory.Create(
          R"""(
              app_profile: {
                  name: "projects/the-project/instances/the-instance/appProfiles/the-profile"
              }
          )""",
          "google.bigtable.admin.v2.BigtableInstanceAdmin.UpdateAppProfile")));
  FinishTest(instance_admin_->AsyncUpdateAppProfile(
      cq_, "the-instance", "the-profile", bigtable::AppProfileUpdateConfig()));
}

TEST_F(ValidContextMdAsyncTest, AsyncUpdateCluster) {
  using ::testing::_;
  bigtable::testing::MockAsyncFailingRpcFactory<btadmin::Cluster,
                                                google::longrunning::Operation>
      rpc_factory;
  EXPECT_CALL(*client_, AsyncUpdateCluster(_, _, _))
      .WillOnce(::testing::Invoke(rpc_factory.Create(
          R"""(
              location: "loc1"
              serve_nodes: 3
              default_storage_type: SSD
              name: "projects/the-project/instances/the-instance/clusters/the-cluster"
          )""",
          "google.bigtable.admin.v2.BigtableInstanceAdmin.UpdateCluster")));
  auto cluster =
      bigtable::ClusterConfig("loc1", 3, bigtable::ClusterConfig::SSD)
          .as_proto();
  cluster.set_name(
      "projects/the-project/instances/the-instance/clusters/the-cluster");
  FinishTest(instance_admin_->AsyncUpdateCluster(
      cq_, bigtable::ClusterConfig(std::move(cluster))));
}

TEST_F(ValidContextMdAsyncTest, AsyncUpdateInstance) {
  using ::testing::_;
  bigtable::testing::MockAsyncFailingRpcFactory<
      btadmin::PartialUpdateInstanceRequest, google::longrunning::Operation>
      rpc_factory;
  EXPECT_CALL(*client_, AsyncUpdateInstance(_, _, _))
      .WillOnce(::testing::Invoke(rpc_factory.Create(
          R"""(
              instance: {
                  name: "projects/the-project/instances/the-instance"
              }
          )""",
          "google.bigtable.admin.v2.BigtableInstanceAdmin."
          "PartialUpdateInstance")));
  bigtable::Instance instance;
  instance.set_name("projects/the-project/instances/the-instance");
  FinishTest(instance_admin_->AsyncUpdateInstance(
      cq_, bigtable::InstanceUpdateConfig(std::move(instance))));
}

TEST_F(InstanceAdminTest, CreateAppProfile) {
  using namespace ::testing;
  bigtable::InstanceAdmin tested(client_);
  std::string expected_text = R"""(
      parent: "projects/the-project/instances/the-instance"
      app_profile_id: "prof"
      app_profile: { multi_cluster_routing_use_any { } }
      )""";
  auto mock = MockRpcFactory<btadmin::CreateAppProfileRequest,
                             btadmin::AppProfile>::
      Create(expected_text,
             "google.bigtable.admin.v2.BigtableInstanceAdmin.CreateAppProfile");
  EXPECT_CALL(*client_, CreateAppProfile(_, _, _)).WillOnce(Invoke(mock));
  ASSERT_STATUS_OK(tested.CreateAppProfile(
      "the-instance", bigtable::AppProfileConfig::MultiClusterUseAny("prof")));
}

TEST_F(InstanceAdminTest, DeleteAppProfile) {
  using namespace ::testing;
  using google::protobuf::Empty;
  bigtable::InstanceAdmin tested(client_);
  std::string expected_text = R"""(
      name: "projects/the-project/instances/the-instance/appProfiles/the-profile"
      ignore_warnings: true
      )""";
  auto mock = MockRpcFactory<btadmin::DeleteAppProfileRequest, Empty>::Create(
      expected_text,
      "google.bigtable.admin.v2.BigtableInstanceAdmin.DeleteAppProfile");
  EXPECT_CALL(*client_, DeleteAppProfile(_, _, _)).WillOnce(Invoke(mock));
  ASSERT_STATUS_OK(tested.DeleteAppProfile("the-instance", "the-profile"));
}

TEST_F(InstanceAdminTest, GetAppProfile) {
  using namespace ::testing;
  using google::protobuf::Empty;
  bigtable::InstanceAdmin tested(client_);
  std::string expected_text = R"""(
      name: "projects/the-project/instances/the-instance/appProfiles/the-profile"
      )""";
  auto mock =
      MockRpcFactory<btadmin::GetAppProfileRequest, btadmin::AppProfile>::
          Create(
              expected_text,
              "google.bigtable.admin.v2.BigtableInstanceAdmin.GetAppProfile");
  EXPECT_CALL(*client_, GetAppProfile(_, _, _)).WillOnce(Invoke(mock));
  ASSERT_STATUS_OK(tested.GetAppProfile("the-instance", "the-profile"));
}

TEST_F(InstanceAdminTest, GetInstance) {
  using namespace ::testing;
  using google::protobuf::Empty;
  bigtable::InstanceAdmin tested(client_);
  std::string expected_text = R"""(
      name: "projects/the-project/instances/the-instance"
      )""";
  auto mock =
      MockRpcFactory<btadmin::GetInstanceRequest, btadmin::Instance>::Create(
          expected_text,
          "google.bigtable.admin.v2.BigtableInstanceAdmin.GetInstance");
  EXPECT_CALL(*client_, GetInstance(_, _, _)).WillOnce(Invoke(mock));
  ASSERT_STATUS_OK(tested.GetInstance("the-instance"));
}
