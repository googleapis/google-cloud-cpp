// Copyright 2020 Google Inc.
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

#include "google/cloud/bigtable/internal/logging_instance_admin_client.h"
#include "google/cloud/bigtable/instance_admin_client.h"
#include "google/cloud/bigtable/testing/mock_instance_admin_client.h"
#include "google/cloud/bigtable/testing/mock_response_reader.h"
#include "google/cloud/testing_util/mock_async_response_reader.h"
#include "google/cloud/testing_util/scoped_log.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace {

using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Return;

using MockAsyncLongrunningOpReader =
    ::google::cloud::testing_util::MockAsyncResponseReader<
        google::longrunning::Operation>;

namespace btadmin = ::google::bigtable::admin::v2;

class LoggingInstanceAdminClientTest : public ::testing::Test {
 protected:
  static Status TransientError() {
    return Status(StatusCode::kUnavailable, "try-again");
  }

  testing_util::ScopedLog log_;
};

TEST_F(LoggingInstanceAdminClientTest, ListInstances) {
  auto mock = std::make_shared<testing::MockInstanceAdminClient>();

  EXPECT_CALL(*mock, ListInstances).WillOnce(Return(grpc::Status()));

  internal::LoggingInstanceAdminClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  btadmin::ListInstancesRequest request;
  btadmin::ListInstancesResponse response;

  auto status = stub.ListInstances(&context, request, &response);

  EXPECT_TRUE(status.ok());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("ListInstances")));
}

TEST_F(LoggingInstanceAdminClientTest, CreateInstance) {
  auto mock = std::make_shared<testing::MockInstanceAdminClient>();

  EXPECT_CALL(*mock, CreateInstance).WillOnce(Return(grpc::Status()));

  internal::LoggingInstanceAdminClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  btadmin::CreateInstanceRequest request;
  google::longrunning::Operation response;

  auto status = stub.CreateInstance(&context, request, &response);

  EXPECT_TRUE(status.ok());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("CreateInstance")));
}

TEST_F(LoggingInstanceAdminClientTest, UpdateInstance) {
  auto mock = std::make_shared<testing::MockInstanceAdminClient>();

  EXPECT_CALL(*mock, UpdateInstance).WillOnce(Return(grpc::Status()));

  internal::LoggingInstanceAdminClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  btadmin::PartialUpdateInstanceRequest request;
  google::longrunning::Operation response;

  auto status = stub.UpdateInstance(&context, request, &response);

  EXPECT_TRUE(status.ok());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("UpdateInstance")));
}

TEST_F(LoggingInstanceAdminClientTest, GetOperation) {
  auto mock = std::make_shared<testing::MockInstanceAdminClient>();

  EXPECT_CALL(*mock, GetOperation).WillOnce(Return(grpc::Status()));

  internal::LoggingInstanceAdminClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  google::longrunning::GetOperationRequest request;
  google::longrunning::Operation response;

  auto status = stub.GetOperation(&context, request, &response);

  EXPECT_TRUE(status.ok());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("GetOperation")));
}

TEST_F(LoggingInstanceAdminClientTest, GetInstance) {
  auto mock = std::make_shared<testing::MockInstanceAdminClient>();

  EXPECT_CALL(*mock, GetInstance).WillOnce(Return(grpc::Status()));

  internal::LoggingInstanceAdminClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  btadmin::GetInstanceRequest request;
  btadmin::Instance response;

  auto status = stub.GetInstance(&context, request, &response);

  EXPECT_TRUE(status.ok());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("GetInstance")));
}

TEST_F(LoggingInstanceAdminClientTest, DeleteInstance) {
  auto mock = std::make_shared<testing::MockInstanceAdminClient>();

  EXPECT_CALL(*mock, DeleteInstance).WillOnce(Return(grpc::Status()));

  internal::LoggingInstanceAdminClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  btadmin::DeleteInstanceRequest request;
  google::protobuf::Empty response;

  auto status = stub.DeleteInstance(&context, request, &response);

  EXPECT_TRUE(status.ok());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("DeleteInstance")));
}

