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
#include "google/cloud/bigtable/internal/async_retry_multi_page.h"
#include "google/cloud/bigtable/internal/async_retry_op.h"
#include "google/cloud/bigtable/internal/async_retry_unary_rpc_and_poll.h"
#include "google/cloud/bigtable/internal/unary_client_utils.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/internal/retry_policy.h"
#include "google/cloud/internal/throw_delegate.h"
#include <google/longrunning/operations.grpc.pb.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/text_format.h>
#include <type_traits>
#include <unordered_set>
// TODO(#5929) - remove after deprecation is completed
#include "google/cloud/internal/disable_deprecation_warnings.inc"

namespace btadmin = ::google::bigtable::admin::v2;

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
static_assert(std::is_copy_assignable<bigtable::InstanceAdmin>::value,
              "bigtable::InstanceAdmin must be CopyAssignable");

using ClientUtils = bigtable::internal::UnaryClientUtils<InstanceAdminClient>;
using google::cloud::internal::Idempotency;

StatusOr<InstanceList> InstanceAdmin::ListInstances() {
  grpc::Status status;
  InstanceList result;
  // Copy the policies in effect for the operation.
  auto rpc_policy = clone_rpc_retry_policy();
  auto backoff_policy = clone_rpc_backoff_policy();

  // Build the RPC request, try to minimize copying.
  std::unordered_set<std::string> unique_failed_locations;
  std::string page_token;
  do {
    btadmin::ListInstancesRequest request;
    request.set_page_token(std::move(page_token));
    request.set_parent(project_name());

    auto response = ClientUtils::MakeCall(
        *(client_), *rpc_policy, *backoff_policy,
        MetadataUpdatePolicy(project_name(), MetadataParamTypes::PARENT),
        &InstanceAdminClient::ListInstances, request,
        "InstanceAdmin::ListInstances", status, Idempotency::kIdempotent);
    if (!status.ok()) {
      break;
    }

    auto& instances = *response.mutable_instances();
    std::move(instances.begin(), instances.end(),
              std::back_inserter(result.instances));
    auto& failed_locations = *response.mutable_failed_locations();
    std::move(
        failed_locations.begin(), failed_locations.end(),
        std::inserter(unique_failed_locations, unique_failed_locations.end()));

    page_token = std::move(*response.mutable_next_page_token());
  } while (!page_token.empty());

  if (!status.ok()) {
    return MakeStatusFromRpcError(status);
  }

  std::move(unique_failed_locations.begin(), unique_failed_locations.end(),
            std::back_inserter(result.failed_locations));
  return result;
}

future<StatusOr<btadmin::Instance>> InstanceAdmin::CreateInstance(
    InstanceConfig instance_config) {
  auto cq = background_threads_->cq();
  return AsyncCreateInstanceImpl(cq, std::move(instance_config));
}

future<StatusOr<google::bigtable::admin::v2::Instance>>
InstanceAdmin::AsyncCreateInstanceImpl(
    CompletionQueue& cq, bigtable::InstanceConfig instance_config) {
  auto request = std::move(instance_config).as_proto();
  request.set_parent(project_name());
  for (auto& kv : *request.mutable_clusters()) {
    kv.second.set_location(project_name() + "/locations/" +
                           kv.second.location());
  }
  std::shared_ptr<InstanceAdminClient> client(client_);
  return internal::AsyncStartPollAfterRetryUnaryRpc<
      google::bigtable::admin::v2::Instance>(
      __func__, clone_polling_policy(), clone_rpc_retry_policy(),
      clone_rpc_backoff_policy(),
      internal::ConstantIdempotencyPolicy(Idempotency::kNonIdempotent),
      MetadataUpdatePolicy(project_name(), MetadataParamTypes::PARENT), client,
      [client](
          grpc::ClientContext* context,
          google::bigtable::admin::v2::CreateInstanceRequest const& request,
          grpc::CompletionQueue* cq) {
        return client->AsyncCreateInstance(context, request, cq);
      },
      std::move(request), cq);
}

future<StatusOr<btadmin::Cluster>> InstanceAdmin::CreateCluster(
    ClusterConfig cluster_config, std::string const& instance_id,
    std::string const& cluster_id) {
  auto cq = background_threads_->cq();
  return AsyncCreateClusterImpl(cq, std::move(cluster_config), instance_id,
                                cluster_id);
}

