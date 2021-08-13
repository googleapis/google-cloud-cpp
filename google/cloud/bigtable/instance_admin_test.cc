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
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/fake_completion_queue_impl.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/mock_async_response_reader.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include "absl/memory/memory.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
// TODO(#5929) - remove after deprecation is completed
#include "google/cloud/internal/disable_deprecation_warnings.inc"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace {

namespace btadmin = ::google::bigtable::admin::v2;

using ::google::cloud::testing_util::FakeCompletionQueueImpl;
using ::google::cloud::testing_util::IsContextMDValid;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AtLeast;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::Return;
using ::testing::ReturnRef;

using MockAdminClient =
    ::google::cloud::bigtable::testing::MockInstanceAdminClient;

std::string const kProjectId = "the-project";

/// A fixture for the bigtable::InstanceAdmin tests.
class InstanceAdminTest : public ::testing::Test {
 protected:
  void SetUp() override {
    EXPECT_CALL(*client_, project()).WillRepeatedly(ReturnRef(kProjectId));
  }

  std::shared_ptr<MockAdminClient> client_ =
      std::make_shared<MockAdminClient>();
};

// A lambda to create lambdas.  Basically we would be rewriting the same
// lambda twice without this thing.
auto create_list_instances_lambda =
    [](std::string const& expected_token, std::string const& returned_token,
       std::vector<std::string> const& instance_ids) {
      return [expected_token, returned_token, instance_ids](
                 grpc::ClientContext* context,
                 btadmin::ListInstancesRequest const& request,
                 btadmin::ListInstancesResponse* response) {
        EXPECT_STATUS_OK(IsContextMDValid(
            *context,
            "google.bigtable.admin.v2.BigtableInstanceAdmin.ListInstances",
            google::cloud::internal::ApiClientHeader()));
        auto const project_name = "projects/" + kProjectId;
        EXPECT_EQ(project_name, request.parent());
        EXPECT_EQ(expected_token, request.page_token());

        EXPECT_NE(nullptr, response);
        for (auto const& instance_id : instance_ids) {
          auto& instance = *response->add_instances();
          // NOLINTNEXTLINE(performance-inefficient-string-concatenation)
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
    EXPECT_STATUS_OK(IsContextMDValid(
        *context, "google.bigtable.admin.v2.BigtableInstanceAdmin.GetCluster",
        google::cloud::internal::ApiClientHeader()));
    EXPECT_NE(nullptr, response);
    response->set_name(request.name());
    return grpc::Status::OK;
  };
};

auto create_get_policy_mock = []() {
  return [](grpc::ClientContext* context,
            ::google::iam::v1::GetIamPolicyRequest const&,
            ::google::iam::v1::Policy* response) {
    EXPECT_STATUS_OK(IsContextMDValid(
        *context, "google.bigtable.admin.v2.BigtableInstanceAdmin.GetIamPolicy",
        google::cloud::internal::ApiClientHeader()));
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
        *context, "google.bigtable.admin.v2.BigtableInstanceAdmin.SetIamPolicy",
        google::cloud::internal::ApiClientHeader()));
    EXPECT_NE(nullptr, response);
    *response = request.policy();
    return grpc::Status::OK;
  };
};

