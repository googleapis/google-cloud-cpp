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
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/capture_log_lines_backend.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner_internal {
inline namespace SPANNER_CLIENT_NS {
namespace {

using ::testing::_;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Return;
namespace gcsa = ::google::spanner::admin::instance::v1;

class InstanceAdminLoggingTest : public ::testing::Test {
 protected:
  void SetUp() override {
    backend_ =
        std::make_shared<google::cloud::testing_util::CaptureLogLinesBackend>();
    logger_id_ = google::cloud::LogSink::Instance().AddBackend(backend_);
    mock_ = std::make_shared<spanner_testing::MockInstanceAdminStub>();
  }

  void TearDown() override {
    google::cloud::LogSink::Instance().RemoveBackend(logger_id_);
    logger_id_ = 0;
  }

  static Status TransientError() {
    return Status(StatusCode::kUnavailable, "try-again");
  }

  std::vector<std::string> ClearLogLines() { return backend_->ClearLogLines(); }

  std::shared_ptr<spanner_testing::MockInstanceAdminStub> mock_;

 private:
  std::shared_ptr<google::cloud::testing_util::CaptureLogLinesBackend> backend_;
  long logger_id_ = 0;  // NOLINT
};

TEST_F(InstanceAdminLoggingTest, GetInstance) {
  EXPECT_CALL(*mock_, GetInstance(_, _)).WillOnce(Return(TransientError()));

  InstanceAdminLogging stub(mock_, TracingOptions{});

  grpc::ClientContext context;
  auto response = stub.GetInstance(context, gcsa::GetInstanceRequest{});
  EXPECT_EQ(TransientError(), response.status());

  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetInstance")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(InstanceAdminLoggingTest, CreateInstance) {
  EXPECT_CALL(*mock_, CreateInstance(_, _)).WillOnce(Return(TransientError()));

  InstanceAdminLogging stub(mock_, TracingOptions{});

  grpc::ClientContext context;
  auto response = stub.CreateInstance(context, gcsa::CreateInstanceRequest{});
  EXPECT_EQ(TransientError(), response.status());

  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("CreateInstance")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(InstanceAdminLoggingTest, UpdateInstance) {
  EXPECT_CALL(*mock_, UpdateInstance(_, _)).WillOnce(Return(TransientError()));

  InstanceAdminLogging stub(mock_, TracingOptions{});

  grpc::ClientContext context;
  auto response = stub.UpdateInstance(context, gcsa::UpdateInstanceRequest{});
  EXPECT_EQ(TransientError(), response.status());

  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("UpdateInstance")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(InstanceAdminLoggingTest, DeleteInstance) {
  EXPECT_CALL(*mock_, DeleteInstance(_, _)).WillOnce(Return(TransientError()));

  InstanceAdminLogging stub(mock_, TracingOptions{});

  grpc::ClientContext context;
  auto status = stub.DeleteInstance(context, gcsa::DeleteInstanceRequest{});
  EXPECT_EQ(TransientError(), status);

  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("DeleteInstance")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(InstanceAdminLoggingTest, GetInstanceConfig) {
  EXPECT_CALL(*mock_, GetInstanceConfig(_, _))
      .WillOnce(Return(TransientError()));

  InstanceAdminLogging stub(mock_, TracingOptions{});

  grpc::ClientContext context;
  auto response =
      stub.GetInstanceConfig(context, gcsa::GetInstanceConfigRequest{});
  EXPECT_EQ(TransientError(), response.status());

  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetInstanceConfig")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(InstanceAdminLoggingTest, ListInstanceConfigs) {
  EXPECT_CALL(*mock_, ListInstanceConfigs(_, _))
      .WillOnce(Return(TransientError()));

  InstanceAdminLogging stub(mock_, TracingOptions{});

  grpc::ClientContext context;
  auto response =
      stub.ListInstanceConfigs(context, gcsa::ListInstanceConfigsRequest{});
  EXPECT_EQ(TransientError(), response.status());

  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListInstanceConfigs")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(InstanceAdminLoggingTest, ListInstances) {
  EXPECT_CALL(*mock_, ListInstances(_, _)).WillOnce(Return(TransientError()));

  InstanceAdminLogging stub(mock_, TracingOptions{});

  grpc::ClientContext context;
  auto response = stub.ListInstances(context, gcsa::ListInstancesRequest{});
  EXPECT_EQ(TransientError(), response.status());

  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListInstances")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(InstanceAdminLoggingTest, GetIamPolicy) {
  EXPECT_CALL(*mock_, GetIamPolicy(_, _)).WillOnce(Return(TransientError()));

  InstanceAdminLogging stub(mock_, TracingOptions{});

  grpc::ClientContext context;
  auto response =
      stub.GetIamPolicy(context, google::iam::v1::GetIamPolicyRequest{});
  EXPECT_EQ(TransientError(), response.status());

  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetIamPolicy")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(InstanceAdminLoggingTest, SetIamPolicy) {
  EXPECT_CALL(*mock_, SetIamPolicy(_, _)).WillOnce(Return(TransientError()));

  InstanceAdminLogging stub(mock_, TracingOptions{});

  grpc::ClientContext context;
  auto response =
      stub.SetIamPolicy(context, google::iam::v1::SetIamPolicyRequest{});
  EXPECT_EQ(TransientError(), response.status());

  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("SetIamPolicy")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(InstanceAdminLoggingTest, TestIamPermissions) {
  EXPECT_CALL(*mock_, TestIamPermissions(_, _))
      .WillOnce(Return(TransientError()));

  InstanceAdminLogging stub(mock_, TracingOptions{});

  grpc::ClientContext context;
  auto response = stub.TestIamPermissions(
      context, google::iam::v1::TestIamPermissionsRequest{});
  EXPECT_EQ(TransientError(), response.status());

  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("TestIamPermissions")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
