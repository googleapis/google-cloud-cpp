// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/spanner/internal/instance_admin_metadata.h"
#include "google/cloud/spanner/instance.h"
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
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

namespace gsai = ::google::spanner::admin::instance;

using ::google::cloud::testing_util::ValidateMetadataFixture;

class InstanceAdminMetadataTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_ = std::make_shared<spanner_testing::MockInstanceAdminStub>();
    InstanceAdminMetadata stub(mock_);
  }

  void TearDown() override {}

  void IsContextMDValid(grpc::ClientContext& context, std::string const& method,
                        google::protobuf::Message const& request) {
    return validate_metadata_fixture_.IsContextMDValid(
        context, method, request, google::cloud::internal::ApiClientHeader());
  }

  static Status TransientError() {
    return Status(StatusCode::kUnavailable, "try-again");
  }

  std::shared_ptr<spanner_testing::MockInstanceAdminStub> mock_;
  ValidateMetadataFixture validate_metadata_fixture_;
};

TEST_F(InstanceAdminMetadataTest, GetInstance) {
  EXPECT_CALL(*mock_, GetInstance)
      .WillOnce([this](grpc::ClientContext& context,
                       gsai::v1::GetInstanceRequest const& request) {
        IsContextMDValid(context,
                         "google.spanner.admin.instance.v1.InstanceAdmin."
                         "GetInstance",
                         request);
        return TransientError();
      });

  InstanceAdminMetadata stub(mock_);
  grpc::ClientContext context;
  gsai::v1::GetInstanceRequest request;
  request.set_name(
      google::cloud::spanner::Instance(
          google::cloud::Project("test-project-id"), "test-instance-id")
          .FullName());
  auto response = stub.GetInstance(context, request);
  EXPECT_EQ(TransientError(), response.status());
}

TEST_F(InstanceAdminMetadataTest, GetInstanceConfig) {
  EXPECT_CALL(*mock_, GetInstanceConfig)
      .WillOnce([this](grpc::ClientContext& context,
                       gsai::v1::GetInstanceConfigRequest const& request) {
        IsContextMDValid(context,
                         "google.spanner.admin.instance.v1.InstanceAdmin."
                         "GetInstanceConfig",
                         request);
        return TransientError();
      });

  InstanceAdminMetadata stub(mock_);
  grpc::ClientContext context;
  gsai::v1::GetInstanceConfigRequest request;
  request.set_name(google::cloud::Project("test-project-id").FullName() +
                   "/instanceConfigs/test-instance-config-id");
  auto response = stub.GetInstanceConfig(context, request);
  EXPECT_EQ(TransientError(), response.status());
}

TEST_F(InstanceAdminMetadataTest, ListInstanceConfigs) {
  EXPECT_CALL(*mock_, ListInstanceConfigs)
      .WillOnce([this](grpc::ClientContext& context,
                       gsai::v1::ListInstanceConfigsRequest const& request) {
        IsContextMDValid(context,
                         "google.spanner.admin.instance.v1.InstanceAdmin."
                         "ListInstanceConfigs",
                         request);
        return TransientError();
      });

  InstanceAdminMetadata stub(mock_);
  grpc::ClientContext context;
  gsai::v1::ListInstanceConfigsRequest request;
  request.set_parent(google::cloud::Project("test-project-id").FullName());
  auto response = stub.ListInstanceConfigs(context, request);
  EXPECT_EQ(TransientError(), response.status());
}