// A lambda to create lambdas.  Basically we would be rewriting the same
// lambda twice without this thing.
auto create_list_clusters_lambda =
    [](std::string const& expected_token, std::string const& returned_token,
       std::string const& instance_id,
       std::vector<std::string> const& cluster_ids) {
      return [expected_token, returned_token, instance_id, cluster_ids](
                 grpc::ClientContext* context,
                 btadmin::ListClustersRequest const& request,
                 btadmin::ListClustersResponse* response) {
        EXPECT_STATUS_OK(IsContextMDValid(
            *context,
            "google.bigtable.admin.v2.BigtableInstanceAdmin.ListClusters",
            google::cloud::internal::ApiClientHeader()));
        auto const instance_name =
            "projects/" + kProjectId + "/instances/" + instance_id;
        EXPECT_EQ(instance_name, request.parent());
        EXPECT_EQ(expected_token, request.page_token());

        EXPECT_NE(nullptr, response);
        for (auto const& cluster_id : cluster_ids) {
          auto& cluster = *response->add_clusters();
          // NOLINTNEXTLINE(performance-inefficient-string-concatenation)
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
  static std::function<SignatureType> Create(
      std::string const& expected_request, std::string const& method) {
    return std::function<SignatureType>(
        [expected_request, method](grpc::ClientContext* context,
                                   RequestType const& request,
                                   ResponseType* response) {
          EXPECT_STATUS_OK(IsContextMDValid(
              *context, method, google::cloud::internal::ApiClientHeader()));
          RequestType expected;
          // Cannot use ASSERT_TRUE() here, it has an embedded "return;"
          EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(
              expected_request, &expected));
          EXPECT_THAT(expected, IsProtoEqual(request));

          EXPECT_NE(nullptr, response);
          return grpc::Status::OK;
        });
  }
};

/// @test Verify basic functionality in the `bigtable::InstanceAdmin` class.
TEST_F(InstanceAdminTest, Default) {
  InstanceAdmin tested(client_);
  EXPECT_EQ("the-project", tested.project_id());
}

TEST_F(InstanceAdminTest, CopyConstructor) {
  InstanceAdmin source(client_);
  std::string const& expected = source.project_id();
  // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
  InstanceAdmin copy(source);
  EXPECT_EQ(expected, copy.project_id());
}

TEST_F(InstanceAdminTest, MoveConstructor) {
  InstanceAdmin source(client_);
  std::string expected = source.project_id();
  InstanceAdmin copy(std::move(source));
  EXPECT_EQ(expected, copy.project_id());
}

TEST_F(InstanceAdminTest, CopyAssignment) {
  std::shared_ptr<MockAdminClient> other_client =
      std::make_shared<MockAdminClient>();
  std::string other_project = "other-project";
  EXPECT_CALL(*other_client, project())
      .WillRepeatedly(ReturnRef(other_project));

  InstanceAdmin source(client_);
  std::string const& expected = source.project_id();
  InstanceAdmin dest(other_client);
  EXPECT_NE(expected, dest.project_id());
  dest = source;
  EXPECT_EQ(expected, dest.project_id());
}

TEST_F(InstanceAdminTest, MoveAssignment) {
  std::shared_ptr<MockAdminClient> other_client =
      std::make_shared<MockAdminClient>();
  std::string other_project = "other-project";
  EXPECT_CALL(*other_client, project())
      .WillRepeatedly(ReturnRef(other_project));

  InstanceAdmin source(client_);
  std::string expected = source.project_id();
  InstanceAdmin dest(other_client);
  EXPECT_NE(expected, dest.project_id());
  dest = std::move(source);
  EXPECT_EQ(expected, dest.project_id());
}

/// @test Verify that `bigtable::InstanceAdmin::ListInstances` works in the easy
/// case.
TEST_F(InstanceAdminTest, ListInstances) {
  InstanceAdmin tested(client_);
  auto mock_list_instances = create_list_instances_lambda("", "", {"t0", "t1"});
  EXPECT_CALL(*client_, ListInstances).WillOnce(mock_list_instances);

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
  InstanceAdmin tested(client_);
  auto mock_recoverable_failure = [](grpc::ClientContext* context,
                                     btadmin::ListInstancesRequest const&,
                                     btadmin::ListInstancesResponse*) {
    EXPECT_STATUS_OK(IsContextMDValid(
        *context,
        "google.bigtable.admin.v2.BigtableInstanceAdmin.ListInstances",
        google::cloud::internal::ApiClientHeader()));
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
  };
  auto batch0 = create_list_instances_lambda("", "token-001", {"t0", "t1"});
  auto batch1 = create_list_instances_lambda("token-001", "", {"t2", "t3"});
  EXPECT_CALL(*client_, ListInstances)
      .WillOnce(mock_recoverable_failure)
      .WillOnce(batch0)
      .WillOnce(mock_recoverable_failure)
      .WillOnce(mock_recoverable_failure)
      .WillOnce(batch1);

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
  InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, ListInstances)
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

  // After all the setup, make the actual call we want to test.
  EXPECT_FALSE(tested.ListInstances());
}

/**
 * @test Verify that `bigtable::InstanceAdmin::ListInstances` handles
 * too many transient failures.
 */