TEST_F(LoggingInstanceAdminClientTest, ListClusters) {
  auto mock = std::make_shared<testing::MockInstanceAdminClient>();

  EXPECT_CALL(*mock, ListClusters).WillOnce(Return(grpc::Status()));

  internal::LoggingInstanceAdminClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  btadmin::ListClustersRequest request;
  btadmin::ListClustersResponse response;

  auto status = stub.ListClusters(&context, request, &response);

  EXPECT_TRUE(status.ok());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("ListClusters")));
}

TEST_F(LoggingInstanceAdminClientTest, GetCluster) {
  auto mock = std::make_shared<testing::MockInstanceAdminClient>();

  EXPECT_CALL(*mock, GetCluster).WillOnce(Return(grpc::Status()));

  internal::LoggingInstanceAdminClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  btadmin::GetClusterRequest request;
  btadmin::Cluster response;

  auto status = stub.GetCluster(&context, request, &response);

  EXPECT_TRUE(status.ok());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("GetCluster")));
}

TEST_F(LoggingInstanceAdminClientTest, DeleteCluster) {
  auto mock = std::make_shared<testing::MockInstanceAdminClient>();

  EXPECT_CALL(*mock, DeleteCluster).WillOnce(Return(grpc::Status()));

  internal::LoggingInstanceAdminClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  btadmin::DeleteClusterRequest request;
  google::protobuf::Empty response;

  auto status = stub.DeleteCluster(&context, request, &response);

  EXPECT_TRUE(status.ok());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("DeleteCluster")));
}

TEST_F(LoggingInstanceAdminClientTest, CreateCluster) {
  auto mock = std::make_shared<testing::MockInstanceAdminClient>();

  EXPECT_CALL(*mock, CreateCluster).WillOnce(Return(grpc::Status()));

  internal::LoggingInstanceAdminClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  btadmin::CreateClusterRequest request;
  google::longrunning::Operation response;

  auto status = stub.CreateCluster(&context, request, &response);

  EXPECT_TRUE(status.ok());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("CreateCluster")));
}

TEST_F(LoggingInstanceAdminClientTest, UpdateCluster) {
  auto mock = std::make_shared<testing::MockInstanceAdminClient>();

  EXPECT_CALL(*mock, UpdateCluster).WillOnce(Return(grpc::Status()));

  internal::LoggingInstanceAdminClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  btadmin::Cluster request;
  google::longrunning::Operation response;

  auto status = stub.UpdateCluster(&context, request, &response);

  EXPECT_TRUE(status.ok());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("UpdateCluster")));
}

TEST_F(LoggingInstanceAdminClientTest, CreateAppProfile) {
  auto mock = std::make_shared<testing::MockInstanceAdminClient>();

  EXPECT_CALL(*mock, CreateAppProfile).WillOnce(Return(grpc::Status()));

  internal::LoggingInstanceAdminClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  btadmin::CreateAppProfileRequest request;
  btadmin::AppProfile response;

  auto status = stub.CreateAppProfile(&context, request, &response);

  EXPECT_TRUE(status.ok());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("CreateAppProfile")));
}

TEST_F(LoggingInstanceAdminClientTest, GetAppProfile) {
  auto mock = std::make_shared<testing::MockInstanceAdminClient>();

  EXPECT_CALL(*mock, GetAppProfile).WillOnce(Return(grpc::Status()));

  internal::LoggingInstanceAdminClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  btadmin::GetAppProfileRequest request;
  btadmin::AppProfile response;

  auto status = stub.GetAppProfile(&context, request, &response);

  EXPECT_TRUE(status.ok());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("GetAppProfile")));
}

TEST_F(LoggingInstanceAdminClientTest, ListAppProfiles) {
  auto mock = std::make_shared<testing::MockInstanceAdminClient>();

  EXPECT_CALL(*mock, ListAppProfiles).WillOnce(Return(grpc::Status()));

  internal::LoggingInstanceAdminClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  btadmin::ListAppProfilesRequest request;
  btadmin::ListAppProfilesResponse response;

  auto status = stub.ListAppProfiles(&context, request, &response);

  EXPECT_TRUE(status.ok());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("ListAppProfiles")));
}