TEST_F(InstanceAdminMetadataTest, CreateInstance) {
  EXPECT_CALL(*mock_, AsyncCreateInstance)
      .WillOnce([this](CompletionQueue&,
                       std::unique_ptr<grpc::ClientContext> context,
                       gsai::v1::CreateInstanceRequest const& request) {
        IsContextMDValid(*context,
                         "google.spanner.admin.instance.v1.InstanceAdmin."
                         "CreateInstance",
                         request);
        return make_ready_future(
            StatusOr<google::longrunning::Operation>(TransientError()));
      });

  InstanceAdminMetadata stub(mock_);
  CompletionQueue cq;
  gsai::v1::CreateInstanceRequest request;
  request.set_parent(google::cloud::Project("test-project-id").FullName());
  request.set_instance_id("test-instance-id");
  auto response = stub.AsyncCreateInstance(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_EQ(TransientError(), response.get().status());
}

TEST_F(InstanceAdminMetadataTest, UpdateInstance) {
  EXPECT_CALL(*mock_, AsyncUpdateInstance)
      .WillOnce([this](CompletionQueue&,
                       std::unique_ptr<grpc::ClientContext> context,
                       gsai::v1::UpdateInstanceRequest const& request) {
        IsContextMDValid(*context,
                         "google.spanner.admin.instance.v1.InstanceAdmin."
                         "UpdateInstance",
                         request);
        return make_ready_future(
            StatusOr<google::longrunning::Operation>(TransientError()));
      });

  InstanceAdminMetadata stub(mock_);
  CompletionQueue cq;
  gsai::v1::UpdateInstanceRequest request;
  request.mutable_instance()->set_name(
      google::cloud::spanner::Instance(
          google::cloud::Project("test-project-id"), "test-instance-id")
          .FullName());
  auto response = stub.AsyncUpdateInstance(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_EQ(TransientError(), response.get().status());
}

TEST_F(InstanceAdminMetadataTest, DeleteInstance) {
  EXPECT_CALL(*mock_, DeleteInstance)
      .WillOnce([this](grpc::ClientContext& context,
                       gsai::v1::DeleteInstanceRequest const& request) {
        IsContextMDValid(context,
                         "google.spanner.admin.instance.v1.InstanceAdmin."
                         "DeleteInstance",
                         request);
        return TransientError();
      });

  InstanceAdminMetadata stub(mock_);
  grpc::ClientContext context;
  gsai::v1::DeleteInstanceRequest request;
  request.set_name(
      google::cloud::spanner::Instance(
          google::cloud::Project("test-project-id"), "test-instance-id")
          .FullName());
  auto status = stub.DeleteInstance(context, request);
  EXPECT_EQ(TransientError(), status);
}

TEST_F(InstanceAdminMetadataTest, ListInstances) {
  EXPECT_CALL(*mock_, ListInstances)
      .WillOnce([this](grpc::ClientContext& context,
                       gsai::v1::ListInstancesRequest const& request) {
        IsContextMDValid(context,
                         "google.spanner.admin.instance.v1.InstanceAdmin."
                         "ListInstances",
                         request);
        return TransientError();
      });

  InstanceAdminMetadata stub(mock_);
  grpc::ClientContext context;
  gsai::v1::ListInstancesRequest request;
  request.set_parent(google::cloud::Project("test-project-id").FullName());
  auto response = stub.ListInstances(context, request);
  EXPECT_EQ(TransientError(), response.status());
}

TEST_F(InstanceAdminMetadataTest, GetIamPolicy) {
  EXPECT_CALL(*mock_, GetIamPolicy)
      .WillOnce([this](grpc::ClientContext& context,
                       google::iam::v1::GetIamPolicyRequest const& request) {
        IsContextMDValid(context,
                         "google.spanner.admin.instance.v1.InstanceAdmin."
                         "GetIamPolicy",
                         request);
        return TransientError();
      });

  InstanceAdminMetadata stub(mock_);
  grpc::ClientContext context;
  google::iam::v1::GetIamPolicyRequest request;
  request.set_resource(
      google::cloud::spanner::Instance(
          google::cloud::Project("test-project-id"), "test-instance-id")
          .FullName());
  auto response = stub.GetIamPolicy(context, request);
  EXPECT_EQ(TransientError(), response.status());
}

TEST_F(InstanceAdminMetadataTest, SetIamPolicy) {
  EXPECT_CALL(*mock_, SetIamPolicy)
      .WillOnce([this](grpc::ClientContext& context,
                       google::iam::v1::SetIamPolicyRequest const& request) {
        IsContextMDValid(context,
                         "google.spanner.admin.instance.v1.InstanceAdmin."
                         "SetIamPolicy",
                         request);
        return TransientError();
      });

  InstanceAdminMetadata stub(mock_);
  grpc::ClientContext context;
  google::iam::v1::SetIamPolicyRequest request;
  request.set_resource(
      google::cloud::spanner::Instance(
          google::cloud::Project("test-project-id"), "test-instance-id")
          .FullName());
  auto response = stub.SetIamPolicy(context, request);
  EXPECT_EQ(TransientError(), response.status());
}

TEST_F(InstanceAdminMetadataTest, TestIamPermissions) {
  EXPECT_CALL(*mock_, TestIamPermissions)
      .WillOnce(
          [this](grpc::ClientContext& context,
                 google::iam::v1::TestIamPermissionsRequest const& request) {
            IsContextMDValid(context,
                             "google.spanner.admin.instance.v1.InstanceAdmin."
                             "TestIamPermissions",
                             request);
            return TransientError();
          });

  InstanceAdminMetadata stub(mock_);
  grpc::ClientContext context;
  google::iam::v1::TestIamPermissionsRequest request;
  request.set_resource(
      google::cloud::spanner::Instance(
          google::cloud::Project("test-project-id"), "test-instance-id")
          .FullName());
  auto response = stub.TestIamPermissions(context, request);
  EXPECT_EQ(TransientError(), response.status());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