TEST_F(InstanceAdminTest, ListInstancesTooManyTransients) {
  using ms = std::chrono::milliseconds;
  InstanceAdmin tested(client_, LimitedErrorCountRetryPolicy(3),
                       ExponentialBackoffPolicy(ms(1), ms(2)));
  EXPECT_CALL(*client_, ListInstances)
      .Times(AtLeast(2))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")));

  // After all the setup, make the actual call we want to test.
  EXPECT_THAT(tested.ListInstances(),
              StatusIs(StatusCode::kUnavailable, HasSubstr("try-again")));
}

/// @test Verify that `bigtable::DeleteInstance` works in the positive case.
TEST_F(InstanceAdminTest, DeleteInstance) {
  using ::google::protobuf::Empty;
  InstanceAdmin tested(client_);
  std::string expected_text = R"""(
  name: 'projects/the-project/instances/the-instance'
      )""";
  auto mock = MockRpcFactory<btadmin::DeleteInstanceRequest, Empty>::Create(
      expected_text,
      "google.bigtable.admin.v2.BigtableInstanceAdmin.DeleteInstance");
  EXPECT_CALL(*client_, DeleteInstance).WillOnce(mock);
  // After all the setup, make the actual call we want to test.
  ASSERT_STATUS_OK(tested.DeleteInstance("the-instance"));
}

/// @test Verify unrecoverable error for DeleteInstance
TEST_F(InstanceAdminTest, DeleteInstanceUnrecoverableError) {
  InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, DeleteInstance)
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));
  // After all the setup, make the actual call we want to test.
  EXPECT_THAT(tested.DeleteInstance("other-instance"), Not(IsOk()));
}

/// @test Verify that recoverable error for DeleteInstance
TEST_F(InstanceAdminTest, DeleteInstanceRecoverableError) {
  InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, DeleteInstance)
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")));

  // After all the setup, make the actual call we want to test.
  EXPECT_THAT(tested.DeleteInstance("other-instance"), Not(IsOk()));
}

/// @test Verify that `bigtable::InstanceAdmin::ListClusters` works in the easy
/// case.
TEST_F(InstanceAdminTest, ListClusters) {
  InstanceAdmin tested(client_);
  std::string const& instance_id = "the-instance";
  auto mock_list_clusters =
      create_list_clusters_lambda("", "", instance_id, {"t0", "t1"});
  EXPECT_CALL(*client_, ListClusters).WillOnce(mock_list_clusters);

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
  InstanceAdmin tested(client_);
  auto mock_recoverable_failure = [](grpc::ClientContext* context,
                                     btadmin::ListClustersRequest const&,
                                     btadmin::ListClustersResponse*) {
    EXPECT_STATUS_OK(IsContextMDValid(
        *context, "google.bigtable.admin.v2.BigtableInstanceAdmin.ListClusters",
        google::cloud::internal::ApiClientHeader()));
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
  };
  std::string const& instance_id = "the-instance";
  auto batch0 =
      create_list_clusters_lambda("", "token-001", instance_id, {"t0", "t1"});
  auto batch1 =
      create_list_clusters_lambda("token-001", "", instance_id, {"t2", "t3"});
  EXPECT_CALL(*client_, ListClusters)
      .WillOnce(mock_recoverable_failure)
      .WillOnce(batch0)
      .WillOnce(mock_recoverable_failure)
      .WillOnce(mock_recoverable_failure)
      .WillOnce(batch1);

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
  InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, ListClusters)
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));

  std::string const& instance_id = "the-instance";
  // After all the setup, make the actual call we want to test.
  EXPECT_FALSE(tested.ListClusters(instance_id));
}

/// @test Verify positive scenario for `bigtable::GetCluster`
TEST_F(InstanceAdminTest, GetCluster) {
  InstanceAdmin tested(client_);
  auto mock = create_get_cluster_mock();
  EXPECT_CALL(*client_, GetCluster).WillOnce(mock);
  // After all the setup, make the actual call we want to test.
  auto cluster = tested.GetCluster("the-instance", "the-cluster");
  ASSERT_STATUS_OK(cluster);
  EXPECT_EQ("projects/the-project/instances/the-instance/clusters/the-cluster",
            cluster->name());
}

