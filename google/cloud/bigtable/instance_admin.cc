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
#include "google/cloud/bigtable/internal/async_retry_unary_rpc_and_poll.h"
#include "google/cloud/bigtable/internal/unary_client_utils.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/internal/throw_delegate.h"
#include <google/longrunning/operations.grpc.pb.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/text_format.h>
#include <type_traits>
#include <unordered_set>

namespace btadmin = ::google::bigtable::admin::v2;

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
static_assert(std::is_copy_assignable<bigtable::InstanceAdmin>::value,
              "bigtable::InstanceAdmin must be CopyAssignable");

using ClientUtils = bigtable::internal::UnaryClientUtils<InstanceAdminClient>;

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
        "InstanceAdmin::ListInstances", status, true);
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

future<StatusOr<InstanceList>> InstanceAdmin::AsyncListInstances(
    CompletionQueue& cq) {
  promise<StatusOr<InstanceList>> instance_list_promise;
  future<StatusOr<InstanceList>> result = instance_list_promise.get_future();
  auto client = client_;
  btadmin::ListInstancesRequest request;
  request.set_parent(project_name());

  struct Accumulator {
    std::vector<Instance> instances;
    std::unordered_set<std::string> failed_locations;
  };

  return internal::StartAsyncRetryMultiPage(
             __func__, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
             MetadataUpdatePolicy(project_name(), MetadataParamTypes::PARENT),
             [client](grpc::ClientContext* context,
                      btadmin::ListInstancesRequest const& request,
                      grpc::CompletionQueue* cq) {
               return client->AsyncListInstances(context, request, cq);
             },
             std::move(request), Accumulator(),
             [](Accumulator acc, btadmin::ListInstancesResponse response) {
               std::move(response.failed_locations().begin(),
                         response.failed_locations().end(),
                         std::inserter(acc.failed_locations,
                                       acc.failed_locations.end()));
               std::move(response.instances().begin(),
                         response.instances().end(),
                         std::back_inserter(acc.instances));
               return acc;
             },
             cq)
      .then([](future<StatusOr<Accumulator>> acc_future)
                -> StatusOr<InstanceList> {
        auto acc = acc_future.get();
        if (!acc) {
          return acc.status();
        }
        InstanceList res;
        res.instances = std::move(acc->instances);
        std::move(acc->failed_locations.begin(), acc->failed_locations.end(),
                  std::back_inserter(res.failed_locations));
        return res;
      });
}

future<StatusOr<btadmin::Instance>> InstanceAdmin::CreateInstance(
    InstanceConfig instance_config) {
  CompletionQueue cq;
  std::thread([](CompletionQueue cq) { cq.Run(); }, cq).detach();

  return AsyncCreateInstance(cq, std::move(instance_config))
      .then([cq](future<StatusOr<btadmin::Instance>> f) mutable {
        cq.Shutdown();
        return f.get();
      });
}

future<StatusOr<google::bigtable::admin::v2::Instance>>
InstanceAdmin::AsyncCreateInstance(CompletionQueue& cq,
                                   bigtable::InstanceConfig instance_config) {
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
      clone_rpc_backoff_policy(), internal::ConstantIdempotencyPolicy(false),
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
  CompletionQueue cq;
  std::thread([](CompletionQueue cq) { cq.Run(); }, cq).detach();

  return AsyncCreateCluster(cq, std::move(cluster_config), instance_id,
                            cluster_id)
      .then([cq](future<StatusOr<btadmin::Cluster>> f) mutable {
        cq.Shutdown();
        return f.get();
      });
}

future<StatusOr<google::bigtable::admin::v2::Cluster>>
InstanceAdmin::AsyncCreateCluster(CompletionQueue& cq,
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
      clone_rpc_backoff_policy(), internal::ConstantIdempotencyPolicy(false),
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
  CompletionQueue cq;
  std::thread([](CompletionQueue cq) { cq.Run(); }, cq).detach();

  return AsyncUpdateInstance(cq, std::move(instance_update_config))
      .then([cq](future<StatusOr<btadmin::Instance>> f) mutable {
        cq.Shutdown();
        return f.get();
      });
}