future<StatusOr<google::bigtable::admin::v2::Cluster>>
InstanceAdmin::AsyncCreateClusterImpl(CompletionQueue& cq,
                                      ClusterConfig cluster_config,
                                      std::string const& instance_id,
                                      std::string const& cluster_id) {
  auto cluster = std::move(cluster_config).as_proto();
  cluster.set_location(project_name() + "/locations/" + cluster.location());
  btadmin::CreateClusterRequest request;
  request.mutable_cluster()->Swap(&cluster);
  auto parent = InstanceName(instance_id);
  request.set_parent(parent);
  request.set_cluster_id(cluster_id);

  std::shared_ptr<InstanceAdminClient> client(client_);
  return internal::AsyncStartPollAfterRetryUnaryRpc<
      google::bigtable::admin::v2::Cluster>(
      __func__, clone_polling_policy(), clone_rpc_retry_policy(),
      clone_rpc_backoff_policy(),
      internal::ConstantIdempotencyPolicy(Idempotency::kNonIdempotent),
      MetadataUpdatePolicy(parent, MetadataParamTypes::PARENT), client,
      [client](grpc::ClientContext* context,
               google::bigtable::admin::v2::CreateClusterRequest const& request,
               grpc::CompletionQueue* cq) {
        return client->AsyncCreateCluster(context, request, cq);
      },
      std::move(request), cq);
}

future<StatusOr<google::bigtable::admin::v2::Instance>>
InstanceAdmin::UpdateInstance(InstanceUpdateConfig instance_update_config) {
  auto cq = background_threads_->cq();
  return AsyncUpdateInstanceImpl(cq, std::move(instance_update_config));
}

future<StatusOr<google::bigtable::admin::v2::Instance>>
InstanceAdmin::AsyncUpdateInstanceImpl(
    CompletionQueue& cq, InstanceUpdateConfig instance_update_config) {
  auto name = instance_update_config.GetName();
  auto request = std::move(instance_update_config).as_proto();

  std::shared_ptr<InstanceAdminClient> client(client_);
  return internal::AsyncStartPollAfterRetryUnaryRpc<
      google::bigtable::admin::v2::Instance>(
      __func__, clone_polling_policy(), clone_rpc_retry_policy(),
      clone_rpc_backoff_policy(),
      internal::ConstantIdempotencyPolicy(Idempotency::kNonIdempotent),
      MetadataUpdatePolicy(name, MetadataParamTypes::INSTANCE_NAME), client,
      [client](grpc::ClientContext* context,
               google::bigtable::admin::v2::PartialUpdateInstanceRequest const&
                   request,
               grpc::CompletionQueue* cq) {
        return client->AsyncUpdateInstance(context, request, cq);
      },
      std::move(request), cq);
}

StatusOr<btadmin::Instance> InstanceAdmin::GetInstance(
    std::string const& instance_id) {
  grpc::Status status;
  // Copy the policies in effect for the operation.
  auto rpc_policy = clone_rpc_retry_policy();
  auto backoff_policy = clone_rpc_backoff_policy();

  btadmin::GetInstanceRequest request;
  // Setting instance name.
  auto name = InstanceName(instance_id);
  request.set_name(name);

  // Call RPC call to get response
  auto result = ClientUtils::MakeCall(
      *(client_), *rpc_policy, *backoff_policy,
      MetadataUpdatePolicy(name, MetadataParamTypes::NAME),
      &InstanceAdminClient::GetInstance, request, "InstanceAdmin::GetInstance",
      status, Idempotency::kIdempotent);
  if (!status.ok()) {
    return MakeStatusFromRpcError(status);
  }
  return result;
}

Status InstanceAdmin::DeleteInstance(std::string const& instance_id) {
  grpc::Status status;
  btadmin::DeleteInstanceRequest request;
  auto name = InstanceName(instance_id);
  request.set_name(name);

  // This API is not idempotent, lets call it without retry
  ClientUtils::MakeNonIdempotentCall(
      *(client_), clone_rpc_retry_policy(),
      MetadataUpdatePolicy(name, MetadataParamTypes::NAME),
      &InstanceAdminClient::DeleteInstance, request,
      "InstanceAdmin::DeleteInstance", status);
  return MakeStatusFromRpcError(status);
}

