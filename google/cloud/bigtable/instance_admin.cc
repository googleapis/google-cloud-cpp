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

#include "google/cloud/bigtable/instance_admin.h"
#include "google/cloud/bigtable/internal/async_future_from_callback.h"
#include "google/cloud/bigtable/internal/grpc_error_delegate.h"
#include "google/cloud/bigtable/internal/poll_longrunning_operation.h"
#include "google/cloud/bigtable/internal/unary_client_utils.h"
#include "google/cloud/internal/throw_delegate.h"
#include <google/longrunning/operations.grpc.pb.h>
#include <google/protobuf/text_format.h>
#include <type_traits>

namespace btadmin = ::google::bigtable::admin::v2;

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
static_assert(std::is_copy_assignable<bigtable::InstanceAdmin>::value,
              "bigtable::InstanceAdmin must be CopyAssignable");

StatusOr<InstanceList> InstanceAdmin::ListInstances() {
  grpc::Status status;
  auto result = impl_.ListInstances(status);
  if (!status.ok()) {
    return internal::MakeStatusFromRpcError(status);
  }
  return result;
}

future<InstanceList> InstanceAdmin::AsyncListInstances(CompletionQueue& cq) {
  promise<InstanceList> instance_list_promise;
  future<InstanceList> result = instance_list_promise.get_future();

  impl_.AsyncListInstances(
      cq, internal::MakeAsyncFutureFromCallback(
              std::move(instance_list_promise), "AsyncListInstances"));
  return result;
}

std::future<StatusOr<google::bigtable::admin::v2::Instance>>
InstanceAdmin::CreateInstance(InstanceConfig instance_config) {
  return std::async(std::launch::async, &InstanceAdmin::CreateInstanceImpl,
                    this, std::move(instance_config));
}

std::future<StatusOr<google::bigtable::admin::v2::Cluster>>
InstanceAdmin::CreateCluster(ClusterConfig cluster_config,
                             bigtable::InstanceId const& instance_id,
                             bigtable::ClusterId const& cluster_id) {
  return std::async(std::launch::async, &InstanceAdmin::CreateClusterImpl, this,
                    std::move(cluster_config), instance_id, cluster_id);
}

StatusOr<google::bigtable::admin::v2::Instance>
InstanceAdmin::CreateInstanceImpl(InstanceConfig instance_config) {
  // Copy the policies in effect for the operation.
  auto rpc_policy = impl_.rpc_retry_policy_->clone();
  auto backoff_policy = impl_.rpc_backoff_policy_->clone();

  // Build the RPC request, try to minimize copying.
  auto request = std::move(instance_config).as_proto();
  request.set_parent(project_name());
  for (auto& kv : *request.mutable_clusters()) {
    kv.second.set_location(project_name() + "/locations/" +
                           kv.second.location());
  }

  using ClientUtils =
      bigtable::internal::noex::UnaryClientUtils<InstanceAdminClient>;

  grpc::Status status;
  auto operation = ClientUtils::MakeCall(
      *impl_.client_, *rpc_policy, *backoff_policy,
      impl_.metadata_update_policy_, &InstanceAdminClient::CreateInstance,
      request, "InstanceAdmin::CreateInstance", status, false);
  if (!status.ok()) {
    return bigtable::internal::MakeStatusFromRpcError(status);
  }

  auto result = internal::PollLongRunningOperation<btadmin::Instance,
                                                   InstanceAdminClient>(
      impl_.client_, impl_.polling_policy_->clone(),
      impl_.metadata_update_policy_, operation, "InstanceAdmin::CreateInstance",
      status);
  if (!status.ok()) {
    return bigtable::internal::MakeStatusFromRpcError(status);
  }
  return result;
}

std::future<StatusOr<google::bigtable::admin::v2::Instance>>
InstanceAdmin::UpdateInstance(InstanceUpdateConfig instance_update_config) {
  return std::async(std::launch::async, &InstanceAdmin::UpdateInstanceImpl,
                    this, std::move(instance_update_config));
}