future<StatusOr<google::bigtable::admin::v2::Instance>>
InstanceAdmin::AsyncUpdateInstance(
    CompletionQueue& cq, InstanceUpdateConfig instance_update_config) {
  auto name = instance_update_config.GetName();
  auto request = std::move(instance_update_config).as_proto();

  std::shared_ptr<InstanceAdminClient> client(client_);
  return internal::AsyncStartPollAfterRetryUnaryRpc<
      google::bigtable::admin::v2::Instance>(
      __func__, clone_polling_policy(), clone_rpc_retry_policy(),
      clone_rpc_backoff_policy(), internal::ConstantIdempotencyPolicy(false),
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
      status, true);
  if (!status.ok()) {
    return MakeStatusFromRpcError(status);
  }
  return result;
}

future<StatusOr<btadmin::Instance>> InstanceAdmin::AsyncGetInstance(
    CompletionQueue& cq, std::string const& instance_id) {
  btadmin::GetInstanceRequest request;
  auto name = InstanceName(instance_id);
  request.set_name(name);

  auto client = client_;
  return internal::StartRetryAsyncUnaryRpc(
      __func__, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
      internal::ConstantIdempotencyPolicy(true),
      MetadataUpdatePolicy(name, MetadataParamTypes::NAME),
      [client](grpc::ClientContext* context,
               btadmin::GetInstanceRequest const& request,
               grpc::CompletionQueue* cq) {
        return client->AsyncGetInstance(context, request, cq);
      },
      std::move(request), cq);
}

Status InstanceAdmin::DeleteInstance(std::string const& instance_id) {
  grpc::Status status;
  btadmin::DeleteInstanceRequest request;
  auto name = InstanceName(instance_id);
  request.set_name(name);

  // This API is not idempotent, lets call it without retry
  ClientUtils::MakeNonIdemponentCall(
      *(client_), clone_rpc_retry_policy(),
      MetadataUpdatePolicy(name, MetadataParamTypes::NAME),
      &InstanceAdminClient::DeleteInstance, request,
      "InstanceAdmin::DeleteInstance", status);
  return MakeStatusFromRpcError(status);
}

future<Status> InstanceAdmin::AsyncDeleteCluster(
    CompletionQueue& cq, std::string const& instance_id,
    std::string const& cluster_id) {
  btadmin::DeleteClusterRequest request;
  auto name = ClusterName(instance_id, cluster_id);
  request.set_name(name);

  auto client = client_;
  return internal::StartRetryAsyncUnaryRpc(
             __func__, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
             internal::ConstantIdempotencyPolicy(false),
             MetadataUpdatePolicy(name, MetadataParamTypes::NAME),
             [client](grpc::ClientContext* context,
                      btadmin::DeleteClusterRequest const& request,
                      grpc::CompletionQueue* cq) {
               return client->AsyncDeleteCluster(context, request, cq);
             },
             std::move(request), cq)
      .then([](future<StatusOr<google::protobuf::Empty>> fut) {
        auto res = fut.get();
        if (res) {
          return google::cloud::Status();
        }
        return res.status();
      });
}

future<Status> InstanceAdmin::AsyncDeleteInstance(
    std::string const& instance_id, CompletionQueue& cq) {
  google::bigtable::admin::v2::DeleteInstanceRequest request;
  auto name = InstanceName(instance_id);
  request.set_name(name);

  auto client = client_;
  return internal::StartRetryAsyncUnaryRpc(
             __func__, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
             internal::ConstantIdempotencyPolicy(true),
             MetadataUpdatePolicy(name, MetadataParamTypes::NAME),
             [client](grpc::ClientContext* context,
                      google::bigtable::admin::v2::DeleteInstanceRequest const&
                          request,
                      grpc::CompletionQueue* cq) {
               return client->AsyncDeleteInstance(context, request, cq);
             },
             std::move(request), cq)
      .then([](future<StatusOr<google::protobuf::Empty>> r) {
        return r.get().status();
      });
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
      status, true);
  if (!status.ok()) {
    return MakeStatusFromRpcError(status);
  }
  return result;
}