/// @test Verify unrecoverable error for GetCluster
TEST_F(InstanceAdminTest, GetClusterUnrecoverableError) {
  InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, GetCluster)
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));
  ASSERT_THAT(tested.GetCluster("other-instance", "the-cluster"), Not(IsOk()));
}

/// @test Verify recoverable errors for GetCluster
TEST_F(InstanceAdminTest, GetClusterRecoverableError) {
  InstanceAdmin tested(client_);
  auto mock_recoverable_failure = [](grpc::ClientContext* context,
                                     btadmin::GetClusterRequest const&,
                                     btadmin::Cluster*) {
    EXPECT_STATUS_OK(IsContextMDValid(
        *context, "google.bigtable.admin.v2.BigtableInstanceAdmin.GetCluster",
        google::cloud::internal::ApiClientHeader()));
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
  };

  auto mock_cluster = create_get_cluster_mock();
  EXPECT_CALL(*client_, GetCluster)
      .WillOnce(mock_recoverable_failure)
      .WillOnce(mock_recoverable_failure)
      .WillOnce(mock_cluster);

  // After all the setup, make the actual call we want to test.
  auto cluster = tested.GetCluster("the-instance", "the-cluster");
  ASSERT_STATUS_OK(cluster);
  EXPECT_EQ("projects/the-project/instances/the-instance/clusters/the-cluster",
            cluster->name());
}

/// @test Verify that DeleteCluster works in the positive case.
TEST_F(InstanceAdminTest, DeleteCluster) {
  using ::google::protobuf::Empty;
  InstanceAdmin tested(client_);
  std::string expected_text = R"""(
  name: 'projects/the-project/instances/the-instance/clusters/the-cluster'
      )""";
  auto mock = MockRpcFactory<btadmin::DeleteClusterRequest, Empty>::Create(
      expected_text,
      "google.bigtable.admin.v2.BigtableInstanceAdmin.DeleteCluster");
  EXPECT_CALL(*client_, DeleteCluster).WillOnce(mock);
  // After all the setup, make the actual call we want to test.
  ASSERT_STATUS_OK(tested.DeleteCluster("the-instance", "the-cluster"));
}

/// @test Verify unrecoverable error for DeleteCluster
TEST_F(InstanceAdminTest, DeleteClusterUnrecoverableError) {
  InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, DeleteCluster)
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));
  // After all the setup, make the actual call we want to test.
  EXPECT_THAT(tested.DeleteCluster("other-instance", "other-cluster"),
              Not(IsOk()));
}

/// @test Verify that recoverable error for DeleteCluster
TEST_F(InstanceAdminTest, DeleteClusterRecoverableError) {
  InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, DeleteCluster)
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")));

  // After all the setup, make the actual call we want to test.
  EXPECT_THAT(tested.DeleteCluster("other-instance", "other-cluster"),
              Not(IsOk()));
}

/// @test Verify positive scenario for InstanceAdmin::GetIamPolicy.
TEST_F(InstanceAdminTest, GetIamPolicy) {
  InstanceAdmin tested(client_);
  auto mock_policy = create_get_policy_mock();
  EXPECT_CALL(*client_, GetIamPolicy).WillOnce(mock_policy);

  std::string resource = "test-resource";
  tested.GetIamPolicy(resource);
}

/// @test Verify that IamPolicies with conditions cause failures.
TEST_F(InstanceAdminTest, GetIamPolicyWithConditionsFails) {
  InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, GetIamPolicy)
      .WillOnce([](grpc::ClientContext* context,
                   ::google::iam::v1::GetIamPolicyRequest const&,
                   ::google::iam::v1::Policy* response) {
        EXPECT_STATUS_OK(IsContextMDValid(
            *context,
            "google.bigtable.admin.v2.BigtableInstanceAdmin.GetIamPolicy",
            google::cloud::internal::ApiClientHeader()));
        EXPECT_NE(nullptr, response);
        response->set_version(3);
        response->set_etag("random-tag");
        auto& new_binding = *response->add_bindings();
        new_binding.set_role("writer");
        new_binding.add_members("abc@gmail.com");
        new_binding.add_members("xyz@gmail.com");
        *new_binding.mutable_condition() = google::type::Expr{};

        return grpc::Status::OK;
      });

  std::string resource = "test-resource";
  auto res = tested.GetIamPolicy(resource);
  ASSERT_THAT(res, StatusIs(StatusCode::kUnimplemented));
}