StatusOr<google::bigtable::admin::v2::Instance>
InstanceAdmin::UpdateInstanceImpl(InstanceUpdateConfig instance_update_config) {
  // Copy the policies in effect for the operation.
  auto rpc_policy = impl_.rpc_retry_policy_->clone();
  auto backoff_policy = impl_.rpc_backoff_policy_->clone();

  MetadataUpdatePolicy metadata_update_policy(instance_update_config.GetName(),
                                              MetadataParamTypes::NAME);

  auto request = std::move(instance_update_config).as_proto();

  using ClientUtils =
      bigtable::internal::noex::UnaryClientUtils<InstanceAdminClient>;

  grpc::Status status;
  auto operation = ClientUtils::MakeCall(
      *impl_.client_, *rpc_policy, *backoff_policy,
      impl_.metadata_update_policy_, &InstanceAdminClient::UpdateInstance,
      request, "InstanceAdmin::UpdateInstance", status, false);
  if (!status.ok()) {
    return bigtable::internal::MakeStatusFromRpcError(status);
  }

  auto result = internal::PollLongRunningOperation<btadmin::Instance,
                                                   InstanceAdminClient>(
      impl_.client_, impl_.polling_policy_->clone(),
      impl_.metadata_update_policy_, operation, "InstanceAdmin::UpdateInstance",
      status);
  if (!status.ok()) {
    return bigtable::internal::MakeStatusFromRpcError(status);
  }
  return result;
}

StatusOr<btadmin::Instance> InstanceAdmin::GetInstance(
    std::string const& instance_id) {
  grpc::Status status;
  auto result = impl_.GetInstance(instance_id, status);
  if (!status.ok()) {
    return internal::MakeStatusFromRpcError(status);
  }
  return result;
}

future<btadmin::Instance> InstanceAdmin::AsyncGetInstance(
    CompletionQueue& cq, std::string const& instance_id) {
  promise<btadmin::Instance> p;
  auto result = p.get_future();

  impl_.AsyncGetInstance(
      cq,
      internal::MakeAsyncFutureFromCallback(std::move(p), "AsyncGetInstance"),
      instance_id);

  return result;
}

Status InstanceAdmin::DeleteInstance(std::string const& instance_id) {
  grpc::Status status;
  impl_.DeleteInstance(instance_id, status);
  return internal::MakeStatusFromRpcError(status);
}

StatusOr<btadmin::Cluster> InstanceAdmin::GetCluster(
    bigtable::InstanceId const& instance_id,
    bigtable::ClusterId const& cluster_id) {
  grpc::Status status;
  auto result = impl_.GetCluster(instance_id, cluster_id, status);
  if (!status.ok()) {
    return internal::MakeStatusFromRpcError(status);
  }
  return result;
}

future<btadmin::Cluster> InstanceAdmin::AsyncGetCluster(
    CompletionQueue& cq, bigtable::InstanceId const& instance_id,
    bigtable::ClusterId const& cluster_id) {
  promise<btadmin::Cluster> p;
  auto result = p.get_future();

  impl_.AsyncGetCluster(
      cq,
      internal::MakeAsyncFutureFromCallback(std::move(p), "AsyncGetCluster"),
      instance_id, cluster_id);

  return result;
}

StatusOr<ClusterList> InstanceAdmin::ListClusters() {
  return ListClusters("-");
}

StatusOr<ClusterList> InstanceAdmin::ListClusters(
    std::string const& instance_id) {
  grpc::Status status;
  auto result = impl_.ListClusters(instance_id, status);
  if (!status.ok()) {
    return internal::MakeStatusFromRpcError(status);
  }
  return result;
}

future<ClusterList> InstanceAdmin::AsyncListClusters(CompletionQueue& cq) {
  return AsyncListClusters(cq, "-");
}