future<StatusOr<btadmin::Cluster>> InstanceAdmin::AsyncGetCluster(
    CompletionQueue& cq, std::string const& instance_id,
    std::string const& cluster_id) {
  promise<StatusOr<btadmin::Cluster>> p;
  auto result = p.get_future();
  btadmin::GetClusterRequest request;
  auto name = ClusterName(instance_id, cluster_id);
  request.set_name(name);
  auto client = client_;
  return internal::StartRetryAsyncUnaryRpc(
      __func__, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
      internal::ConstantIdempotencyPolicy(true),
      MetadataUpdatePolicy(name, MetadataParamTypes::NAME),
      [client](grpc::ClientContext* context,
               btadmin::GetClusterRequest const& request,
               grpc::CompletionQueue* cq) {
        return client->AsyncGetCluster(context, request, cq);
      },
      std::move(request), cq);
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
        "InstanceAdmin::ListClusters", status, true);
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

future<StatusOr<ClusterList>> InstanceAdmin::AsyncListClusters(
    CompletionQueue& cq) {
  return AsyncListClusters(cq, "-");
}

future<StatusOr<ClusterList>> InstanceAdmin::AsyncListClusters(
    CompletionQueue& cq, std::string const& instance_id) {
  auto client = client_;
  btadmin::ListClustersRequest request;
  auto parent = InstanceName(instance_id);
  request.set_parent(parent);

  struct Accumulator {
    std::vector<btadmin::Cluster> clusters;
    std::unordered_set<std::string> failed_locations;
  };

  return internal::StartAsyncRetryMultiPage(
             __func__, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
             MetadataUpdatePolicy(parent, MetadataParamTypes::PARENT),
             [client](grpc::ClientContext* context,
                      btadmin::ListClustersRequest const& request,
                      grpc::CompletionQueue* cq) {
               return client->AsyncListClusters(context, request, cq);
             },
             std::move(request), Accumulator(),
             [](Accumulator acc, btadmin::ListClustersResponse response) {
               std::move(response.failed_locations().begin(),
                         response.failed_locations().end(),
                         std::inserter(acc.failed_locations,
                                       acc.failed_locations.end()));
               std::move(response.clusters().begin(), response.clusters().end(),
                         std::back_inserter(acc.clusters));
               return acc;
             },
             cq)
      .then([](future<StatusOr<Accumulator>> acc_future)
                -> StatusOr<ClusterList> {
        auto acc = acc_future.get();
        if (!acc) {
          return acc.status();
        }
        ClusterList res;
        res.clusters = std::move(acc->clusters);
        std::move(acc->failed_locations.begin(), acc->failed_locations.end(),
                  std::back_inserter(res.failed_locations));
        return res;
      });
}

future<StatusOr<google::bigtable::admin::v2::Cluster>>
InstanceAdmin::UpdateCluster(ClusterConfig cluster_config) {
  CompletionQueue cq;
  std::thread([](CompletionQueue cq) { cq.Run(); }, cq).detach();

  return AsyncUpdateCluster(cq, std::move(cluster_config))
      .then([cq](future<StatusOr<btadmin::Cluster>> f) mutable {
        cq.Shutdown();
        return f.get();
      });
}