StatusOr<btadmin::Cluster> InstanceAdmin::GetCluster(
    std::string const& instance_id, std::string const& cluster_id) {
  grpc::Status status;
  auto rpc_policy = clone_rpc_retry_policy();
  auto backoff_policy = clone_rpc_backoff_policy();

  btadmin::GetClusterRequest request;
  auto name = ClusterName(instance_id, cluster_id);
  request.set_name(name);

  auto result = ClientUtils::MakeCall(
      *(client_), *rpc_policy, *backoff_policy,
      MetadataUpdatePolicy(name, MetadataParamTypes::NAME),
      &InstanceAdminClient::GetCluster, request, "InstanceAdmin::GetCluster",
      status, Idempotency::kIdempotent);
  if (!status.ok()) {
    return MakeStatusFromRpcError(status);
  }
  return result;
}

StatusOr<ClusterList> InstanceAdmin::ListClusters() {
  return ListClusters("-");
}

StatusOr<ClusterList> InstanceAdmin::ListClusters(
    std::string const& instance_id) {
  grpc::Status status;
  ClusterList result;
  std::unordered_set<std::string> unique_failed_locations;
  std::string page_token;

  // Copy the policies in effect for the operation.
  auto rpc_policy = clone_rpc_retry_policy();
  auto backoff_policy = clone_rpc_backoff_policy();

  do {
    // Build the RPC request, try to minimize copying.
    btadmin::ListClustersRequest request;
    request.set_page_token(std::move(page_token));
    auto parent = InstanceName(instance_id);
    request.set_parent(parent);

    auto response = ClientUtils::MakeCall(
        *(client_), *rpc_policy, *backoff_policy,
        MetadataUpdatePolicy(parent, MetadataParamTypes::PARENT),
        &InstanceAdminClient::ListClusters, request,
        "InstanceAdmin::ListClusters", status, Idempotency::kIdempotent);
    if (!status.ok()) {
      break;
    }

    auto& clusters = *response.mutable_clusters();
    std::move(clusters.begin(), clusters.end(),
              std::back_inserter(result.clusters));
    auto& failed_locations = *response.mutable_failed_locations();
    std::move(
        failed_locations.begin(), failed_locations.end(),
        std::inserter(unique_failed_locations, unique_failed_locations.end()));
    page_token = std::move(*response.mutable_next_page_token());
  } while (!page_token.empty());

  if (!status.ok()) {
    return MakeStatusFromRpcError(status);
  }

  std::move(unique_failed_locations.begin(), unique_failed_locations.end(),
            std::back_inserter(result.failed_locations));
  return result;
}

future<StatusOr<google::bigtable::admin::v2::Cluster>>
InstanceAdmin::UpdateCluster(ClusterConfig cluster_config) {
  auto cq = background_threads_->cq();
  return AsyncUpdateClusterImpl(cq, std::move(cluster_config));
}

future<StatusOr<google::bigtable::admin::v2::Cluster>>
InstanceAdmin::AsyncUpdateClusterImpl(CompletionQueue& cq,
                                      ClusterConfig cluster_config) {
  auto request = std::move(cluster_config).as_proto();
  auto name = request.name();

  std::shared_ptr<InstanceAdminClient> client(client_);
  return internal::AsyncStartPollAfterRetryUnaryRpc<
      google::bigtable::admin::v2::Cluster>(
      __func__, clone_polling_policy(), clone_rpc_retry_policy(),
      clone_rpc_backoff_policy(),
      internal::ConstantIdempotencyPolicy(Idempotency::kNonIdempotent),
      MetadataUpdatePolicy(name, MetadataParamTypes::NAME), client,
      [client](grpc::ClientContext* context,
               google::bigtable::admin::v2::Cluster const& request,
               grpc::CompletionQueue* cq) {
        return client->AsyncUpdateCluster(context, request, cq);
      },
      std::move(request), cq);
}

