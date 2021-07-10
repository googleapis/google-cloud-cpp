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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_LOGGING_INSTANCE_ADMIN_CLIENT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_LOGGING_INSTANCE_ADMIN_CLIENT_H

#include "google/cloud/bigtable/instance_admin_client.h"
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

/**
 * Implements a logging InstanceAdminClient.
 *
 * This class is used by the Cloud Bigtable wrappers to access Cloud Bigtable.
 * Multiple `bigtable::InstanceAdmin` objects may share a connection via a
 * single `InstanceAdminClient` object. The `InstanceAdminClient` object is
 * configured at construction time, this configuration includes the credentials,
 * access endpoints, default timeouts, and other gRPC configuration options.
 * This is an interface class because it is also used as a dependency injection
 * point in some of the tests.
 *
 * @par Cost
 * Applications should avoid unnecessarily creating new objects of type
 * `InstanceAdminClient`. Creating a new object of this type typically requires
 * connecting to the Cloud Bigtable servers, and performing the authentication
 * workflows with Google Cloud Platform. These operations can take many
 * milliseconds, therefore applications should try to reuse the same
 * `InstanceAdminClient` instances when possible.
 */

class LoggingInstanceAdminClient
    : public google::cloud::bigtable::InstanceAdminClient {
 public:
  LoggingInstanceAdminClient(
      std::shared_ptr<google::cloud::bigtable::InstanceAdminClient> child,
      google::cloud::TracingOptions options)
      : child_(std::move(child)), tracing_options_(std::move(options)) {}

  std::string const& project() const override { return child_->project(); }

  std::shared_ptr<grpc::Channel> Channel() override {
    return child_->Channel();
  }

  void reset() override { child_->reset(); }

  grpc::Status ListInstances(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::ListInstancesRequest const& request,
      google::bigtable::admin::v2::ListInstancesResponse* response) override;

  grpc::Status CreateInstance(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::CreateInstanceRequest const& request,
      google::longrunning::Operation* response) override;

  grpc::Status UpdateInstance(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::PartialUpdateInstanceRequest const& request,
      google::longrunning::Operation* response) override;

  grpc::Status GetOperation(
      grpc::ClientContext* context,
      google::longrunning::GetOperationRequest const& request,
      google::longrunning::Operation* response) override;

  grpc::Status GetInstance(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::GetInstanceRequest const& request,
      google::bigtable::admin::v2::Instance* response) override;

  grpc::Status DeleteInstance(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::DeleteInstanceRequest const& request,
      google::protobuf::Empty* response) override;

  grpc::Status ListClusters(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::ListClustersRequest const& request,
      google::bigtable::admin::v2::ListClustersResponse* response) override;

  grpc::Status GetCluster(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::GetClusterRequest const& request,
      google::bigtable::admin::v2::Cluster* response) override;

  grpc::Status DeleteCluster(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::DeleteClusterRequest const& request,
      google::protobuf::Empty* response) override;

  grpc::Status CreateCluster(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::CreateClusterRequest const& request,
      google::longrunning::Operation* response) override;

  grpc::Status UpdateCluster(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::Cluster const& request,
      google::longrunning::Operation* response) override;

  grpc::Status CreateAppProfile(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::CreateAppProfileRequest const& request,
      google::bigtable::admin::v2::AppProfile* response) override;

  grpc::Status GetAppProfile(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::GetAppProfileRequest const& request,
      google::bigtable::admin::v2::AppProfile* response) override;

  grpc::Status ListAppProfiles(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::ListAppProfilesRequest const& request,
      google::bigtable::admin::v2::ListAppProfilesResponse* response) override;

  grpc::Status UpdateAppProfile(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::UpdateAppProfileRequest const& request,
      google::longrunning::Operation* response) override;

  grpc::Status DeleteAppProfile(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::DeleteAppProfileRequest const& request,
      google::protobuf::Empty* response) override;

  grpc::Status GetIamPolicy(grpc::ClientContext* context,
                            google::iam::v1::GetIamPolicyRequest const& request,
                            google::iam::v1::Policy* response) override;

  grpc::Status SetIamPolicy(grpc::ClientContext* context,
                            google::iam::v1::SetIamPolicyRequest const& request,
                            google::iam::v1::Policy* response) override;

  grpc::Status TestIamPermissions(
      grpc::ClientContext* context,
      google::iam::v1::TestIamPermissionsRequest const& request,
      google::iam::v1::TestIamPermissionsResponse* response) override;

  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::admin::v2::ListInstancesResponse>>
  AsyncListInstances(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::ListInstancesRequest const& request,
      grpc::CompletionQueue* cq) override;

  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::admin::v2::Instance>>
  AsyncGetInstance(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::GetInstanceRequest const& request,
      grpc::CompletionQueue* cq) override;

  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::admin::v2::Cluster>>
  AsyncGetCluster(grpc::ClientContext* context,
                  google::bigtable::admin::v2::GetClusterRequest const& request,
                  grpc::CompletionQueue* cq) override;

  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>
  AsyncDeleteCluster(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::DeleteClusterRequest const& request,
      grpc::CompletionQueue* cq) override;

  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::longrunning::Operation>>
  AsyncCreateCluster(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::CreateClusterRequest const& request,
      grpc::CompletionQueue* cq) override;

  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::longrunning::Operation>>
  AsyncCreateInstance(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::CreateInstanceRequest const& request,
      grpc::CompletionQueue* cq) override;

  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::longrunning::Operation>>
  AsyncUpdateInstance(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::PartialUpdateInstanceRequest const& request,
      grpc::CompletionQueue* cq) override;

  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::longrunning::Operation>>
  AsyncUpdateCluster(grpc::ClientContext* context,
                     google::bigtable::admin::v2::Cluster const& request,
                     grpc::CompletionQueue* cq) override;

  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>
  AsyncDeleteInstance(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::DeleteInstanceRequest const& request,
      grpc::CompletionQueue* cq) override;

  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::admin::v2::ListClustersResponse>>
  AsyncListClusters(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::ListClustersRequest const& request,
      grpc::CompletionQueue* cq) override;

  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::admin::v2::AppProfile>>
  AsyncGetAppProfile(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::GetAppProfileRequest const& request,
      grpc::CompletionQueue* cq) override;

  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>
  AsyncDeleteAppProfile(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::DeleteAppProfileRequest const& request,
      grpc::CompletionQueue* cq) override;

  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::admin::v2::AppProfile>>
  AsyncCreateAppProfile(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::CreateAppProfileRequest const& request,
      grpc::CompletionQueue* cq) override;

  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::longrunning::Operation>>
  AsyncUpdateAppProfile(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::UpdateAppProfileRequest const& request,
      grpc::CompletionQueue* cq) override;

  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::admin::v2::ListAppProfilesResponse>>
  AsyncListAppProfiles(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::ListAppProfilesRequest const& request,
      grpc::CompletionQueue* cq) override;

  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::iam::v1::Policy>>
  AsyncGetIamPolicy(grpc::ClientContext* context,
                    google::iam::v1::GetIamPolicyRequest const& request,
                    grpc::CompletionQueue* cq) override;

  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::iam::v1::Policy>>
  AsyncSetIamPolicy(grpc::ClientContext* context,
                    google::iam::v1::SetIamPolicyRequest const& request,
                    grpc::CompletionQueue* cq) override;

  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::iam::v1::TestIamPermissionsResponse>>
  AsyncTestIamPermissions(
      grpc::ClientContext* context,
      google::iam::v1::TestIamPermissionsRequest const& request,
      grpc::CompletionQueue* cq) override;

  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::longrunning::Operation>>
  AsyncGetOperation(grpc::ClientContext* context,
                    google::longrunning::GetOperationRequest const& request,
                    grpc::CompletionQueue* cq) override;

 private:
  google::cloud::BackgroundThreadsFactory BackgroundThreadsFactory() override {
    return child_->BackgroundThreadsFactory();
  }

  std::shared_ptr<google::cloud::bigtable::InstanceAdminClient> child_;
  google::cloud::TracingOptions tracing_options_;
};

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_LOGGING_INSTANCE_ADMIN_CLIENT_H