/// @test Verify unrecoverable errors for InstanceAdmin::GetIamPolicy.
TEST_F(InstanceAdminTest, GetIamPolicyUnrecoverableError) {
  InstanceAdmin tested(client_);

  EXPECT_CALL(*client_, GetIamPolicy)
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "err!")));

  std::string resource = "other-resource";

  EXPECT_FALSE(tested.GetIamPolicy(resource));
}

/// @test Verify recoverable errors for InstanceAdmin::GetIamPolicy.
TEST_F(InstanceAdminTest, GetIamPolicyRecoverableError) {
  namespace iamproto = ::google::iam::v1;

  InstanceAdmin tested(client_);

  auto mock_recoverable_failure = [](grpc::ClientContext* context,
                                     iamproto::GetIamPolicyRequest const&,
                                     iamproto::Policy*) {
    EXPECT_STATUS_OK(IsContextMDValid(
        *context, "google.bigtable.admin.v2.BigtableInstanceAdmin.GetIamPolicy",
        google::cloud::internal::ApiClientHeader()));
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
  };
  auto mock_policy = create_get_policy_mock();

  EXPECT_CALL(*client_, GetIamPolicy)
      .WillOnce(mock_recoverable_failure)
      .WillOnce(mock_policy);

  std::string resource = "test-resource";
  tested.GetIamPolicy(resource);
}

/// @test Verify positive scenario for InstanceAdmin::GetNativeIamPolicy.
TEST_F(InstanceAdminTest, GetNativeIamPolicy) {
  InstanceAdmin tested(client_);
  auto mock_policy = create_get_policy_mock();
  EXPECT_CALL(*client_, GetIamPolicy).WillOnce(mock_policy);

  std::string resource = "test-resource";
  auto policy = tested.GetNativeIamPolicy(resource);
  ASSERT_STATUS_OK(policy);
  EXPECT_EQ(3, policy->version());
  EXPECT_EQ("random-tag", policy->etag());
}

/// @test Verify unrecoverable errors for InstanceAdmin::GetNativeIamPolicy.
TEST_F(InstanceAdminTest, GetNativeIamPolicyUnrecoverableError) {
  InstanceAdmin tested(client_);

  EXPECT_CALL(*client_, GetIamPolicy)
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "err!")));

  std::string resource = "other-resource";

  EXPECT_FALSE(tested.GetNativeIamPolicy(resource));
}

/// @test Verify recoverable errors for InstanceAdmin::GetNativeIamPolicy.
TEST_F(InstanceAdminTest, GetNativeIamPolicyRecoverableError) {
  namespace iamproto = ::google::iam::v1;

  InstanceAdmin tested(client_);

  auto mock_recoverable_failure = [](grpc::ClientContext* context,
                                     iamproto::GetIamPolicyRequest const&,
                                     iamproto::Policy*) {
    EXPECT_STATUS_OK(IsContextMDValid(
        *context, "google.bigtable.admin.v2.BigtableInstanceAdmin.GetIamPolicy",
        google::cloud::internal::ApiClientHeader()));
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
  };
  auto mock_policy = create_get_policy_mock();

  EXPECT_CALL(*client_, GetIamPolicy)
      .WillOnce(mock_recoverable_failure)
      .WillOnce(mock_policy);

  std::string resource = "test-resource";
  auto policy = tested.GetNativeIamPolicy(resource);
  ASSERT_STATUS_OK(policy);
  EXPECT_EQ(3, policy->version());
  EXPECT_EQ("random-tag", policy->etag());
}

/// @test Verify positive scenario for InstanceAdmin::SetIamPolicy.
TEST_F(InstanceAdminTest, SetIamPolicy) {
  InstanceAdmin tested(client_);
  auto mock_policy = create_policy_with_params();
  EXPECT_CALL(*client_, SetIamPolicy).WillOnce(mock_policy);

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
  InstanceAdmin tested(client_);

  EXPECT_CALL(*client_, SetIamPolicy)
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "err!")));

  std::string resource = "test-resource";
  google::cloud::IamBindings iam_bindings =
      google::cloud::IamBindings("writer", {"abc@gmail.com", "xyz@gmail.com"});
  EXPECT_FALSE(tested.SetIamPolicy(resource, iam_bindings, "test-tag"));
}