future<ClusterList> InstanceAdmin::AsyncListClusters(
    CompletionQueue& cq, std::string const& instance_id) {
  promise<ClusterList> p;
  auto result = p.get_future();

  impl_.AsyncListClusters(
      cq,
      internal::MakeAsyncFutureFromCallback(std::move(p), "AsyncListClusters"),
      instance_id);
  return result;
}

std::future<StatusOr<google::bigtable::admin::v2::Cluster>>
InstanceAdmin::UpdateCluster(ClusterConfig cluster_config) {
  return std::async(std::launch::async, &InstanceAdmin::UpdateClusterImpl, this,
                    std::move(cluster_config));
}

StatusOr<google::bigtable::admin::v2::Cluster> InstanceAdmin::UpdateClusterImpl(
    ClusterConfig cluster_config) {
  // Copy the policies in effect for the operation.
  auto rpc_policy = impl_.rpc_retry_policy_->clone();
  auto backoff_policy = impl_.rpc_backoff_policy_->clone();

  MetadataUpdatePolicy metadata_update_policy(cluster_config.GetName(),
                                              MetadataParamTypes::NAME);

  auto request = std::move(cluster_config).as_proto();

  using ClientUtils =
      bigtable::internal::noex::UnaryClientUtils<InstanceAdminClient>;

  grpc::Status status;
  auto operation = ClientUtils::MakeCall(
      *impl_.client_, *rpc_policy, *backoff_policy,
      impl_.metadata_update_policy_, &InstanceAdminClient::UpdateCluster,
      request, "InstanceAdmin::UpdateCluster", status, false);
  if (!status.ok()) {
    return bigtable::internal::MakeStatusFromRpcError(status);
  }

  auto result =
      internal::PollLongRunningOperation<btadmin::Cluster, InstanceAdminClient>(
          impl_.client_, impl_.polling_policy_->clone(),
          impl_.metadata_update_policy_, operation,
          "InstanceAdmin::UpdateCluster", status);
  if (!status.ok()) {
    return bigtable::internal::MakeStatusFromRpcError(status);
  }
  return result;
}
Status InstanceAdmin::DeleteCluster(bigtable::InstanceId const& instance_id,
                                    bigtable::ClusterId const& cluster_id) {
  grpc::Status status;
  impl_.DeleteCluster(instance_id, cluster_id, status);
  return internal::MakeStatusFromRpcError(status);
}

StatusOr<btadmin::AppProfile> InstanceAdmin::CreateAppProfile(
    bigtable::InstanceId const& instance_id, AppProfileConfig config) {
  grpc::Status status;
  auto result = impl_.CreateAppProfile(instance_id, std::move(config), status);
  if (!status.ok()) {
    return internal::MakeStatusFromRpcError(status);
  }
  return result;
}

StatusOr<btadmin::AppProfile> InstanceAdmin::GetAppProfile(
    bigtable::InstanceId const& instance_id,
    bigtable::AppProfileId const& profile_id) {
  grpc::Status status;
  auto result = impl_.GetAppProfile(instance_id, profile_id, status);
  if (!status.ok()) {
    return internal::MakeStatusFromRpcError(status);
  }
  return result;
}

std::future<StatusOr<btadmin::AppProfile>> InstanceAdmin::UpdateAppProfile(
    bigtable::InstanceId instance_id, bigtable::AppProfileId profile_id,
    AppProfileUpdateConfig config) {
  return std::async(std::launch::async, &InstanceAdmin::UpdateAppProfileImpl,
                    this, std::move(instance_id), std::move(profile_id),
                    std::move(config));
}

StatusOr<std::vector<btadmin::AppProfile>> InstanceAdmin::ListAppProfiles(
    std::string const& instance_id) {
  grpc::Status status;
  auto result = impl_.ListAppProfiles(instance_id, status);
  if (!status.ok()) {
    return internal::MakeStatusFromRpcError(status);
  }
  return result;
}

