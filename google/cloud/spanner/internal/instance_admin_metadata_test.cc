// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/internal/instance_admin_metadata.h"
#include "google/cloud/spanner/testing/mock_instance_admin_stub.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
namespace spanner_internal {
inline namespace SPANNER_CLIENT_NS {
namespace {

using ::google::cloud::testing_util::IsContextMDValid;
namespace gcsa = ::google::spanner::admin::instance::v1;

class InstanceAdminMetadataTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_ = std::make_shared<spanner_testing::MockInstanceAdminStub>();
    InstanceAdminMetadata stub(mock_);
    expected_api_client_header_ = google::cloud::internal::ApiClientHeader();
  }

  void TearDown() override {}

  static Status TransientError() {
    return Status(StatusCode::kUnavailable, "try-again");
  }

  std::shared_ptr<spanner_testing::MockInstanceAdminStub> mock_;
  std::string expected_api_client_header_;
};

TEST_F(InstanceAdminMetadataTest, GetInstance) {
  EXPECT_CALL(*mock_, GetInstance)
      .WillOnce([this](grpc::ClientContext& context,
                       gcsa::GetInstanceRequest const&) {
        EXPECT_STATUS_OK(
            IsContextMDValid(context,
                             "google.spanner.admin.instance.v1.InstanceAdmin."
                             "GetInstance",
                             expected_api_client_header_));
        return TransientError();
      });

  InstanceAdminMetadata stub(mock_);
  grpc::ClientContext context;
  gcsa::GetInstanceRequest request;
  request.set_name(
      "projects/test-project-id/"
      "instances/test-instance-id");
  auto response = stub.GetInstance(context, request);
  EXPECT_EQ(TransientError(), response.status());
}

TEST_F(InstanceAdminMetadataTest, GetInstanceConfig) {
  EXPECT_CALL(*mock_, GetInstanceConfig)
      .WillOnce([this](grpc::ClientContext& context,
                       gcsa::GetInstanceConfigRequest const&) {
        EXPECT_STATUS_OK(
            IsContextMDValid(context,
                             "google.spanner.admin.instance.v1.InstanceAdmin."
                             "GetInstanceConfig",
                             expected_api_client_header_));
        return TransientError();
      });

  InstanceAdminMetadata stub(mock_);
  grpc::ClientContext context;
  gcsa::GetInstanceConfigRequest request;
  request.set_name(
      "projects/test-project-id/"
      "instanceConfigs/test-instance-config-id");
  auto response = stub.GetInstanceConfig(context, request);
  EXPECT_EQ(TransientError(), response.status());
}

TEST_F(InstanceAdminMetadataTest, ListInstanceConfigs) {
  EXPECT_CALL(*mock_, ListInstanceConfigs)
      .WillOnce([this](grpc::ClientContext& context,
                       gcsa::ListInstanceConfigsRequest const&) {
        EXPECT_STATUS_OK(
            IsContextMDValid(context,
                             "google.spanner.admin.instance.v1.InstanceAdmin."
                             "ListInstanceConfigs",
                             expected_api_client_header_));
        return TransientError();
      });

  InstanceAdminMetadata stub(mock_);
  grpc::ClientContext context;
  gcsa::ListInstanceConfigsRequest request;
  request.set_parent("projects/test-project-id");
  auto response = stub.ListInstanceConfigs(context, request);
  EXPECT_EQ(TransientError(), response.status());
}

TEST_F(InstanceAdminMetadataTest, CreateInstance) {
  EXPECT_CALL(*mock_, AsyncCreateInstance)
      .WillOnce([this](CompletionQueue&,
                       std::unique_ptr<grpc::ClientContext> context,
                       gcsa::CreateInstanceRequest const&) {
        EXPECT_STATUS_OK(
            IsContextMDValid(*context,
                             "google.spanner.admin.instance.v1.InstanceAdmin."
                             "CreateInstance",
                             expected_api_client_header_));
        return make_ready_future(
            StatusOr<google::longrunning::Operation>(TransientError()));
      });

  InstanceAdminMetadata stub(mock_);
  CompletionQueue cq;
  gcsa::CreateInstanceRequest request;
  request.set_parent("projects/test-project-id");
  request.set_instance_id("test-instance-id");
  auto response = stub.AsyncCreateInstance(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_EQ(TransientError(), response.get().status());
}

TEST_F(InstanceAdminMetadataTest, UpdateInstance) {
  EXPECT_CALL(*mock_, AsyncUpdateInstance)
      .WillOnce([this](CompletionQueue&,
                       std::unique_ptr<grpc::ClientContext> context,
                       gcsa::UpdateInstanceRequest const&) {
        EXPECT_STATUS_OK(
            IsContextMDValid(*context,
                             "google.spanner.admin.instance.v1.InstanceAdmin."
                             "UpdateInstance",
                             expected_api_client_header_));
        return make_ready_future(
            StatusOr<google::longrunning::Operation>(TransientError()));
      });

  InstanceAdminMetadata stub(mock_);
  CompletionQueue cq;
  gcsa::UpdateInstanceRequest request;
  request.mutable_instance()->set_name(
      "projects/test-project-id/instances/test-instance-id");
  auto response = stub.AsyncUpdateInstance(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_EQ(TransientError(), response.get().status());
}

TEST_F(InstanceAdminMetadataTest, DeleteInstance) {
  EXPECT_CALL(*mock_, DeleteInstance)
      .WillOnce([this](grpc::ClientContext& context,
                       gcsa::DeleteInstanceRequest const&) {
        EXPECT_STATUS_OK(
            IsContextMDValid(context,
                             "google.spanner.admin.instance.v1.InstanceAdmin."
                             "DeleteInstance",
                             expected_api_client_header_));
        return TransientError();
      });

  InstanceAdminMetadata stub(mock_);
  grpc::ClientContext context;
  gcsa::DeleteInstanceRequest request;
  request.set_name("projects/test-project-id/instances/test-instance-id");
  auto status = stub.DeleteInstance(context, request);
  EXPECT_EQ(TransientError(), status);
}

TEST_F(InstanceAdminMetadataTest, ListInstances) {
  EXPECT_CALL(*mock_, ListInstances)
      .WillOnce([this](grpc::ClientContext& context,
                       gcsa::ListInstancesRequest const&) {
        EXPECT_STATUS_OK(
            IsContextMDValid(context,
                             "google.spanner.admin.instance.v1.InstanceAdmin."
                             "ListInstances",
                             expected_api_client_header_));
        return TransientError();
      });

  InstanceAdminMetadata stub(mock_);
  grpc::ClientContext context;
  gcsa::ListInstancesRequest request;
  request.set_parent("projects/test-project-id");
  auto response = stub.ListInstances(context, request);
  EXPECT_EQ(TransientError(), response.status());
}

TEST_F(InstanceAdminMetadataTest, GetIamPolicy) {
  EXPECT_CALL(*mock_, GetIamPolicy)
      .WillOnce([this](grpc::ClientContext& context,
                       google::iam::v1::GetIamPolicyRequest const&) {
        EXPECT_STATUS_OK(
            IsContextMDValid(context,
                             "google.spanner.admin.instance.v1.InstanceAdmin."
                             "GetIamPolicy",
                             expected_api_client_header_));
        return TransientError();
      });

  InstanceAdminMetadata stub(mock_);
  grpc::ClientContext context;
  google::iam::v1::GetIamPolicyRequest request;
  request.set_resource("projects/test-project-id/instances/test-instance-id");

  auto response = stub.GetIamPolicy(context, request);
  EXPECT_EQ(TransientError(), response.status());
}

TEST_F(InstanceAdminMetadataTest, SetIamPolicy) {
  EXPECT_CALL(*mock_, SetIamPolicy)
      .WillOnce([this](grpc::ClientContext& context,
                       google::iam::v1::SetIamPolicyRequest const&) {
        EXPECT_STATUS_OK(
            IsContextMDValid(context,
                             "google.spanner.admin.instance.v1.InstanceAdmin."
                             "SetIamPolicy",
                             expected_api_client_header_));
        return TransientError();
      });

  InstanceAdminMetadata stub(mock_);
  grpc::ClientContext context;
  google::iam::v1::SetIamPolicyRequest request;
  request.set_resource("projects/test-project-id/instances/test-instance-id");
  auto response = stub.SetIamPolicy(context, request);
  EXPECT_EQ(TransientError(), response.status());
}

TEST_F(InstanceAdminMetadataTest, TestIamPermissions) {
  EXPECT_CALL(*mock_, TestIamPermissions)
      .WillOnce([this](grpc::ClientContext& context,
                       google::iam::v1::TestIamPermissionsRequest const&) {
        EXPECT_STATUS_OK(
            IsContextMDValid(context,
                             "google.spanner.admin.instance.v1.InstanceAdmin."
                             "TestIamPermissions",
                             expected_api_client_header_));
        return TransientError();
      });

  InstanceAdminMetadata stub(mock_);
  grpc::ClientContext context;
  google::iam::v1::TestIamPermissionsRequest request;
  request.set_resource("projects/test-project-id/instances/test-instance-id");
  auto response = stub.TestIamPermissions(context, request);
  EXPECT_EQ(TransientError(), response.status());
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