/// @test Verify recoverable errors for InstanceAdmin::SetIamPolicy.
TEST_F(InstanceAdminTest, SetIamPolicyRecoverableError) {
  namespace iamproto = ::google::iam::v1;

  InstanceAdmin tested(client_);

  auto mock_recoverable_failure = [](grpc::ClientContext* context,
                                     iamproto::SetIamPolicyRequest const&,
                                     iamproto::Policy*) {
    EXPECT_STATUS_OK(IsContextMDValid(
        *context, "google.bigtable.admin.v2.BigtableInstanceAdmin.SetIamPolicy",
        google::cloud::internal::ApiClientHeader()));
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
  };
  auto mock_policy = create_policy_with_params();

  EXPECT_CALL(*client_, SetIamPolicy)
      .WillOnce(mock_recoverable_failure)
      .WillOnce(mock_policy);

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
  InstanceAdmin tested(client_);
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

/// @test Verify unrecoverable errors for InstanceAdmin::SetIamPolicy (native).
TEST_F(InstanceAdminTest, SetNativeIamPolicyUnrecoverableError) {
  InstanceAdmin tested(client_);

  EXPECT_CALL(*client_, SetIamPolicy)
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "err!")));

  std::string resource = "test-resource";
  auto iam_policy =
      IamPolicy({IamBinding("writer", {"abc@gmail.com", "xyz@gmail.com"})},
                "test-tag", 0);
  EXPECT_FALSE(tested.SetIamPolicy(resource, iam_policy));
}