future<StatusOr<google::bigtable::admin::v2::Cluster>>
InstanceAdmin::AsyncUpdateCluster(CompletionQueue& cq,
                                  ClusterConfig cluster_config) {
  auto request = std::move(cluster_config).as_proto();
  auto name = request.name();

  std::shared_ptr<InstanceAdminClient> client(client_);
  return internal::AsyncStartPollAfterRetryUnaryRpc<
      google::bigtable::admin::v2::Cluster>(
      __func__, clone_polling_policy(), clone_rpc_retry_policy(),
      clone_rpc_backoff_policy(), internal::ConstantIdempotencyPolicy(false),
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
  ClientUtils::MakeNonIdemponentCall(
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
  auto result = ClientUtils::MakeNonIdemponentCall(
      *(client_), clone_rpc_retry_policy(),
      MetadataUpdatePolicy(parent, MetadataParamTypes::PARENT),
      &InstanceAdminClient::CreateAppProfile, request,
      "InstanceAdmin::CreateAppProfile", status);

  if (!status.ok()) {
    return MakeStatusFromRpcError(status);
  }
  return result;
}

future<StatusOr<google::bigtable::admin::v2::AppProfile>>
InstanceAdmin::AsyncCreateAppProfile(CompletionQueue& cq,
                                     std::string const& instance_id,
                                     AppProfileConfig config) {
  auto request = std::move(config).as_proto();
  auto parent = InstanceName(instance_id);
  request.set_parent(parent);

  std::shared_ptr<InstanceAdminClient> client(client_);
  return internal::StartRetryAsyncUnaryRpc(
      __func__, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
      internal::ConstantIdempotencyPolicy(false),
      MetadataUpdatePolicy(parent, MetadataParamTypes::PARENT),
      [client](grpc::ClientContext* context,
               btadmin::CreateAppProfileRequest const& request,
               grpc::CompletionQueue* cq) {
        return client->AsyncCreateAppProfile(context, request, cq);
      },
      std::move(request), cq);
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
      "InstanceAdmin::GetAppProfile", status, true);

  if (!status.ok()) {
    return MakeStatusFromRpcError(status);
  }
  return result;
}

future<StatusOr<google::bigtable::admin::v2::AppProfile>>
InstanceAdmin::AsyncGetAppProfile(CompletionQueue& cq,
                                  std::string const& instance_id,
                                  std::string const& profile_id) {
  btadmin::GetAppProfileRequest request;
  auto name = AppProfileName(instance_id, profile_id);
  request.set_name(name);

  std::shared_ptr<InstanceAdminClient> client(client_);
  return internal::StartRetryAsyncUnaryRpc(
      __func__, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
      internal::ConstantIdempotencyPolicy(true),
      MetadataUpdatePolicy(name, MetadataParamTypes::NAME),
      [client](grpc::ClientContext* context,
               btadmin::GetAppProfileRequest const& request,
               grpc::CompletionQueue* cq) {
        return client->AsyncGetAppProfile(context, request, cq);
      },
      std::move(request), cq);
}

future<StatusOr<btadmin::AppProfile>> InstanceAdmin::UpdateAppProfile(
    std::string const& instance_id, std::string const& profile_id,
    AppProfileUpdateConfig config) {
  CompletionQueue cq;
  std::thread([](CompletionQueue cq) { cq.Run(); }, cq).detach();

  return AsyncUpdateAppProfile(cq, instance_id, profile_id, std::move(config))
      .then([cq](future<StatusOr<btadmin::AppProfile>> f) mutable {
        cq.Shutdown();
        return f.get();
      });
}

future<StatusOr<google::bigtable::admin::v2::AppProfile>>
InstanceAdmin::AsyncUpdateAppProfile(CompletionQueue& cq,
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
      clone_rpc_backoff_policy(), internal::ConstantIdempotencyPolicy(false),
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
        "InstanceAdmin::ListAppProfiles", status, true);
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

future<StatusOr<std::vector<btadmin::AppProfile>>>
InstanceAdmin::AsyncListAppProfiles(CompletionQueue& cq,
                                    std::string const& instance_id) {
  auto client = client_;
  btadmin::ListAppProfilesRequest request;
  auto parent = InstanceName(instance_id);
  request.set_parent(parent);

  return internal::StartAsyncRetryMultiPage(
      __func__, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
      MetadataUpdatePolicy(parent, MetadataParamTypes::PARENT),
      [client](grpc::ClientContext* context,
               btadmin::ListAppProfilesRequest const& request,
               grpc::CompletionQueue* cq) {
        return client->AsyncListAppProfiles(context, request, cq);
      },
      std::move(request), std::vector<btadmin::AppProfile>(),
      [](std::vector<btadmin::AppProfile> acc,
         btadmin::ListAppProfilesResponse response) {
        std::move(response.app_profiles().begin(),
                  response.app_profiles().end(), std::back_inserter(acc));
        return acc;
      },
      cq);
}

Status InstanceAdmin::DeleteAppProfile(std::string const& instance_id,
                                       std::string const& profile_id,
                                       bool ignore_warnings) {
  grpc::Status status;
  btadmin::DeleteAppProfileRequest request;
  auto name = AppProfileName(instance_id, profile_id);
  request.set_name(name);
  request.set_ignore_warnings(ignore_warnings);

  ClientUtils::MakeNonIdemponentCall(
      *(client_), clone_rpc_retry_policy(),
      MetadataUpdatePolicy(name, MetadataParamTypes::NAME),
      &InstanceAdminClient::DeleteAppProfile, request,
      "InstanceAdmin::DeleteAppProfile", status);

  return MakeStatusFromRpcError(status);
}

future<Status> InstanceAdmin::AsyncDeleteAppProfile(
    CompletionQueue& cq, std::string const& instance_id,
    std::string const& profile_id, bool ignore_warnings) {
  btadmin::DeleteAppProfileRequest request;
  auto name = AppProfileName(instance_id, profile_id);
  request.set_name(name);
  request.set_ignore_warnings(ignore_warnings);

  std::shared_ptr<InstanceAdminClient> client(client_);
  return internal::StartRetryAsyncUnaryRpc(
             __func__, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
             internal::ConstantIdempotencyPolicy(false),
             MetadataUpdatePolicy(name, MetadataParamTypes::NAME),
             [client](grpc::ClientContext* context,
                      btadmin::DeleteAppProfileRequest const& request,
                      grpc::CompletionQueue* cq) {
               return client->AsyncDeleteAppProfile(context, request, cq);
             },
             std::move(request), cq)
      .then([](future<StatusOr<google::protobuf::Empty>> fut) {
        auto res = fut.get();
        if (res) {
          return google::cloud::Status();
        }
        return res.status();
      });
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
      "InstanceAdmin::GetIamPolicy", status, true);

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
      "InstanceAdmin::GetIamPolicy", status, true);

  if (!status.ok()) {
    return MakeStatusFromRpcError(status);
  }

  return proto;
}

