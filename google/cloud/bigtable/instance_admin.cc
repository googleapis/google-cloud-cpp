// Copyright 2018 Google Inc.
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

#include "google/cloud/bigtable/instance_admin.h"
#include <sstream>
#include <string>
#include <type_traits>

namespace btadmin = ::google::bigtable::admin::v2;

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

static_assert(std::is_copy_assignable<bigtable::InstanceAdmin>::value,
              "bigtable::InstanceAdmin must be CopyAssignable");

StatusOr<InstanceList> InstanceAdmin::ListInstances() {
  google::cloud::internal::OptionsSpan span(policies_);
  InstanceList result;

  // Build the RPC request, try to minimize copying.
  btadmin::ListInstancesRequest request;
  request.set_parent(project_name());
  auto sor = connection_->ListInstances(request);
  if (!sor) return std::move(sor).status();
  auto response = *std::move(sor);
  auto& instances = *response.mutable_instances();
  std::move(instances.begin(), instances.end(),
            std::back_inserter(result.instances));
  auto& failed_locations = *response.mutable_failed_locations();
  std::move(failed_locations.begin(), failed_locations.end(),
            std::back_inserter(result.failed_locations));
  return result;
}

future<StatusOr<btadmin::Instance>> InstanceAdmin::CreateInstance(
    InstanceConfig instance_config) {
  google::cloud::internal::OptionsSpan span(policies_);
  auto request = std::move(instance_config).as_proto();
  request.set_parent(project_name());
  for (auto& kv : *request.mutable_clusters()) {
    kv.second.set_location(project_name() + "/locations/" +
                           kv.second.location());
  }
  return connection_->CreateInstance(request);
}

future<StatusOr<btadmin::Cluster>> InstanceAdmin::CreateCluster(
    ClusterConfig cluster_config, std::string const& instance_id,
    std::string const& cluster_id) {
  google::cloud::internal::OptionsSpan span(policies_);
  auto cluster = std::move(cluster_config).as_proto();
  cluster.set_location(project_name() + "/locations/" + cluster.location());
  btadmin::CreateClusterRequest request;
  request.mutable_cluster()->Swap(&cluster);
  request.set_parent(InstanceName(instance_id));
  request.set_cluster_id(cluster_id);
  return connection_->CreateCluster(request);
}

future<StatusOr<google::bigtable::admin::v2::Instance>>
InstanceAdmin::UpdateInstance(InstanceUpdateConfig instance_update_config) {
  google::cloud::internal::OptionsSpan span(policies_);
  auto request = std::move(instance_update_config).as_proto();
  return connection_->PartialUpdateInstance(request);
}

StatusOr<btadmin::Instance> InstanceAdmin::GetInstance(
    std::string const& instance_id) {
  google::cloud::internal::OptionsSpan span(policies_);
  btadmin::GetInstanceRequest request;
  request.set_name(InstanceName(instance_id));
  return connection_->GetInstance(request);
}

Status InstanceAdmin::DeleteInstance(std::string const& instance_id) {
  google::cloud::internal::OptionsSpan span(policies_);
  btadmin::DeleteInstanceRequest request;
  request.set_name(InstanceName(instance_id));
  return connection_->DeleteInstance(request);
}

StatusOr<btadmin::Cluster> InstanceAdmin::GetCluster(
    std::string const& instance_id, std::string const& cluster_id) {
  google::cloud::internal::OptionsSpan span(policies_);
  btadmin::GetClusterRequest request;
  request.set_name(ClusterName(instance_id, cluster_id));
  return connection_->GetCluster(request);
}

StatusOr<ClusterList> InstanceAdmin::ListClusters() {
  return ListClusters("-");
}

StatusOr<ClusterList> InstanceAdmin::ListClusters(
    std::string const& instance_id) {
  google::cloud::internal::OptionsSpan span(policies_);
  ClusterList result;

  btadmin::ListClustersRequest request;
  request.set_parent(InstanceName(instance_id));
  auto sor = connection_->ListClusters(request);
  if (!sor) return std::move(sor).status();
  auto response = *std::move(sor);
  auto& clusters = *response.mutable_clusters();
  std::move(clusters.begin(), clusters.end(),
            std::back_inserter(result.clusters));
  auto& failed_locations = *response.mutable_failed_locations();
  std::move(failed_locations.begin(), failed_locations.end(),
            std::back_inserter(result.failed_locations));
  return result;
}