TEST_F(LoggingInstanceAdminClientTest, UpdateAppProfile) {
  auto mock = std::make_shared<testing::MockInstanceAdminClient>();

  EXPECT_CALL(*mock, UpdateAppProfile).WillOnce(Return(grpc::Status()));

  internal::LoggingInstanceAdminClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  btadmin::UpdateAppProfileRequest request;
  google::longrunning::Operation response;

  auto status = stub.UpdateAppProfile(&context, request, &response);

  EXPECT_TRUE(status.ok());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("UpdateAppProfile")));
}

TEST_F(LoggingInstanceAdminClientTest, DeleteAppProfile) {
  auto mock = std::make_shared<testing::MockInstanceAdminClient>();

  EXPECT_CALL(*mock, DeleteAppProfile).WillOnce(Return(grpc::Status()));

  internal::LoggingInstanceAdminClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  btadmin::DeleteAppProfileRequest request;
  google::protobuf::Empty response;

  auto status = stub.DeleteAppProfile(&context, request, &response);

  EXPECT_TRUE(status.ok());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("DeleteAppProfile")));
}

TEST_F(LoggingInstanceAdminClientTest, GetIamPolicy) {
  auto mock = std::make_shared<testing::MockInstanceAdminClient>();

  EXPECT_CALL(*mock, GetIamPolicy).WillOnce(Return(grpc::Status()));

  internal::LoggingInstanceAdminClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  google::iam::v1::GetIamPolicyRequest request;
  google::iam::v1::Policy response;

  auto status = stub.GetIamPolicy(&context, request, &response);

  EXPECT_TRUE(status.ok());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("GetIamPolicy")));
}

TEST_F(LoggingInstanceAdminClientTest, SetIamPolicy) {
  auto mock = std::make_shared<testing::MockInstanceAdminClient>();

  EXPECT_CALL(*mock, SetIamPolicy).WillOnce(Return(grpc::Status()));

  internal::LoggingInstanceAdminClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  google::iam::v1::SetIamPolicyRequest request;
  google::iam::v1::Policy response;

  auto status = stub.SetIamPolicy(&context, request, &response);

  EXPECT_TRUE(status.ok());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("SetIamPolicy")));
}

TEST_F(LoggingInstanceAdminClientTest, TestIamPermissions) {
  auto mock = std::make_shared<testing::MockInstanceAdminClient>();

  EXPECT_CALL(*mock, TestIamPermissions).WillOnce(Return(grpc::Status()));

  internal::LoggingInstanceAdminClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  google::iam::v1::TestIamPermissionsRequest request;
  google::iam::v1::TestIamPermissionsResponse response;

  auto status = stub.TestIamPermissions(&context, request, &response);

  EXPECT_TRUE(status.ok());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("TestIamPermissions")));
}

TEST_F(LoggingInstanceAdminClientTest, AsyncCreateInstance) {
  auto reader = absl::make_unique<MockAsyncLongrunningOpReader>();
  auto mock = std::make_shared<testing::MockInstanceAdminClient>();

  EXPECT_CALL(*mock, AsyncCreateInstance)
      .WillOnce([&reader](grpc::ClientContext*,
                          btadmin::CreateInstanceRequest const&,
                          grpc::CompletionQueue*) {
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            google::longrunning::Operation>>(reader.get());
      });

  internal::LoggingInstanceAdminClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  btadmin::CreateInstanceRequest request;
  grpc::CompletionQueue cq;

  stub.AsyncCreateInstance(&context, request, &cq);

  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("AsyncCreateInstance")));
}

TEST_F(LoggingInstanceAdminClientTest, AsyncUpdateInstance) {
  auto reader = absl::make_unique<MockAsyncLongrunningOpReader>();
  auto mock = std::make_shared<testing::MockInstanceAdminClient>();

  EXPECT_CALL(*mock, AsyncUpdateInstance)
      .WillOnce([&reader](grpc::ClientContext*,
                          btadmin::PartialUpdateInstanceRequest const&,
                          grpc::CompletionQueue*) {
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            google::longrunning::Operation>>(reader.get());
      });

  internal::LoggingInstanceAdminClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  btadmin::PartialUpdateInstanceRequest request;
  grpc::CompletionQueue cq;

  stub.AsyncUpdateInstance(&context, request, &cq);

  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("AsyncUpdateInstance")));
}

}  // namespace
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