future<StatusOr<google::cloud::IamPolicy>> InstanceAdmin::AsyncGetIamPolicy(
    CompletionQueue& cq, std::string const& instance_id) {
  ::google::iam::v1::GetIamPolicyRequest request;

  auto resource = InstanceName(instance_id);
  request.set_resource(resource);

  std::shared_ptr<InstanceAdminClient> client(client_);
  return internal::StartRetryAsyncUnaryRpc(
             __func__, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
             internal::ConstantIdempotencyPolicy(true),
             MetadataUpdatePolicy(resource, MetadataParamTypes::RESOURCE),
             [client](grpc::ClientContext* context,
                      ::google::iam::v1::GetIamPolicyRequest const& request,
                      grpc::CompletionQueue* cq) {
               return client->AsyncGetIamPolicy(context, request, cq);
             },
             std::move(request), cq)
      .then([](future<StatusOr<::google::iam::v1::Policy>> fut)
                -> StatusOr<google::cloud::IamPolicy> {
        auto res = fut.get();
        if (!res) {
          return res.status();
        }
        return ProtoToWrapper(std::move(*res));
      });
}

future<StatusOr<google::iam::v1::Policy>>
InstanceAdmin::AsyncGetNativeIamPolicy(CompletionQueue& cq,
                                       std::string const& instance_id) {
  ::google::iam::v1::GetIamPolicyRequest request;
  auto resource = InstanceName(instance_id);
  request.set_resource(resource);

  std::shared_ptr<InstanceAdminClient> client(client_);
  return internal::StartRetryAsyncUnaryRpc(
      __func__, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
      internal::ConstantIdempotencyPolicy(true),
      MetadataUpdatePolicy(resource, MetadataParamTypes::RESOURCE),
      [client](grpc::ClientContext* context,
               ::google::iam::v1::GetIamPolicyRequest const& request,
               grpc::CompletionQueue* cq) {
        return client->AsyncGetIamPolicy(context, request, cq);
      },
      std::move(request), cq);
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
    auto new_binding = policy.add_bindings();
    new_binding->set_role(binding.first);
    for (auto& member : binding.second) {
      new_binding->add_members(member);
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
      "InstanceAdmin::SetIamPolicy", status, true);

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
      "InstanceAdmin::SetIamPolicy", status, true);

  if (!status.ok()) {
    return MakeStatusFromRpcError(status);
  }

  return proto;
}

