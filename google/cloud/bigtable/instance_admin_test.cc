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
#include "google/cloud/bigtable/testing/mock_completion_queue.h"
#include "google/cloud/bigtable/testing/mock_instance_admin_client.h"
#include "google/cloud/bigtable/testing/mock_response_reader.h"
#include "google/cloud/internal/make_unique.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <gmock/gmock.h>

namespace {
namespace btadmin = google::bigtable::admin::v2;
namespace bigtable = google::cloud::bigtable;

using MockAdminClient = bigtable::testing::MockInstanceAdminClient;
using namespace google::cloud::testing_util::chrono_literals;

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
    // Return the right token.
    response->set_next_page_token(returned_token);
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

// A lambda to create lambdas.  Basically we would be rewriting the same
// lambda twice without this thing.
auto create_list_clusters_lambda =
    [](std::string expected_token, std::string returned_token,
       std::string instance_id, std::vector<std::string> cluster_ids) {
      return [expected_token, returned_token, instance_id, cluster_ids](
                 grpc::ClientContext* ctx,
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
      expected_text);
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
  auto mock_recoverable_failure =
      [](grpc::ClientContext* ctx, btadmin::ListClustersRequest const& request,
         btadmin::ListClustersResponse* response) {
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
  auto mock = create_cluster();
  EXPECT_CALL(*client_, GetCluster(_, _, _)).WillOnce(Invoke(mock));
  bigtable::InstanceId instance_id("the-instance");
  bigtable::ClusterId cluster_id("the-cluster");
  // After all the setup, make the actual call we want to test.
  auto cluster = tested.GetCluster(instance_id, cluster_id);
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
  bigtable::InstanceId instance_id("other-instance");
  bigtable::ClusterId cluster_id("other-cluster");

  ASSERT_FALSE(tested.GetCluster(instance_id, cluster_id));
}

/// @test Verify recoverable errors for GetCluster
TEST_F(InstanceAdminTest, GetClusterRecoverableError) {
  using namespace ::testing;

  bigtable::InstanceAdmin tested(client_);
  auto mock_recoverable_failure = [](grpc::ClientContext* ctx,
                                     btadmin::GetClusterRequest const& request,
                                     btadmin::Cluster* response) {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
  };

  auto mock_cluster = create_cluster();
  EXPECT_CALL(*client_, GetCluster(_, _, _))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(mock_cluster));

  // After all the setup, make the actual call we want to test.
  bigtable::InstanceId instance_id("the-instance");
  bigtable::ClusterId cluster_id("the-cluster");
  // After all the setup, make the actual call we want to test.
  auto cluster = tested.GetCluster(instance_id, cluster_id);
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
      expected_text);
  EXPECT_CALL(*client_, DeleteCluster(_, _, _)).WillOnce(Invoke(mock));
  bigtable::InstanceId instance_id("the-instance");
  bigtable::ClusterId cluster_id("the-cluster");
  // After all the setup, make the actual call we want to test.
  ASSERT_STATUS_OK(tested.DeleteCluster(instance_id, cluster_id));
}