Status InstanceAdmin::DeleteCluster(std::string const& instance_id,
                                    std::string const& cluster_id) {
  grpc::Status status;
  btadmin::DeleteClusterRequest request;
  auto name = ClusterName(instance_id, cluster_id);
  request.set_name(name);

  auto metadata_update_policy =
      MetadataUpdatePolicy(name, MetadataParamTypes::NAME);

  // This API is not idempotent, lets call it without retry
  ClientUtils::MakeNonIdempotentCall(
      *(client_), clone_rpc_retry_policy(), metadata_update_policy,
      &InstanceAdminClient::DeleteCluster, request,
      "InstanceAdmin::DeleteCluster", status);
  return MakeStatusFromRpcError(status);
}

StatusOr<btadmin::AppProfile> InstanceAdmin::CreateAppProfile(
    std::string const& instance_id, AppProfileConfig config) {
  grpc::Status status;
  auto request = std::move(config).as_proto();
  auto parent = InstanceName(instance_id);
  request.set_parent(parent);

  // This is a non-idempotent API, use the correct retry loop for this type of
  // operation.
  auto result = ClientUtils::MakeNonIdempotentCall(
      *(client_), clone_rpc_retry_policy(),
      MetadataUpdatePolicy(parent, MetadataParamTypes::PARENT),
      &InstanceAdminClient::CreateAppProfile, request,
      "InstanceAdmin::CreateAppProfile", status);

  if (!status.ok()) {
    return MakeStatusFromRpcError(status);
  }
  return result;
}

StatusOr<btadmin::AppProfile> InstanceAdmin::GetAppProfile(
    std::string const& instance_id, std::string const& profile_id) {
  grpc::Status status;
  btadmin::GetAppProfileRequest request;
  auto name = AppProfileName(instance_id, profile_id);
  request.set_name(name);

  auto result = ClientUtils::MakeCall(
      *(client_), clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
      MetadataUpdatePolicy(name, MetadataParamTypes::NAME),
      &InstanceAdminClient::GetAppProfile, request,
      "InstanceAdmin::GetAppProfile", status, Idempotency::kIdempotent);

  if (!status.ok()) {
    return MakeStatusFromRpcError(status);
  }
  return result;
}

future<StatusOr<btadmin::AppProfile>> InstanceAdmin::UpdateAppProfile(
    std::string const& instance_id, std::string const& profile_id,
    AppProfileUpdateConfig config) {
  auto cq = background_threads_->cq();
  return AsyncUpdateAppProfileImpl(cq, instance_id, profile_id,
                                   std::move(config));
}

future<StatusOr<google::bigtable::admin::v2::AppProfile>>
InstanceAdmin::AsyncUpdateAppProfileImpl(CompletionQueue& cq,
                                         std::string const& instance_id,
                                         std::string const& profile_id,
                                         AppProfileUpdateConfig config) {
  auto request = std::move(config).as_proto();
  auto name = AppProfileName(instance_id, profile_id);
  request.mutable_app_profile()->set_name(name);

  std::shared_ptr<InstanceAdminClient> client(client_);
  return internal::AsyncStartPollAfterRetryUnaryRpc<
      google::bigtable::admin::v2::AppProfile>(
      __func__, clone_polling_policy(), clone_rpc_retry_policy(),
      clone_rpc_backoff_policy(),
      internal::ConstantIdempotencyPolicy(Idempotency::kNonIdempotent),
      MetadataUpdatePolicy(name, MetadataParamTypes::APP_PROFILE_NAME), client,
      [client](
          grpc::ClientContext* context,
          google::bigtable::admin::v2::UpdateAppProfileRequest const& request,
          grpc::CompletionQueue* cq) {
        return client->AsyncUpdateAppProfile(context, request, cq);
      },
      std::move(request), cq);
}

