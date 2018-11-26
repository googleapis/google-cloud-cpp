// Copyright 2018 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_INSTANCE_ADMIN_CLIENT_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_INSTANCE_ADMIN_CLIENT_H_

#include "google/cloud/bigtable/instance_admin_client.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
namespace testing {

class MockInstanceAdminClient : public bigtable::InstanceAdminClient {
 public:
  MOCK_CONST_METHOD0(project, std::string const&());
  MOCK_METHOD0(Channel, std::shared_ptr<grpc::Channel>());
  MOCK_METHOD0(reset, void());
  MOCK_METHOD3(
      ListInstances,
      grpc::Status(grpc::ClientContext*,
                   google::bigtable::admin::v2::ListInstancesRequest const&,
                   google::bigtable::admin::v2::ListInstancesResponse*));
  MOCK_METHOD3(
      CreateInstance,
      grpc::Status(grpc::ClientContext*,
                   google::bigtable::admin::v2::CreateInstanceRequest const&,
                   google::longrunning::Operation*));

  MOCK_METHOD3(
      AsyncCreateInstance,
      std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
          google::longrunning::Operation>>(
          grpc::ClientContext* context,
          const google::bigtable::admin::v2::CreateInstanceRequest& request,
          grpc::CompletionQueue* cq));

  MOCK_METHOD3(
      UpdateInstance,
      grpc::Status(
          grpc::ClientContext*,
          google::bigtable::admin::v2::PartialUpdateInstanceRequest const&,
          google::longrunning::Operation*));

  MOCK_METHOD3(
      AsyncUpdateInstance,
      std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
          google::longrunning::Operation>>(
          grpc::ClientContext* context,
          const google::bigtable::admin::v2::PartialUpdateInstanceRequest&
              request,
          grpc::CompletionQueue* cq));

  MOCK_METHOD3(GetOperation,
               grpc::Status(grpc::ClientContext*,
                            google::longrunning::GetOperationRequest const&,
                            google::longrunning::Operation*));

  MOCK_METHOD3(
      GetInstance,
      grpc::Status(grpc::ClientContext*,
                   google::bigtable::admin::v2::GetInstanceRequest const&,
                   google::bigtable::admin::v2::Instance*));

  MOCK_METHOD3(
      AsyncGetInstance,
      std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
          google::bigtable::admin::v2::Instance>>(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::GetInstanceRequest const& request,
          grpc::CompletionQueue* cq));

  MOCK_METHOD3(
      DeleteInstance,
      grpc::Status(grpc::ClientContext*,
                   google::bigtable::admin::v2::DeleteInstanceRequest const&,
                   google::protobuf::Empty*));

  MOCK_METHOD3(
      AsyncDeleteInstance,
      std::unique_ptr<
          grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::DeleteInstanceRequest const& request,
          grpc::CompletionQueue* cq));

  MOCK_METHOD3(
      ListClusters,
      grpc::Status(grpc::ClientContext*,
                   google::bigtable::admin::v2::ListClustersRequest const&,
                   google::bigtable::admin::v2::ListClustersResponse*));

  MOCK_METHOD3(AsyncListClusters,
               std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                   google::bigtable::admin::v2::ListClustersResponse>>(
                   grpc::ClientContext*,
                   const google::bigtable::admin::v2::ListClustersRequest&,
                   grpc::CompletionQueue*));

  MOCK_METHOD3(
      GetCluster,
      grpc::Status(grpc::ClientContext*,
                   google::bigtable::admin::v2::GetClusterRequest const&,
                   google::bigtable::admin::v2::Cluster*));

  MOCK_METHOD3(
      AsyncGetCluster,
      std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
          google::bigtable::admin::v2::Cluster>>(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::GetClusterRequest const& request,
          grpc::CompletionQueue* cq));

  MOCK_METHOD3(
      DeleteCluster,
      grpc::Status(grpc::ClientContext*,
                   google::bigtable::admin::v2::DeleteClusterRequest const&,
                   google::protobuf::Empty*));