/// @test Verify recoverable errors for InstanceAdmin::SetIamPolicy (native).
TEST_F(InstanceAdminTest, SetNativeIamPolicyRecoverableError) {
  namespace iamproto = ::google::iam::v1;

  InstanceAdmin tested(client_);

  auto mock_recoverable_failure = [](grpc::ClientContext* context,
                                     iamproto::SetIamPolicyRequest const&,
                                     iamproto::Policy*) {
    EXPECT_STATUS_OK(IsContextMDValid(
        *context, "google.bigtable.admin.v2.BigtableInstanceAdmin.SetIamPolicy",
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
/// @test Verify that InstanceAdmin::TestIamPermissions works in simple case.
TEST_F(InstanceAdminTest, TestIamPermissions) {
  namespace iamproto = ::google::iam::v1;
  InstanceAdmin tested(client_);

  auto mock_permission_set =
      [](grpc::ClientContext* context,
         iamproto::TestIamPermissionsRequest const&,
         iamproto::TestIamPermissionsResponse* response) {
        EXPECT_STATUS_OK(IsContextMDValid(
            *context,
            "google.bigtable.admin.v2.BigtableInstanceAdmin.TestIamPermissions",
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

/// @test Test for unrecoverable errors for InstanceAdmin::TestIamPermissions.
TEST_F(InstanceAdminTest, TestIamPermissionsUnrecoverableError) {
  InstanceAdmin tested(client_);

  EXPECT_CALL(*client_, TestIamPermissions)
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "err!")));

  std::string resource = "other-resource";

  EXPECT_FALSE(
      tested.TestIamPermissions(resource, {"reader", "writer", "owner"}));
}

/// @test Test for recoverable errors for InstanceAdmin::TestIamPermissions.
TEST_F(InstanceAdminTest, TestIamPermissionsRecoverableError) {
  namespace iamproto = ::google::iam::v1;
  InstanceAdmin tested(client_);

  auto mock_recoverable_failure = [](grpc::ClientContext* context,
                                     iamproto::TestIamPermissionsRequest const&,
                                     iamproto::TestIamPermissionsResponse*) {
    EXPECT_STATUS_OK(IsContextMDValid(
        *context,
        "google.bigtable.admin.v2.BigtableInstanceAdmin.TestIamPermissions",
        google::cloud::internal::ApiClientHeader()));
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
  };

  auto mock_permission_set =
      [](grpc::ClientContext* context,
         iamproto::TestIamPermissionsRequest const&,
         iamproto::TestIamPermissionsResponse* response) {
        EXPECT_STATUS_OK(IsContextMDValid(
            *context,
            "google.bigtable.admin.v2.BigtableInstanceAdmin.TestIamPermissions",
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

/// A fixture for the bigtable::InstanceAdmin tests that have async
/// implementations.
class ValidContextMdAsyncTest : public ::testing::Test {
 public:
  ValidContextMdAsyncTest()
      : cq_impl_(new FakeCompletionQueueImpl),
        cq_(cq_impl_),
        client_(new MockAdminClient(
            ClientOptions{}.DisableBackgroundThreads(cq_))) {
    EXPECT_CALL(*client_, project()).WillRepeatedly(ReturnRef(kProjectId));
    instance_admin_ = absl::make_unique<InstanceAdmin>(client_);
  }

 protected:
  template <typename ResultType>
  void FinishTest(ResultType res_future) {
    EXPECT_EQ(1U, cq_impl_->size());
    cq_impl_->SimulateCompletion(true);
    EXPECT_EQ(0U, cq_impl_->size());
    auto res = res_future.get();
    EXPECT_THAT(res, StatusIs(StatusCode::kPermissionDenied));
  }

  std::shared_ptr<FakeCompletionQueueImpl> cq_impl_;
  CompletionQueue cq_;
  std::shared_ptr<MockAdminClient> client_;
  std::unique_ptr<InstanceAdmin> instance_admin_;
};

TEST_F(ValidContextMdAsyncTest, CreateCluster) {
  ::google::cloud::bigtable::testing::MockAsyncFailingRpcFactory<
      btadmin::CreateClusterRequest, google::longrunning::Operation>
      rpc_factory;
  EXPECT_CALL(*client_, AsyncCreateCluster)
      .WillOnce(rpc_factory.Create(
          R"""(
              parent: "projects/the-project/instances/the-instance"
              cluster_id: "the-cluster"
              cluster: {
                  location: "projects/the-project/locations/loc1"
                  serve_nodes: 3
                  default_storage_type: SSD
              }
          )""",
          "google.bigtable.admin.v2.BigtableInstanceAdmin.CreateCluster"));
  FinishTest(instance_admin_->CreateCluster(
      ClusterConfig("loc1", 3, ClusterConfig::SSD), "the-instance",
      "the-cluster"));
}

TEST_F(ValidContextMdAsyncTest, CreateInstance) {
  ::google::cloud::bigtable::testing::MockAsyncFailingRpcFactory<
      btadmin::CreateInstanceRequest, google::longrunning::Operation>
      rpc_factory;
  EXPECT_CALL(*client_, AsyncCreateInstance)
      .WillOnce(rpc_factory.Create(
          R"""(
              parent: "projects/the-project"
              instance_id: "the-instance"
              instance: { display_name: "Displayed instance" }
          )""",
          "google.bigtable.admin.v2.BigtableInstanceAdmin.CreateInstance"));
  FinishTest(instance_admin_->CreateInstance(
      InstanceConfig("the-instance", "Displayed instance", {})));
}

TEST_F(ValidContextMdAsyncTest, UpdateAppProfile) {
  ::google::cloud::bigtable::testing::MockAsyncFailingRpcFactory<
      btadmin::UpdateAppProfileRequest, google::longrunning::Operation>
      rpc_factory;
  EXPECT_CALL(*client_, AsyncUpdateAppProfile)
      .WillOnce(rpc_factory.Create(
          R"""(
              app_profile: {
                  name: "projects/the-project/instances/the-instance/appProfiles/the-profile"
              }
          )""",
          "google.bigtable.admin.v2.BigtableInstanceAdmin.UpdateAppProfile"));
  FinishTest(instance_admin_->UpdateAppProfile("the-instance", "the-profile",
                                               AppProfileUpdateConfig()));
}

TEST_F(ValidContextMdAsyncTest, UpdateCluster) {
  ::google::cloud::bigtable::testing::MockAsyncFailingRpcFactory<
      btadmin::Cluster, google::longrunning::Operation>
      rpc_factory;
  EXPECT_CALL(*client_, AsyncUpdateCluster)
      .WillOnce(rpc_factory.Create(
          R"""(
              location: "loc1"
              serve_nodes: 3
              default_storage_type: SSD
              name: "projects/the-project/instances/the-instance/clusters/the-cluster"
          )""",
          "google.bigtable.admin.v2.BigtableInstanceAdmin.UpdateCluster"));
  auto cluster = ClusterConfig("loc1", 3, ClusterConfig::SSD).as_proto();
  cluster.set_name(
      "projects/the-project/instances/the-instance/clusters/the-cluster");
  FinishTest(instance_admin_->UpdateCluster(ClusterConfig(std::move(cluster))));
}

TEST_F(ValidContextMdAsyncTest, UpdateInstance) {
  ::google::cloud::bigtable::testing::MockAsyncFailingRpcFactory<
      btadmin::PartialUpdateInstanceRequest, google::longrunning::Operation>
      rpc_factory;
  EXPECT_CALL(*client_, AsyncUpdateInstance)
      .WillOnce(rpc_factory.Create(
          R"""(
              instance: {
                  name: "projects/the-project/instances/the-instance"
              }
          )""",
          "google.bigtable.admin.v2.BigtableInstanceAdmin."
          "PartialUpdateInstance"));
  Instance instance;
  instance.set_name("projects/the-project/instances/the-instance");
  FinishTest(instance_admin_->UpdateInstance(
      InstanceUpdateConfig(std::move(instance))));
}

TEST_F(InstanceAdminTest, CreateAppProfile) {
  InstanceAdmin tested(client_);
  std::string expected_text = R"""(
      parent: "projects/the-project/instances/the-instance"
      app_profile_id: "prof"
      app_profile: { multi_cluster_routing_use_any { } }
      )""";
  auto mock = MockRpcFactory<btadmin::CreateAppProfileRequest,
                             btadmin::AppProfile>::
      Create(expected_text,
             "google.bigtable.admin.v2.BigtableInstanceAdmin.CreateAppProfile");
  EXPECT_CALL(*client_, CreateAppProfile).WillOnce(mock);
  ASSERT_STATUS_OK(tested.CreateAppProfile(
      "the-instance", AppProfileConfig::MultiClusterUseAny("prof")));
}

TEST_F(InstanceAdminTest, DeleteAppProfile) {
  using ::google::protobuf::Empty;
  InstanceAdmin tested(client_);
  std::string expected_text = R"""(
      name: "projects/the-project/instances/the-instance/appProfiles/the-profile"
      ignore_warnings: true
      )""";
  auto mock = MockRpcFactory<btadmin::DeleteAppProfileRequest, Empty>::Create(
      expected_text,
      "google.bigtable.admin.v2.BigtableInstanceAdmin.DeleteAppProfile");
  EXPECT_CALL(*client_, DeleteAppProfile).WillOnce(mock);
  ASSERT_STATUS_OK(tested.DeleteAppProfile("the-instance", "the-profile"));
}

TEST_F(InstanceAdminTest, GetAppProfile) {
  using ::google::protobuf::Empty;
  InstanceAdmin tested(client_);
  std::string expected_text = R"""(
      name: "projects/the-project/instances/the-instance/appProfiles/the-profile"
      )""";
  auto mock =
      MockRpcFactory<btadmin::GetAppProfileRequest, btadmin::AppProfile>::
          Create(
              expected_text,
              "google.bigtable.admin.v2.BigtableInstanceAdmin.GetAppProfile");
  EXPECT_CALL(*client_, GetAppProfile).WillOnce(mock);
  ASSERT_STATUS_OK(tested.GetAppProfile("the-instance", "the-profile"));
}

TEST_F(InstanceAdminTest, GetInstance) {
  using ::google::protobuf::Empty;
  InstanceAdmin tested(client_);
  std::string expected_text = R"""(
      name: "projects/the-project/instances/the-instance"
      )""";
  auto mock =
      MockRpcFactory<btadmin::GetInstanceRequest, btadmin::Instance>::Create(
          expected_text,
          "google.bigtable.admin.v2.BigtableInstanceAdmin.GetInstance");
  EXPECT_CALL(*client_, GetInstance).WillOnce(mock);
  ASSERT_STATUS_OK(tested.GetInstance("the-instance"));
}

}  // namespace
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