Status InstanceAdmin::DeleteAppProfile(bigtable::InstanceId const& instance_id,
                                       bigtable::AppProfileId const& profile_id,
                                       bool ignore_warnings) {
  grpc::Status status;
  impl_.DeleteAppProfile(instance_id, profile_id, ignore_warnings, status);
  return internal::MakeStatusFromRpcError(status);
}

StatusOr<google::bigtable::admin::v2::Cluster> InstanceAdmin::CreateClusterImpl(
    ClusterConfig const& cluster_config,
    bigtable::InstanceId const& instance_id,
    bigtable::ClusterId const& cluster_id) {
  // Copy the policies in effect for the operation.
  auto rpc_policy = impl_.rpc_retry_policy_->clone();
  auto backoff_policy = impl_.rpc_backoff_policy_->clone();

  // Build the RPC request, try to minimize copying.
  auto cluster = cluster_config.as_proto();
  cluster.set_location(project_name() + "/locations/" + cluster.location());

  btadmin::CreateClusterRequest request;
  request.mutable_cluster()->Swap(&cluster);
  request.set_parent(project_name() + "/instances/" + instance_id.get());
  request.set_cluster_id(cluster_id.get());

  using ClientUtils =
      bigtable::internal::noex::UnaryClientUtils<InstanceAdminClient>;

  grpc::Status status;
  auto operation = ClientUtils::MakeCall(
      *impl_.client_, *rpc_policy, *backoff_policy,
      impl_.metadata_update_policy_, &InstanceAdminClient::CreateCluster,
      request, "InstanceAdmin::CreateCluster", status, false);
  if (!status.ok()) {
    return bigtable::internal::MakeStatusFromRpcError(status);
  }

  auto result =
      internal::PollLongRunningOperation<btadmin::Cluster, InstanceAdminClient>(
          impl_.client_, impl_.polling_policy_->clone(),
          impl_.metadata_update_policy_, operation,
          "InstanceAdmin::CreateCluster", status);
  if (!status.ok()) {
    return bigtable::internal::MakeStatusFromRpcError(status);
  }
  return result;
}

StatusOr<btadmin::AppProfile> InstanceAdmin::UpdateAppProfileImpl(
    bigtable::InstanceId instance_id, bigtable::AppProfileId profile_id,
    AppProfileUpdateConfig config) {
  grpc::Status status;
  auto operation = impl_.UpdateAppProfile(
      std::move(instance_id), std::move(profile_id), std::move(config), status);
  if (!status.ok()) {
    return bigtable::internal::MakeStatusFromRpcError(status);
  }

  auto result = internal::PollLongRunningOperation<btadmin::AppProfile,
                                                   InstanceAdminClient>(
      impl_.client_, impl_.polling_policy_->clone(),
      impl_.metadata_update_policy_, operation,
      "InstanceAdmin::UpdateAppProfileImpl", status);
  if (!status.ok()) {
    return bigtable::internal::MakeStatusFromRpcError(status);
  }
  return result;
}

StatusOr<google::cloud::IamPolicy> InstanceAdmin::GetIamPolicy(
    std::string const& instance_id) {
  grpc::Status status;
  auto result = impl_.GetIamPolicy(instance_id, status);

  if (!status.ok()) {
    return internal::MakeStatusFromRpcError(status);
  }
  return result;
}

StatusOr<google::cloud::IamPolicy> InstanceAdmin::SetIamPolicy(
    std::string const& instance_id,
    google::cloud::IamBindings const& iam_bindings, std::string const& etag) {
  grpc::Status status;
  auto result = impl_.SetIamPolicy(instance_id, iam_bindings, etag, status);

  if (!status.ok()) {
    return internal::MakeStatusFromRpcError(status);
  }
  return result;
}

StatusOr<std::vector<std::string>> InstanceAdmin::TestIamPermissions(
    std::string const& instance_id,
    std::vector<std::string> const& permissions) {
  grpc::Status status;
  auto result = impl_.TestIamPermissions(instance_id, permissions, status);

  if (!status.ok()) {
    return internal::MakeStatusFromRpcError(status);
  }

  return result;
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
