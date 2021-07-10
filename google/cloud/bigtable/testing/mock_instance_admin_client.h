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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_INSTANCE_ADMIN_CLIENT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_INSTANCE_ADMIN_CLIENT_H

#include "google/cloud/bigtable/instance_admin_client.h"
#include <gmock/gmock.h>
#include <string>

namespace google {
namespace cloud {
namespace bigtable {
namespace testing {

class MockInstanceAdminClient : public bigtable::InstanceAdminClient {
 public:
  explicit MockInstanceAdminClient(ClientOptions options = {})
      : options_(std::move(options)) {}

  MOCK_METHOD(std::string const&, project, (), (const, override));
  MOCK_METHOD(std::shared_ptr<grpc::Channel>, Channel, (), (override));
  MOCK_METHOD(void, reset, (), (override));
  MOCK_METHOD(grpc::Status, ListInstances,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::ListInstancesRequest const&,
               google::bigtable::admin::v2::ListInstancesResponse*),
              (override));

  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                  google::bigtable::admin::v2::ListInstancesResponse>>,
              AsyncListInstances,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::ListInstancesRequest const&,
               grpc::CompletionQueue*),
              (override));

  MOCK_METHOD(grpc::Status, CreateInstance,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::CreateInstanceRequest const&,
               google::longrunning::Operation*),
              (override));

  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                  google::longrunning::Operation>>,
              AsyncCreateInstance,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::CreateInstanceRequest const&,
               grpc::CompletionQueue*),
              (override));

  MOCK_METHOD(grpc::Status, UpdateInstance,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::PartialUpdateInstanceRequest const&,
               google::longrunning::Operation*),
              (override));

  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                  google::longrunning::Operation>>,
              AsyncUpdateInstance,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::PartialUpdateInstanceRequest const&,
               grpc::CompletionQueue*),
              (override));

  MOCK_METHOD(grpc::Status, GetOperation,
              (grpc::ClientContext*,
               google::longrunning::GetOperationRequest const&,
               google::longrunning::Operation*),
              (override));

  MOCK_METHOD(grpc::Status, GetInstance,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::GetInstanceRequest const&,
               google::bigtable::admin::v2::Instance*),
              (override));

  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                  google::bigtable::admin::v2::Instance>>,
              AsyncGetInstance,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::GetInstanceRequest const&,
               grpc::CompletionQueue*),
              (override));

  MOCK_METHOD(grpc::Status, DeleteInstance,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::DeleteInstanceRequest const&,
               google::protobuf::Empty*),
              (override));

  MOCK_METHOD(
      std::unique_ptr<
          grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>,
      AsyncDeleteInstance,
      (grpc::ClientContext*,
       google::bigtable::admin::v2::DeleteInstanceRequest const&,
       grpc::CompletionQueue*),
      (override));

  MOCK_METHOD(grpc::Status, ListClusters,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::ListClustersRequest const&,
               google::bigtable::admin::v2::ListClustersResponse*),
              (override));

  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                  google::bigtable::admin::v2::ListClustersResponse>>,
              AsyncListClusters,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::ListClustersRequest const&,
               grpc::CompletionQueue*),
              (override));

  MOCK_METHOD(grpc::Status, GetCluster,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::GetClusterRequest const&,
               google::bigtable::admin::v2::Cluster*),
              (override));

  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                  google::bigtable::admin::v2::Cluster>>,
              AsyncGetCluster,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::GetClusterRequest const&,
               grpc::CompletionQueue*),
              (override));

  MOCK_METHOD(grpc::Status, DeleteCluster,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::DeleteClusterRequest const&,
               google::protobuf::Empty*),
              (override));

  MOCK_METHOD(
      std::unique_ptr<
          grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>,
      AsyncDeleteCluster,
      (grpc::ClientContext*,
       google::bigtable::admin::v2::DeleteClusterRequest const&,
       grpc::CompletionQueue*),
      (override));

  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                  google::longrunning::Operation>>,
              AsyncCreateCluster,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::CreateClusterRequest const&,
               grpc::CompletionQueue*),
              (override));

  MOCK_METHOD(grpc::Status, CreateCluster,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::CreateClusterRequest const&,
               google::longrunning::Operation*),
              (override));

  MOCK_METHOD(grpc::Status, UpdateCluster,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::Cluster const&,
               google::longrunning::Operation*),
              (override));

  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                  google::longrunning::Operation>>,
              AsyncUpdateCluster,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::Cluster const&,
               grpc::CompletionQueue*),
              (override));

  MOCK_METHOD(grpc::Status, CreateAppProfile,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::CreateAppProfileRequest const&,
               google::bigtable::admin::v2::AppProfile*),
              (override));

  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                  google::bigtable::admin::v2::AppProfile>>,
              AsyncCreateAppProfile,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::CreateAppProfileRequest const&,
               grpc::CompletionQueue*),
              (override));

  MOCK_METHOD(grpc::Status, GetAppProfile,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::GetAppProfileRequest const&,
               google::bigtable::admin::v2::AppProfile*),
              (override));

  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                  google::bigtable::admin::v2::AppProfile>>,
              AsyncGetAppProfile,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::GetAppProfileRequest const&,
               grpc::CompletionQueue*),
              (override));

  MOCK_METHOD(grpc::Status, ListAppProfiles,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::ListAppProfilesRequest const&,
               google::bigtable::admin::v2::ListAppProfilesResponse*),
              (override));

  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                  google::bigtable::admin::v2::ListAppProfilesResponse>>,
              AsyncListAppProfiles,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::ListAppProfilesRequest const&,
               grpc::CompletionQueue*),
              (override));

  MOCK_METHOD(grpc::Status, UpdateAppProfile,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::UpdateAppProfileRequest const&,
               google::longrunning::Operation*),
              (override));

  MOCK_METHOD(grpc::Status, DeleteAppProfile,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::DeleteAppProfileRequest const&,
               google::protobuf::Empty*),
              (override));

  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                  google::longrunning::Operation>>,
              AsyncUpdateAppProfile,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::UpdateAppProfileRequest const&,
               grpc::CompletionQueue*),
              (override));

  MOCK_METHOD(
      std::unique_ptr<
          grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>,
      AsyncDeleteAppProfile,
      (grpc::ClientContext*,
       google::bigtable::admin::v2::DeleteAppProfileRequest const&,
       grpc::CompletionQueue*),
      (override));

  MOCK_METHOD(grpc::Status, GetIamPolicy,
              (grpc::ClientContext*,
               google::iam::v1::GetIamPolicyRequest const&,
               google::iam::v1::Policy*),
              (override));

  MOCK_METHOD(
      std::unique_ptr<
          grpc::ClientAsyncResponseReaderInterface<google::iam::v1::Policy>>,
      AsyncGetIamPolicy,
      (grpc::ClientContext*, google::iam::v1::GetIamPolicyRequest const&,
       grpc::CompletionQueue*),
      (override));

  MOCK_METHOD(grpc::Status, SetIamPolicy,
              (grpc::ClientContext*,
               google::iam::v1::SetIamPolicyRequest const&,
               google::iam::v1::Policy*),
              (override));

  MOCK_METHOD(
      std::unique_ptr<
          grpc::ClientAsyncResponseReaderInterface<google::iam::v1::Policy>>,
      AsyncSetIamPolicy,
      (grpc::ClientContext*, google::iam::v1::SetIamPolicyRequest const&,
       grpc::CompletionQueue*),
      (override));

  MOCK_METHOD(grpc::Status, TestIamPermissions,
              (grpc::ClientContext*,
               google::iam::v1::TestIamPermissionsRequest const&,
               google::iam::v1::TestIamPermissionsResponse*),
              (override));
  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                  google::iam::v1::TestIamPermissionsResponse>>,
              AsyncTestIamPermissions,
              (grpc::ClientContext*,
               google::iam::v1::TestIamPermissionsRequest const&,
               grpc::CompletionQueue*),
              (override));
  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                  google::longrunning::Operation>>,
              AsyncGetOperation,
              (grpc::ClientContext*,
               google::longrunning::GetOperationRequest const&,
               grpc::CompletionQueue*),
              (override));

 private:
  google::cloud::BackgroundThreadsFactory BackgroundThreadsFactory() override {
    return options_.background_threads_factory();
  }

  ClientOptions options_;
};

}  // namespace testing
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_INSTANCE_ADMIN_CLIENT_H