future<StatusOr<google::bigtable::admin::v2::Cluster>>
InstanceAdmin::UpdateCluster(ClusterConfig cluster_config) {
  google::cloud::internal::OptionsSpan span(policies_);
  auto request = std::move(cluster_config).as_proto();
  return connection_->UpdateCluster(request);
}

Status InstanceAdmin::DeleteCluster(std::string const& instance_id,
                                    std::string const& cluster_id) {
  google::cloud::internal::OptionsSpan span(policies_);
  btadmin::DeleteClusterRequest request;
  request.set_name(ClusterName(instance_id, cluster_id));
  return connection_->DeleteCluster(request);
}

StatusOr<btadmin::AppProfile> InstanceAdmin::CreateAppProfile(
    std::string const& instance_id, AppProfileConfig config) {
  google::cloud::internal::OptionsSpan span(policies_);
  auto request = std::move(config).as_proto();
  request.set_parent(InstanceName(instance_id));
  return connection_->CreateAppProfile(request);
}

StatusOr<btadmin::AppProfile> InstanceAdmin::GetAppProfile(
    std::string const& instance_id, std::string const& profile_id) {
  google::cloud::internal::OptionsSpan span(policies_);
  btadmin::GetAppProfileRequest request;
  request.set_name(AppProfileName(instance_id, profile_id));
  return connection_->GetAppProfile(request);
}

future<StatusOr<btadmin::AppProfile>> InstanceAdmin::UpdateAppProfile(
    std::string const& instance_id, std::string const& profile_id,
    AppProfileUpdateConfig config) {
  google::cloud::internal::OptionsSpan span(policies_);
  auto request = std::move(config).as_proto();
  request.mutable_app_profile()->set_name(
      AppProfileName(instance_id, profile_id));
  return connection_->UpdateAppProfile(request);
}

StatusOr<std::vector<btadmin::AppProfile>> InstanceAdmin::ListAppProfiles(
    std::string const& instance_id) {
  google::cloud::internal::OptionsSpan span(policies_);
  std::vector<btadmin::AppProfile> result;

  btadmin::ListAppProfilesRequest request;
  request.set_parent(InstanceName(instance_id));
  auto sr = connection_->ListAppProfiles(request);
  for (auto& ap : sr) {
    if (!ap) return std::move(ap).status();
    result.emplace_back(*std::move(ap));
  }
  return result;
}

Status InstanceAdmin::DeleteAppProfile(std::string const& instance_id,
                                       std::string const& profile_id,
                                       bool ignore_warnings) {
  google::cloud::internal::OptionsSpan span(policies_);
  btadmin::DeleteAppProfileRequest request;
  request.set_name(AppProfileName(instance_id, profile_id));
  request.set_ignore_warnings(ignore_warnings);
  return connection_->DeleteAppProfile(request);
}

StatusOr<google::iam::v1::Policy> InstanceAdmin::GetNativeIamPolicy(
    std::string const& instance_id) {
  google::cloud::internal::OptionsSpan span(policies_);
  google::iam::v1::GetIamPolicyRequest request;
  request.set_resource(InstanceName(instance_id));
  return connection_->GetIamPolicy(request);
}

StatusOr<google::iam::v1::Policy> InstanceAdmin::SetIamPolicy(
    std::string const& instance_id, google::iam::v1::Policy const& iam_policy) {
  google::cloud::internal::OptionsSpan span(policies_);
  google::iam::v1::SetIamPolicyRequest request;
  request.set_resource(InstanceName(instance_id));
  *request.mutable_policy() = iam_policy;
  return connection_->SetIamPolicy(request);
}

StatusOr<std::vector<std::string>> InstanceAdmin::TestIamPermissions(
    std::string const& instance_id,
    std::vector<std::string> const& permissions) {
  google::cloud::internal::OptionsSpan span(policies_);
  google::iam::v1::TestIamPermissionsRequest request;
  request.set_resource(InstanceName(instance_id));
  for (auto const& permission : permissions) {
    request.add_permissions(permission);
  }
  auto sor = connection_->TestIamPermissions(request);
  if (!sor) return std::move(sor).status();
  auto response = *std::move(sor);
  std::vector<std::string> result;
  auto& ps = *response.mutable_permissions();
  std::move(ps.begin(), ps.end(), std::back_inserter(result));
  return result;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