StatusOr<std::vector<btadmin::AppProfile>> InstanceAdmin::ListAppProfiles(
    std::string const& instance_id) {
  grpc::Status status;
  std::vector<btadmin::AppProfile> result;
  std::string page_token;
  // Copy the policies in effect for the operation.
  auto rpc_policy = clone_rpc_retry_policy();
  auto backoff_policy = clone_rpc_backoff_policy();
  auto parent = InstanceName(instance_id);

  do {
    // Build the RPC request, try to minimize copying.
    btadmin::ListAppProfilesRequest request;
    request.set_page_token(std::move(page_token));
    request.set_parent(parent);

    auto response = ClientUtils::MakeCall(
        *(client_), *rpc_policy, *backoff_policy,
        MetadataUpdatePolicy(parent, MetadataParamTypes::PARENT),
        &InstanceAdminClient::ListAppProfiles, request,
        "InstanceAdmin::ListAppProfiles", status, Idempotency::kIdempotent);
    if (!status.ok()) {
      break;
    }

    for (auto& x : *response.mutable_app_profiles()) {
      result.emplace_back(std::move(x));
    }
    page_token = std::move(*response.mutable_next_page_token());
  } while (!page_token.empty());

  if (!status.ok()) {
    return MakeStatusFromRpcError(status);
  }
  return result;
}

Status InstanceAdmin::DeleteAppProfile(std::string const& instance_id,
                                       std::string const& profile_id,
                                       bool ignore_warnings) {
  grpc::Status status;
  btadmin::DeleteAppProfileRequest request;
  auto name = AppProfileName(instance_id, profile_id);
  request.set_name(name);
  request.set_ignore_warnings(ignore_warnings);

  ClientUtils::MakeNonIdempotentCall(
      *(client_), clone_rpc_retry_policy(),
      MetadataUpdatePolicy(name, MetadataParamTypes::NAME),
      &InstanceAdminClient::DeleteAppProfile, request,
      "InstanceAdmin::DeleteAppProfile", status);

  return MakeStatusFromRpcError(status);
}

StatusOr<google::cloud::IamPolicy> InstanceAdmin::GetIamPolicy(
    std::string const& instance_id) {
  grpc::Status status;
  auto rpc_policy = clone_rpc_retry_policy();
  auto backoff_policy = clone_rpc_backoff_policy();

  ::google::iam::v1::GetIamPolicyRequest request;
  auto resource = InstanceName(instance_id);
  request.set_resource(resource);

  MetadataUpdatePolicy metadata_update_policy(resource,
                                              MetadataParamTypes::RESOURCE);

  auto proto = ClientUtils::MakeCall(
      *(client_), *rpc_policy, *backoff_policy, metadata_update_policy,
      &InstanceAdminClient::GetIamPolicy, request,
      "InstanceAdmin::GetIamPolicy", status, Idempotency::kIdempotent);

  if (!status.ok()) {
    return MakeStatusFromRpcError(status);
  }

  return ProtoToWrapper(std::move(proto));
}

StatusOr<google::iam::v1::Policy> InstanceAdmin::GetNativeIamPolicy(
    std::string const& instance_id) {
  grpc::Status status;
  auto rpc_policy = clone_rpc_retry_policy();
  auto backoff_policy = clone_rpc_backoff_policy();

  ::google::iam::v1::GetIamPolicyRequest request;
  auto resource = InstanceName(instance_id);
  request.set_resource(resource);

  MetadataUpdatePolicy metadata_update_policy(resource,
                                              MetadataParamTypes::RESOURCE);

  auto proto = ClientUtils::MakeCall(
      *(client_), *rpc_policy, *backoff_policy, metadata_update_policy,
      &InstanceAdminClient::GetIamPolicy, request,
      "InstanceAdmin::GetIamPolicy", status, Idempotency::kIdempotent);

  if (!status.ok()) {
    return MakeStatusFromRpcError(status);
  }

  return proto;
}

StatusOr<google::cloud::IamPolicy> InstanceAdmin::SetIamPolicy(
    std::string const& instance_id,
    google::cloud::IamBindings const& iam_bindings, std::string const& etag) {
  grpc::Status status;
  auto rpc_policy = clone_rpc_retry_policy();
  auto backoff_policy = clone_rpc_backoff_policy();

  ::google::iam::v1::Policy policy;
  policy.set_etag(etag);
  auto role_bindings = iam_bindings.bindings();
  for (auto& binding : role_bindings) {
    auto& new_binding = *policy.add_bindings();
    new_binding.set_role(binding.first);
    for (auto const& member : binding.second) {
      new_binding.add_members(member);
    }
  }

  ::google::iam::v1::SetIamPolicyRequest request;
  auto resource = InstanceName(instance_id);
  request.set_resource(resource);
  *request.mutable_policy() = std::move(policy);

  MetadataUpdatePolicy metadata_update_policy(resource,
                                              MetadataParamTypes::RESOURCE);

  auto proto = ClientUtils::MakeCall(
      *(client_), *rpc_policy, *backoff_policy, metadata_update_policy,
      &InstanceAdminClient::SetIamPolicy, request,
      "InstanceAdmin::SetIamPolicy", status, Idempotency::kIdempotent);

  if (!status.ok()) {
    return MakeStatusFromRpcError(status);
  }

  return ProtoToWrapper(std::move(proto));
}

