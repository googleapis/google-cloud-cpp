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

#include "google/cloud/bigtable/internal/instance_admin.h"
#include "google/cloud/bigtable/internal/unary_client_utils.h"
#include <type_traits>

namespace btproto = ::google::bigtable::admin::v2;

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace noex {
static_assert(std::is_copy_assignable<bigtable::noex::InstanceAdmin>::value,
              "bigtable::noex::InstanceAdmin must be CopyAssignable");

using ClientUtils =
    bigtable::internal::noex::UnaryClientUtils<bigtable::InstanceAdminClient>;

std::vector<btproto::Instance> InstanceAdmin::ListInstances(
    grpc::Status& status) {
  // Copy the policies in effect for the operation.
  auto rpc_policy = rpc_retry_policy_->clone();
  auto backoff_policy = rpc_backoff_policy_->clone();

  // Build the RPC request, try to minimize copying.
  std::vector<btproto::Instance> result;
  std::string page_token;
  do {
    btproto::ListInstancesRequest request;
    request.set_page_token(std::move(page_token));
    request.set_parent(project_name_);

    auto response = ClientUtils::MakeCall(
        *client_, *rpc_policy, *backoff_policy, metadata_update_policy_,
        &InstanceAdminClient::ListInstances, request,
        "InstanceAdmin::ListInstances", status, true);
    if (not status.ok()) {
      return result;
    }

    for (auto& x : *response.mutable_instances()) {
      result.emplace_back(std::move(x));
    }
    page_token = std::move(*response.mutable_next_page_token());
  } while (not page_token.empty());
  return result;
}

btproto::Instance InstanceAdmin::GetInstance(std::string const& instance_id,
                                             grpc::Status& status) {
  // Copy the policies in effect for the operation.
  auto rpc_policy = rpc_retry_policy_->clone();
  auto backoff_policy = rpc_backoff_policy_->clone();

  btproto::GetInstanceRequest request;
  // Setting instance name.
  request.set_name(project_name_ + "/instances/" + instance_id);

  // Call RPC call to get response
  return ClientUtils::MakeCall(*client_, *rpc_policy, *backoff_policy,
                               metadata_update_policy_,
                               &InstanceAdminClient::GetInstance, request,
                               "InstanceAdmin::GetInstance", status, true);
}

void InstanceAdmin::DeleteInstance(std::string const& instance_id,
                                   grpc::Status& status) {
  btproto::DeleteInstanceRequest request;
  request.set_name(InstanceName(instance_id));

  // This API is not idempotent, lets call it without retry
  ClientUtils::MakeNonIdemponentCall(
      *client_, rpc_retry_policy_->clone(), metadata_update_policy_,
      &InstanceAdminClient::DeleteInstance, request,
      "InstanceAdmin::DeleteInstance", status);
}

btproto::Cluster InstanceAdmin::GetCluster(
    bigtable::InstanceId const& instance_id,
    bigtable::ClusterId const& cluster_id, grpc::Status& status) {
  auto rpc_policy = rpc_retry_policy_->clone();
  auto backoff_policy = rpc_backoff_policy_->clone();

  btproto::GetClusterRequest request;
  request.set_name(ClusterName(instance_id, cluster_id));

  return ClientUtils::MakeCall(*client_, *rpc_policy, *backoff_policy,
                               metadata_update_policy_,
                               &InstanceAdminClient::GetCluster, request,
                               "InstanceAdmin::GetCluster", status, true);
}

std::vector<btproto::Cluster> InstanceAdmin::ListClusters(
    std::string const& instance_id, grpc::Status& status) {
  // Copy the policies in effect for the operation.
  auto rpc_policy = rpc_retry_policy_->clone();
  auto backoff_policy = rpc_backoff_policy_->clone();

  // Build the RPC request, try to minimize copying.
  std::vector<btproto::Cluster> result;
  std::string page_token;
  do {
    btproto::ListClustersRequest request;
    request.set_page_token(std::move(page_token));
    request.set_parent(InstanceName(instance_id));

    auto response = ClientUtils::MakeCall(
        *client_, *rpc_policy, *backoff_policy, metadata_update_policy_,
        &InstanceAdminClient::ListClusters, request,
        "InstanceAdmin::ListClusters", status, true);
    if (not status.ok()) {
      return result;
    }

    for (auto& x : *response.mutable_clusters()) {
      result.emplace_back(std::move(x));
    }
    page_token = std::move(*response.mutable_next_page_token());
  } while (not page_token.empty());
  return result;
}

void InstanceAdmin::DeleteCluster(bigtable::InstanceId const& instance_id,
                                  bigtable::ClusterId const& cluster_id,
                                  grpc::Status& status) {
  btproto::DeleteClusterRequest request;
  request.set_name(ClusterName(instance_id, cluster_id));

  MetadataUpdatePolicy metadata_update_policy(
      ClusterName(instance_id, cluster_id), MetadataParamTypes::NAME);

  // This API is not idempotent, lets call it without retry
  ClientUtils::MakeNonIdemponentCall(
      *client_, rpc_retry_policy_->clone(), metadata_update_policy_,
      &InstanceAdminClient::DeleteCluster, request,
      "InstanceAdmin::DeleteCluster", status);
}

btproto::AppProfile InstanceAdmin::CreateAppProfile(
    bigtable::InstanceId const& instance_id, AppProfileConfig config,
    grpc::Status& status) {
  auto request = config.as_proto_move();
  request.set_parent(InstanceName(instance_id.get()));

  // This API is not idempotent, call it without retry.
  return ClientUtils::MakeNonIdemponentCall(
      *client_, rpc_retry_policy_->clone(), metadata_update_policy_,
      &InstanceAdminClient::CreateAppProfile, request,
      "InstanceAdmin::CreateAppProfile", status);
}

btproto::AppProfile InstanceAdmin::GetAppProfile(
    bigtable::InstanceId const& instance_id,
    bigtable::AppProfileId const& profile_id, grpc::Status& status) {
  btproto::GetAppProfileRequest request;
  request.set_name(InstanceName(instance_id.get()) + "/appProfiles/" +
                   profile_id.get());

  return ClientUtils::MakeCall(
      *client_, rpc_retry_policy_->clone(), rpc_backoff_policy_->clone(),
      metadata_update_policy_, &InstanceAdminClient::GetAppProfile, request,
      "InstanceAdmin::GetAppProfile", status, true);
}

::google::longrunning::Operation InstanceAdmin::UpdateAppProfile(
    bigtable::InstanceId instance_id, bigtable::AppProfileId profile_id,
    AppProfileUpdateConfig config, grpc::Status& status) {
  auto request = config.as_proto_move();
  request.mutable_app_profile()->set_name(
      InstanceName(instance_id.get() + "/appProfiles/" + profile_id.get()));

  // This API is not idempotent, call it without retry.
  return ClientUtils::MakeCall(
      *client_, rpc_retry_policy_->clone(), rpc_backoff_policy_->clone(),
      metadata_update_policy_, &InstanceAdminClient::UpdateAppProfile, request,
      "InstanceAdmin::UpdateAppProfile", status, true);
}

std::vector<btproto::AppProfile> InstanceAdmin::ListAppProfiles(
    std::string const& instance_id, grpc::Status& status) {
  // Copy the policies in effect for the operation.
  auto rpc_policy = rpc_retry_policy_->clone();
  auto backoff_policy = rpc_backoff_policy_->clone();

  // Build the RPC request, try to minimize copying.
  std::vector<btproto::AppProfile> result;
  std::string page_token;
  do {
    btproto::ListAppProfilesRequest request;
    request.set_page_token(std::move(page_token));
    request.set_parent(InstanceName(instance_id));

    auto response = ClientUtils::MakeCall(
        *client_, *rpc_policy, *backoff_policy, metadata_update_policy_,
        &InstanceAdminClient::ListAppProfiles, request,
        "InstanceAdmin::ListAppProfiles", status, true);
    if (not status.ok()) {
      return result;
    }

    for (auto& x : *response.mutable_app_profiles()) {
      result.emplace_back(std::move(x));
    }
    page_token = std::move(*response.mutable_next_page_token());
  } while (not page_token.empty());
  return result;
}

void InstanceAdmin::DeleteAppProfile(bigtable::InstanceId const& instance_id,
                                     bigtable::AppProfileId const& profile_id,
                                     bool ignore_warnings,
                                     grpc::Status& status) {
  btproto::DeleteAppProfileRequest request;
  request.set_name(InstanceName(instance_id.get()) + "/appProfiles/" +
                   profile_id.get());
  request.set_ignore_warnings(ignore_warnings);

  ClientUtils::MakeNonIdemponentCall(
      *client_, rpc_retry_policy_->clone(), metadata_update_policy_,
      &InstanceAdminClient::DeleteAppProfile, request,
      "InstanceAdmin::DeleteAppProfile", status);
}

::google::iam::v1::Policy InstanceAdmin::GetIamPolicy(
    std::string const& resource, grpc::Status& status) {
  auto rpc_policy = rpc_retry_policy_->clone();
  auto backoff_policy = rpc_backoff_policy_->clone();

  ::google::iam::v1::GetIamPolicyRequest request;
  request.set_resource(resource);

  MetadataUpdatePolicy metadata_update_policy(project_name(),
                                              MetadataParamTypes::RESOURCE);

  return ClientUtils::MakeCall(*client_, *rpc_policy, *backoff_policy,
                               metadata_update_policy,
                               &InstanceAdminClient::GetIamPolicy, request,
                               "InstanceAdmin::GetIamPolicy", status, true);
}

::google::iam::v1::Policy InstanceAdmin::SetIamPolicy(
    std::string const& resource, std::int32_t const& version,
    google::cloud::IamBindings const& iam_bindings, std::string const& etag,
    grpc::Status& status) {
  auto rpc_policy = rpc_retry_policy_->clone();
  auto backoff_policy = rpc_backoff_policy_->clone();

  ::google::iam::v1::Policy policy;
  policy.set_version(version);
  policy.set_etag(etag);
  auto role_bindings = iam_bindings.bindings();
  for (auto& binding : role_bindings) {
    auto new_binding = policy.add_bindings();
    new_binding->set_role(binding.first);
    for (auto& member : binding.second) {
      new_binding->add_members(member);
    }
  }

  ::google::iam::v1::SetIamPolicyRequest request;
  request.set_resource(resource);
  *request.mutable_policy() = std::move(policy);

  MetadataUpdatePolicy metadata_update_policy(project_name(),
                                              MetadataParamTypes::RESOURCE);

  return ClientUtils::MakeCall(*client_, *rpc_policy, *backoff_policy,
                               metadata_update_policy,
                               &InstanceAdminClient::SetIamPolicy, request,
                               "InstanceAdmin::SetIamPolicy", status, true);
}

std::vector<std::string> InstanceAdmin::TestIamPermissions(
    std::string const& resource, std::vector<std::string> const& permissions,
    grpc::Status& status) {
  // Copy the policies in effect for the operation.
  auto rpc_policy = rpc_retry_policy_->clone();
  auto backoff_policy = rpc_backoff_policy_->clone();

  ::google::iam::v1::TestIamPermissionsRequest request;
  request.set_resource(resource);

  for (auto& permission : permissions) {
    request.add_permissions(permission);
  }

  MetadataUpdatePolicy metadata_update_policy(project_name(),
                                              MetadataParamTypes::RESOURCE);

  auto response = ClientUtils::MakeCall(
      *client_, *rpc_policy, *backoff_policy, metadata_update_policy,
      &InstanceAdminClient::TestIamPermissions, request,
      "InstanceAdmin::TestIamPermissions", status, true);

  std::vector<std::string> resource_permissions;

  for (auto& permission : *response.mutable_permissions()) {
    resource_permissions.push_back(permission);
  }

  return resource_permissions;
}

}  // namespace noex
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