/// @test Verify unrecoverable error for DeleteCluster
TEST_F(InstanceAdminTest, DeleteClusterUnrecoverableError) {
  using namespace ::testing;
  bigtable::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, DeleteCluster(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh")));
  bigtable::InstanceId instance_id("other-instance");
  bigtable::ClusterId cluster_id("other-cluster");
  // After all the setup, make the actual call we want to test.
  EXPECT_FALSE(tested.DeleteCluster(instance_id, cluster_id).ok());
}

/// @test Verify that recoverable error for DeleteCluster
TEST_F(InstanceAdminTest, DeleteClusterRecoverableError) {
  using namespace ::testing;
  bigtable::InstanceAdmin tested(client_);
  EXPECT_CALL(*client_, DeleteCluster(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")));

  bigtable::InstanceId instance_id("other-instance");
  bigtable::ClusterId cluster_id("other-cluster");
  // After all the setup, make the actual call we want to test.
  EXPECT_FALSE(tested.DeleteCluster(instance_id, cluster_id).ok());
}

/// @test Verify positive scenario for InstanceAdmin::GetIamPolicy.
TEST_F(InstanceAdminTest, GetIamPolicy) {
  using ::testing::_;
  using ::testing::Invoke;

  bigtable::InstanceAdmin tested(client_);
  auto mock_policy = create_policy();
  EXPECT_CALL(*client_, GetIamPolicy(_, _, _)).WillOnce(Invoke(mock_policy));

  std::string resource = "test-resource";
  tested.GetIamPolicy(resource);
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

  auto mock_recoverable_failure =
      [](grpc::ClientContext* ctx, iamproto::GetIamPolicyRequest const& request,
         iamproto::Policy* response) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
      };
  auto mock_policy = create_policy();

  EXPECT_CALL(*client_, GetIamPolicy(_, _, _))
      .WillOnce(Invoke(mock_recoverable_failure))
      .WillOnce(Invoke(mock_policy));

  std::string resource = "test-resource";
  tested.GetIamPolicy(resource);
}

using MockAsyncIamPolicyReader =
    google::cloud::bigtable::testing::MockAsyncResponseReader<
        ::google::iam::v1::Policy>;

class AsyncGetIamPolicyTest : public ::testing::Test {
 public:
  AsyncGetIamPolicyTest()
      : cq_impl_(new bigtable::testing::MockCompletionQueue),
        cq_(cq_impl_),
        client_(new bigtable::testing::MockInstanceAdminClient),
        reader_(new MockAsyncIamPolicyReader) {
    using namespace ::testing;
    EXPECT_CALL(*client_, project()).WillRepeatedly(ReturnRef(kProjectId));
    EXPECT_CALL(*client_, AsyncGetIamPolicy(_, _, _))
        .WillOnce(
            Invoke([this](grpc::ClientContext*,
                          ::google::iam::v1::GetIamPolicyRequest const& request,
                          grpc::CompletionQueue*) {
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
    user_future_ = instance_admin.AsyncGetIamPolicy(
        cq_, google::cloud::bigtable::InstanceId("test-instance"));
  }

  std::shared_ptr<bigtable::testing::MockCompletionQueue> cq_impl_;
  bigtable::CompletionQueue cq_;
  std::shared_ptr<bigtable::testing::MockInstanceAdminClient> client_;
  google::cloud::future<google::cloud::StatusOr<google::cloud::IamPolicy>>
      user_future_;
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
  EXPECT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);
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
  EXPECT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);

  auto policy = user_future_.get();
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

  EXPECT_EQ(1U, policy->bindings.size());
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

  auto mock_recoverable_failure =
      [](grpc::ClientContext* ctx, iamproto::SetIamPolicyRequest const& request,
         iamproto::Policy* response) {
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

  EXPECT_EQ(1U, policy->bindings.size());
  EXPECT_EQ("test-tag", policy->etag);
}

/// @test Verify that InstanceAdmin::TestIamPermissions works in simple case.
TEST_F(InstanceAdminTest, TestIamPermissions) {
  using ::testing::_;
  using ::testing::Invoke;
  namespace iamproto = ::google::iam::v1;
  bigtable::InstanceAdmin tested(client_);

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

  std::string resource = "the-resource";
  auto permission_set =
      tested.TestIamPermissions(resource, {"reader", "writer", "owner"});
  ASSERT_STATUS_OK(permission_set);

  EXPECT_EQ(2U, permission_set->size());
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

  std::string resource = "the-resource";
  auto permission_set =
      tested.TestIamPermissions(resource, {"writer", "reader", "owner"});
  ASSERT_STATUS_OK(permission_set);

  EXPECT_EQ(2U, permission_set->size());
}

using MockAsyncDeleteClusterReader =
    google::cloud::bigtable::testing::MockAsyncResponseReader<
        ::google::protobuf::Empty>;

class AsyncDeleteClusterTest : public ::testing::Test {
 public:
  AsyncDeleteClusterTest()
      : cq_impl_(new bigtable::testing::MockCompletionQueue),
        cq_(cq_impl_),
        client_(new bigtable::testing::MockInstanceAdminClient),
        reader_(new MockAsyncDeleteClusterReader) {
    using namespace ::testing;
    EXPECT_CALL(*client_, project()).WillRepeatedly(ReturnRef(kProjectId));
    EXPECT_CALL(*client_, AsyncDeleteCluster(_, _, _))
        .WillOnce(Invoke([this](grpc::ClientContext*,
                                btadmin::DeleteClusterRequest const& request,
                                grpc::CompletionQueue*) {
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
    user_future_ = instance_admin.AsyncDeleteCluster(
        cq_, google::cloud::bigtable::InstanceId("test-instance"),
        google::cloud::bigtable::ClusterId("the-cluster"));
  }

  std::shared_ptr<bigtable::testing::MockCompletionQueue> cq_impl_;
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
  EXPECT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);
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
  EXPECT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);

  auto status = user_future_.get();
  ASSERT_EQ(google::cloud::StatusCode::kPermissionDenied, status.code());
}

using MockAsyncSetIamPolicyReader =
    google::cloud::bigtable::testing::MockAsyncResponseReader<
        ::google::iam::v1::Policy>;

class AsyncSetIamPolicyTest : public ::testing::Test {
 public:
  AsyncSetIamPolicyTest()
      : cq_impl_(new bigtable::testing::MockCompletionQueue),
        cq_(cq_impl_),
        client_(new bigtable::testing::MockInstanceAdminClient),
        reader_(new MockAsyncSetIamPolicyReader) {
    using namespace ::testing;
    EXPECT_CALL(*client_, project()).WillRepeatedly(ReturnRef(kProjectId));
    EXPECT_CALL(*client_, AsyncSetIamPolicy(_, _, _))
        .WillOnce(
            Invoke([this](grpc::ClientContext*,
                          ::google::iam::v1::SetIamPolicyRequest const& request,
                          grpc::CompletionQueue*) {
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
        cq_, bigtable::InstanceId("test-instance"),
        google::cloud::IamBindings("writer",
                                   {"abc@gmail.com", "xyz@gmail.com"}),
        "test-tag");
  }

  std::shared_ptr<bigtable::testing::MockCompletionQueue> cq_impl_;
  bigtable::CompletionQueue cq_;
  std::shared_ptr<bigtable::testing::MockInstanceAdminClient> client_;
  google::cloud::future<google::cloud::StatusOr<google::cloud::IamPolicy>>
      user_future_;
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
  EXPECT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);
  auto policy = user_future_.get();
  ASSERT_STATUS_OK(policy);

  EXPECT_EQ(1U, policy->bindings.size());
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
  EXPECT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);

  auto policy = user_future_.get();
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
      : cq_impl_(new bigtable::testing::MockCompletionQueue),
        cq_(cq_impl_),
        client_(new bigtable::testing::MockInstanceAdminClient),
        reader_(new MockAsyncTestIamPermissionsReader) {
    using namespace ::testing;
    EXPECT_CALL(*client_, project()).WillRepeatedly(ReturnRef(kProjectId));
    EXPECT_CALL(*client_, AsyncTestIamPermissions(_, _, _))
        .WillOnce(Invoke(
            [this](grpc::ClientContext*,
                   ::google::iam::v1::TestIamPermissionsRequest const& request,
                   grpc::CompletionQueue*) {
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

  std::shared_ptr<bigtable::testing::MockCompletionQueue> cq_impl_;
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
  EXPECT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);
  auto permission_set = user_future_.get();
  ASSERT_STATUS_OK(permission_set);
  EXPECT_EQ(2U, permission_set->size());
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
  EXPECT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);

  auto permission_set = user_future_.get();
  ASSERT_FALSE(permission_set);
  ASSERT_EQ(google::cloud::StatusCode::kPermissionDenied,
            permission_set.status().code());
}
