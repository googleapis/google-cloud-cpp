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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INSTANCE_ADMIN_CLIENT_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INSTANCE_ADMIN_CLIENT_H_

#include "google/cloud/bigtable/client_options.h"
#include <google/bigtable/admin/v2/bigtable_instance_admin.grpc.pb.h>
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
// Forward declare some classes so we can be friends.
class InstanceAdmin;
namespace noex {
class InstanceAdmin;
}  // namespace noex

/**
 * Connects to Cloud Bigtable's instance administration APIs.
 *
 * This class is used by the Cloud Bigtable wrappers to access Cloud Bigtable.
 * Multiple `bigtable::InstanceAdmin` objects may share a connection via a
 * single `InstanceAdminClient` object. The `InstanceAdminClient` object is
 * configured at construction time, this configuration includes the credentials,
 * access endpoints, default timeouts, and other gRPC configuration options.
 * This is an interface class because it is also used as a dependency injection
 * point in some of the tests.
 */
class InstanceAdminClient {
 public:
  virtual ~InstanceAdminClient() = default;

  /// The project that this AdminClient works on.
  virtual std::string const& project() const = 0;

  /**
   * Return a new channel to handle admin operations.
   *
   * Intended to access rarely used services in the same endpoints as the
   * Bigtable admin interfaces, for example, the google.longrunning.Operations.
   */
  virtual std::shared_ptr<grpc::Channel> Channel() = 0;

  /**
   * Reset and create a new Stub().
   *
   * Currently this is only used in testing.  In the future, we expect this,
   * or a similar member function, will be needed to handle errors that require
   * a new connection, or an explicit refresh of the credentials.
   */
  virtual void reset() = 0;

  // The member functions of this class are not intended for general use by
  // application developers (they are simply a dependency injection point). Make
  // them protected, so the mock classes can override them, and then make the
  // classes that do use them friends.
 protected:
  friend class InstanceAdmin;
  friend class noex::InstanceAdmin;
  //@{
  /// @name The `google.bigtable.v2.InstanceAdmin` wrappers.
  virtual grpc::Status ListInstances(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::ListInstancesRequest const& request,
      google::bigtable::admin::v2::ListInstancesResponse* response) = 0;

  virtual grpc::Status CreateInstance(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::CreateInstanceRequest const& request,
      google::longrunning::Operation* response) = 0;

  virtual grpc::Status UpdateInstance(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::PartialUpdateInstanceRequest const& request,
      google::longrunning::Operation* response) = 0;

  virtual grpc::Status GetInstance(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::GetInstanceRequest const& request,
      google::bigtable::admin::v2::Instance* response) = 0;

  virtual grpc::Status DeleteInstance(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::DeleteInstanceRequest const& request,
      google::protobuf::Empty* response) = 0;

  virtual grpc::Status ListClusters(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::ListClustersRequest const& request,
      google::bigtable::admin::v2::ListClustersResponse* response) = 0;

  virtual grpc::Status DeleteCluster(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::DeleteClusterRequest const& request,
      google::protobuf::Empty* response) = 0;

  virtual grpc::Status CreateCluster(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::CreateClusterRequest const& request,
      google::longrunning::Operation* response) = 0;

  virtual grpc::Status GetCluster(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::GetClusterRequest const& request,
      google::bigtable::admin::v2::Cluster* response) = 0;

  virtual grpc::Status UpdateCluster(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::Cluster const& request,
      google::longrunning::Operation* response) = 0;

  virtual grpc::Status CreateAppProfile(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::CreateAppProfileRequest const& request,
      google::bigtable::admin::v2::AppProfile* response) = 0;

  virtual grpc::Status GetAppProfile(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::GetAppProfileRequest const& request,
      google::bigtable::admin::v2::AppProfile* response) = 0;

  virtual grpc::Status ListAppProfiles(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::ListAppProfilesRequest const& request,
      google::bigtable::admin::v2::ListAppProfilesResponse* response) = 0;

  virtual grpc::Status UpdateAppProfile(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::UpdateAppProfileRequest const& request,
      google::longrunning::Operation* response) = 0;

  virtual grpc::Status DeleteAppProfile(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::DeleteAppProfileRequest const& request,
      google::protobuf::Empty* response) = 0;

  virtual grpc::Status GetIamPolicy(
      grpc::ClientContext* context,
      google::iam::v1::GetIamPolicyRequest const& request,
      google::iam::v1::Policy* response) = 0;

  virtual grpc::Status SetIamPolicy(
      grpc::ClientContext* context,
      google::iam::v1::SetIamPolicyRequest const& request,
      google::iam::v1::Policy* response) = 0;

  virtual grpc::Status TestIamPermissions(
      grpc::ClientContext* context,
      google::iam::v1::TestIamPermissionsRequest const& request,
      google::iam::v1::TestIamPermissionsResponse* response) = 0;
  //@}

  //@{
  /// @name The `google.longrunning.Operations` wrappers.
  virtual grpc::Status GetOperation(
      grpc::ClientContext* context,
      google::longrunning::GetOperationRequest const& request,
      google::longrunning::Operation* response) = 0;
  //@}
};

/// Create a new admin client configured via @p options.
std::shared_ptr<InstanceAdminClient> CreateDefaultInstanceAdminClient(
    std::string project, ClientOptions options);

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INSTANCE_ADMIN_CLIENT_H_