  MOCK_METHOD3(
      AsyncDeleteCluster,
      std::unique_ptr<
          grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::DeleteClusterRequest const& request,
          grpc::CompletionQueue* cq));

  MOCK_METHOD3(
      AsyncCreateCluster,
      std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
          google::longrunning::Operation>>(
          grpc::ClientContext* context,
          const google::bigtable::admin::v2::CreateClusterRequest& request,
          grpc::CompletionQueue* cq));

  MOCK_METHOD3(
      CreateCluster,
      grpc::Status(grpc::ClientContext*,
                   google::bigtable::admin::v2::CreateClusterRequest const&,
                   google::longrunning::Operation*));

  MOCK_METHOD3(UpdateCluster,
               grpc::Status(grpc::ClientContext*,
                            google::bigtable::admin::v2::Cluster const&,
                            google::longrunning::Operation*));

  MOCK_METHOD3(AsyncUpdateCluster,
               std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                   google::longrunning::Operation>>(
                   grpc::ClientContext* context,
                   const google::bigtable::admin::v2::Cluster& request,
                   grpc::CompletionQueue* cq));

  MOCK_METHOD3(
      CreateAppProfile,
      grpc::Status(grpc::ClientContext*,
                   google::bigtable::admin::v2::CreateAppProfileRequest const&,
                   google::bigtable::admin::v2::AppProfile*));

  MOCK_METHOD3(
      AsyncCreateAppProfile,
      std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
          google::bigtable::admin::v2::AppProfile>>(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::CreateAppProfileRequest const& request,
          grpc::CompletionQueue* cq));

  MOCK_METHOD3(
      GetAppProfile,
      grpc::Status(grpc::ClientContext*,
                   google::bigtable::admin::v2::GetAppProfileRequest const&,
                   google::bigtable::admin::v2::AppProfile*));

  MOCK_METHOD3(
      AsyncGetAppProfile,
      std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
          google::bigtable::admin::v2::AppProfile>>(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::GetAppProfileRequest const& request,
          grpc::CompletionQueue* cq));

  MOCK_METHOD3(
      ListAppProfiles,
      grpc::Status(grpc::ClientContext*,
                   google::bigtable::admin::v2::ListAppProfilesRequest const&,
                   google::bigtable::admin::v2::ListAppProfilesResponse*));

  MOCK_METHOD3(
      UpdateAppProfile,
      grpc::Status(grpc::ClientContext*,
                   google::bigtable::admin::v2::UpdateAppProfileRequest const&,
                   google::longrunning::Operation*));

  MOCK_METHOD3(
      DeleteAppProfile,
      grpc::Status(grpc::ClientContext*,
                   google::bigtable::admin::v2::DeleteAppProfileRequest const&,
                   google::protobuf::Empty*));

  MOCK_METHOD3(
      AsyncUpdateAppProfile,
      std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
          google::longrunning::Operation>>(
          grpc::ClientContext* context,
          const google::bigtable::admin::v2::UpdateAppProfileRequest& request,
          grpc::CompletionQueue* cq));

  MOCK_METHOD3(
      AsyncDeleteAppProfile,
      std::unique_ptr<
          grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::DeleteAppProfileRequest const& request,
          grpc::CompletionQueue* cq));

  MOCK_METHOD3(GetIamPolicy,
               grpc::Status(grpc::ClientContext*,
                            google::iam::v1::GetIamPolicyRequest const&,
                            google::iam::v1::Policy*));

  MOCK_METHOD3(SetIamPolicy,
               grpc::Status(grpc::ClientContext*,
                            google::iam::v1::SetIamPolicyRequest const&,
                            google::iam::v1::Policy*));

  MOCK_METHOD3(TestIamPermissions,
               grpc::Status(grpc::ClientContext*,
                            google::iam::v1::TestIamPermissionsRequest const&,
                            google::iam::v1::TestIamPermissionsResponse*));
  MOCK_METHOD3(AsyncGetOperation,
               std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                   google::longrunning::Operation>>(
                   grpc::ClientContext* context,
                   const google::longrunning::GetOperationRequest& request,
                   grpc::CompletionQueue* cq));
};

}  // namespace testing
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_INSTANCE_ADMIN_CLIENT_H_
