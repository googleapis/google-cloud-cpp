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

#include "google/cloud/spanner/internal/instance_admin_logging.h"
#include "google/cloud/spanner/testing/mock_instance_admin_stub.h"
#include "google/cloud/spanner/tracing_options.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
namespace spanner_internal {
inline namespace SPANNER_CLIENT_NS {
namespace {

using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Return;
namespace gcsa = ::google::spanner::admin::instance::v1;

class InstanceAdminLoggingTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_ = std::make_shared<spanner_testing::MockInstanceAdminStub>();
  }

  static Status TransientError() {
    return Status(StatusCode::kUnavailable, "try-again");
  }

  std::shared_ptr<spanner_testing::MockInstanceAdminStub> mock_;
  testing_util::ScopedLog log_;
};

TEST_F(InstanceAdminLoggingTest, GetInstance) {
  EXPECT_CALL(*mock_, GetInstance).WillOnce(Return(TransientError()));

  InstanceAdminLogging stub(mock_, TracingOptions{});

  grpc::ClientContext context;
  auto response = stub.GetInstance(context, gcsa::GetInstanceRequest{});
  EXPECT_EQ(TransientError(), response.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetInstance")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(InstanceAdminLoggingTest, CreateInstance) {
  EXPECT_CALL(*mock_, AsyncCreateInstance)
      .WillOnce([](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                   gcsa::CreateInstanceRequest const&) {
        return make_ready_future(
            StatusOr<google::longrunning::Operation>(TransientError()));
      });

  InstanceAdminLogging stub(mock_, TracingOptions{});

  CompletionQueue cq;
  auto response =
      stub.AsyncCreateInstance(cq, absl::make_unique<grpc::ClientContext>(),
                               gcsa::CreateInstanceRequest{});
  EXPECT_EQ(TransientError(), response.get().status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("CreateInstance")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(InstanceAdminLoggingTest, UpdateInstance) {
  EXPECT_CALL(*mock_, AsyncUpdateInstance)
      .WillOnce([](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                   gcsa::UpdateInstanceRequest const&) {
        return make_ready_future(
            StatusOr<google::longrunning::Operation>(TransientError()));
      });

  InstanceAdminLogging stub(mock_, TracingOptions{});

  CompletionQueue cq;
  auto response =
      stub.AsyncUpdateInstance(cq, absl::make_unique<grpc::ClientContext>(),
                               gcsa::UpdateInstanceRequest{});
  EXPECT_EQ(TransientError(), response.get().status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("UpdateInstance")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(InstanceAdminLoggingTest, DeleteInstance) {
  EXPECT_CALL(*mock_, DeleteInstance).WillOnce(Return(TransientError()));

  InstanceAdminLogging stub(mock_, TracingOptions{});

  grpc::ClientContext context;
  auto status = stub.DeleteInstance(context, gcsa::DeleteInstanceRequest{});
  EXPECT_EQ(TransientError(), status);

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("DeleteInstance")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(InstanceAdminLoggingTest, GetInstanceConfig) {
  EXPECT_CALL(*mock_, GetInstanceConfig).WillOnce(Return(TransientError()));

  InstanceAdminLogging stub(mock_, TracingOptions{});

  grpc::ClientContext context;
  auto response =
      stub.GetInstanceConfig(context, gcsa::GetInstanceConfigRequest{});
  EXPECT_EQ(TransientError(), response.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetInstanceConfig")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(InstanceAdminLoggingTest, ListInstanceConfigs) {
  EXPECT_CALL(*mock_, ListInstanceConfigs).WillOnce(Return(TransientError()));

  InstanceAdminLogging stub(mock_, TracingOptions{});

  grpc::ClientContext context;
  auto response =
      stub.ListInstanceConfigs(context, gcsa::ListInstanceConfigsRequest{});
  EXPECT_EQ(TransientError(), response.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListInstanceConfigs")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(InstanceAdminLoggingTest, ListInstances) {
  EXPECT_CALL(*mock_, ListInstances).WillOnce(Return(TransientError()));

  InstanceAdminLogging stub(mock_, TracingOptions{});

  grpc::ClientContext context;
  auto response = stub.ListInstances(context, gcsa::ListInstancesRequest{});
  EXPECT_EQ(TransientError(), response.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListInstances")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(InstanceAdminLoggingTest, GetIamPolicy) {
  EXPECT_CALL(*mock_, GetIamPolicy).WillOnce(Return(TransientError()));

  InstanceAdminLogging stub(mock_, TracingOptions{});

  grpc::ClientContext context;
  auto response =
      stub.GetIamPolicy(context, google::iam::v1::GetIamPolicyRequest{});
  EXPECT_EQ(TransientError(), response.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetIamPolicy")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(InstanceAdminLoggingTest, SetIamPolicy) {
  EXPECT_CALL(*mock_, SetIamPolicy).WillOnce(Return(TransientError()));

  InstanceAdminLogging stub(mock_, TracingOptions{});

  grpc::ClientContext context;
  auto response =
      stub.SetIamPolicy(context, google::iam::v1::SetIamPolicyRequest{});
  EXPECT_EQ(TransientError(), response.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("SetIamPolicy")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(InstanceAdminLoggingTest, TestIamPermissions) {
  EXPECT_CALL(*mock_, TestIamPermissions).WillOnce(Return(TransientError()));

  InstanceAdminLogging stub(mock_, TracingOptions{});

  grpc::ClientContext context;
  auto response = stub.TestIamPermissions(
      context, google::iam::v1::TestIamPermissionsRequest{});
  EXPECT_EQ(TransientError(), response.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("TestIamPermissions")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