future<StatusOr<google::cloud::IamPolicy>> InstanceAdmin::AsyncSetIamPolicy(
    CompletionQueue& cq, std::string const& instance_id,
    google::cloud::IamBindings const& iam_bindings, std::string const& etag) {
  ::google::iam::v1::Policy policy;
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
  auto resource = InstanceName(instance_id);
  request.set_resource(resource);
  *request.mutable_policy() = std::move(policy);

  std::shared_ptr<InstanceAdminClient> client(client_);
  return internal::StartRetryAsyncUnaryRpc(
             __func__, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
             internal::ConstantIdempotencyPolicy(false),
             MetadataUpdatePolicy(resource, MetadataParamTypes::RESOURCE),
             [client](grpc::ClientContext* context,
                      ::google::iam::v1::SetIamPolicyRequest const& request,
                      grpc::CompletionQueue* cq) {
               return client->AsyncSetIamPolicy(context, request, cq);
             },
             std::move(request), cq)
      .then([](future<StatusOr<::google::iam::v1::Policy>> response_fut)
                -> StatusOr<google::cloud::IamPolicy> {
        auto response = response_fut.get();
        if (!response) {
          return response.status();
        }
        return ProtoToWrapper(std::move(*response));
      });
}

future<StatusOr<google::iam::v1::Policy>> InstanceAdmin::AsyncSetIamPolicy(
    CompletionQueue& cq, std::string const& instance_id,
    google::iam::v1::Policy const& iam_policy) {
  ::google::iam::v1::SetIamPolicyRequest request;
  auto resource = InstanceName(instance_id);
  request.set_resource(resource);
  *request.mutable_policy() = iam_policy;

  std::shared_ptr<InstanceAdminClient> client(client_);
  return internal::StartRetryAsyncUnaryRpc(
      __func__, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
      internal::ConstantIdempotencyPolicy(false),
      MetadataUpdatePolicy(resource, MetadataParamTypes::RESOURCE),
      [client](grpc::ClientContext* context,
               ::google::iam::v1::SetIamPolicyRequest const& request,
               grpc::CompletionQueue* cq) {
        return client->AsyncSetIamPolicy(context, request, cq);
      },
      std::move(request), cq);
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

  for (auto& permission : permissions) {
    request.add_permissions(permission);
  }

  MetadataUpdatePolicy metadata_update_policy(resource,
                                              MetadataParamTypes::RESOURCE);

  auto response = ClientUtils::MakeCall(
      *(client_), *rpc_policy, *backoff_policy, metadata_update_policy,
      &InstanceAdminClient::TestIamPermissions, request,
      "InstanceAdmin::TestIamPermissions", status, true);

  std::vector<std::string> resource_permissions;

  for (auto& permission : *response.mutable_permissions()) {
    resource_permissions.push_back(permission);
  }

  if (!status.ok()) {
    return MakeStatusFromRpcError(status);
  }

  return resource_permissions;
}

future<StatusOr<std::vector<std::string>>>
InstanceAdmin::AsyncTestIamPermissions(
    CompletionQueue& cq, std::string const& instance_id,
    std::vector<std::string> const& permissions) {
  ::google::iam::v1::TestIamPermissionsRequest request;
  auto resource = InstanceName(instance_id);
  request.set_resource(resource);
  for (auto& permission : permissions) {
    request.add_permissions(permission);
  }

  auto client = client_;
  return internal::StartRetryAsyncUnaryRpc(
             __func__, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
             internal::ConstantIdempotencyPolicy(true),
             MetadataUpdatePolicy(resource, MetadataParamTypes::RESOURCE),
             [client](
                 grpc::ClientContext* context,
                 ::google::iam::v1::TestIamPermissionsRequest const& request,
                 grpc::CompletionQueue* cq) {
               return client->AsyncTestIamPermissions(context, request, cq);
             },
             std::move(request), cq)
      .then([](future<StatusOr<::google::iam::v1::TestIamPermissionsResponse>>
                   response_fut) -> StatusOr<std::vector<std::string>> {
        auto response = response_fut.get();
        if (!response) {
          return response.status();
        }
        std::vector<std::string> res;
        res.reserve(response->permissions_size());
        std::move(response->mutable_permissions()->begin(),
                  response->mutable_permissions()->end(),
                  std::back_inserter(res));
        return res;
      });
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
    for (auto field_desc : field_descs) {
      if (field_desc->name() != "members" && field_desc->name() != "role") {
        std::stringstream os;
        os << "IamBinding field \"" << field_desc->name()
           << "\" is unknown to Bigtable C++ client. Please use "
              "[Async]GetNativeIamPolicy() and their respective "
              "[Async]SetIamPolicy() overloads.";
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