StatusOr<google::iam::v1::Policy> InstanceAdmin::SetIamPolicy(
    std::string const& instance_id, google::iam::v1::Policy const& iam_policy) {
  grpc::Status status;
  auto rpc_policy = clone_rpc_retry_policy();
  auto backoff_policy = clone_rpc_backoff_policy();

  ::google::iam::v1::SetIamPolicyRequest request;
  auto resource = InstanceName(instance_id);
  request.set_resource(resource);
  *request.mutable_policy() = iam_policy;

  MetadataUpdatePolicy metadata_update_policy(resource,
                                              MetadataParamTypes::RESOURCE);

  auto proto = ClientUtils::MakeCall(
      *(client_), *rpc_policy, *backoff_policy, metadata_update_policy,
      &InstanceAdminClient::SetIamPolicy, request,
      "InstanceAdmin::SetIamPolicy", status, Idempotency::kIdempotent);

  if (!status.ok()) {
    return MakeStatusFromRpcError(status);
  }

  return proto;
}

StatusOr<std::vector<std::string>> InstanceAdmin::TestIamPermissions(
    std::string const& instance_id,
    std::vector<std::string> const& permissions) {
  grpc::Status status;
  ::google::iam::v1::TestIamPermissionsRequest request;
  auto resource = InstanceName(instance_id);
  request.set_resource(resource);

  // Copy the policies in effect for the operation.
  auto rpc_policy = clone_rpc_retry_policy();
  auto backoff_policy = clone_rpc_backoff_policy();

  for (auto const& permission : permissions) {
    request.add_permissions(permission);
  }

  MetadataUpdatePolicy metadata_update_policy(resource,
                                              MetadataParamTypes::RESOURCE);

  auto response = ClientUtils::MakeCall(
      *(client_), *rpc_policy, *backoff_policy, metadata_update_policy,
      &InstanceAdminClient::TestIamPermissions, request,
      "InstanceAdmin::TestIamPermissions", status, Idempotency::kIdempotent);

  std::vector<std::string> resource_permissions;

  for (auto& permission : *response.mutable_permissions()) {
    resource_permissions.push_back(permission);
  }

  if (!status.ok()) {
    return MakeStatusFromRpcError(status);
  }

  return resource_permissions;
}

StatusOr<google::cloud::IamPolicy> InstanceAdmin::ProtoToWrapper(
    google::iam::v1::Policy proto) {
  google::cloud::IamPolicy result;
  result.version = proto.version();
  result.etag = std::move(*proto.mutable_etag());
  for (auto& binding : *proto.mutable_bindings()) {
    std::vector<google::protobuf::FieldDescriptor const*> field_descs;
    // On newer versions of Protobuf (circa 3.9.1) `GetReflection()` changed
    // from a virtual member function to a static member function. clang-tidy
    // warns (and we turn all warnings to errors) about using the static
    // version via the object, as we do below. Disable the warning because that
    // seems like the only portable solution.
    // NOLINTNEXTLINE(readability-static-accessed-through-instance)
    binding.GetReflection()->ListFields(binding, &field_descs);
    for (auto const* field_desc : field_descs) {
      if (field_desc->name() != "members" && field_desc->name() != "role") {
        std::stringstream os;
        os << "IamBinding field \"" << field_desc->name()
           << "\" is unknown to Bigtable C++ client. Please use "
              "GetNativeIamPolicy() and its"
              "SetIamPolicy() overload.";
        return Status(StatusCode::kUnimplemented, os.str());
      }
    }
    for (auto& member : *binding.mutable_members()) {
      result.bindings.AddMember(binding.role(), std::move(member));
    }
  }
  return result;
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
