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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_INSTANCE_ADMIN_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_INSTANCE_ADMIN_H_

#include "google/cloud/bigtable/app_profile_config.h"
#include "google/cloud/bigtable/cluster_config.h"
#include "google/cloud/bigtable/cluster_list_responses.h"
#include "google/cloud/bigtable/instance_admin_client.h"
#include "google/cloud/bigtable/instance_config.h"
#include "google/cloud/bigtable/instance_list_responses.h"
#include "google/cloud/bigtable/instance_update_config.h"
#include "google/cloud/bigtable/internal/async_list_app_profiles.h"
#include "google/cloud/bigtable/internal/async_list_clusters.h"
#include "google/cloud/bigtable/internal/async_list_instances.h"
#include "google/cloud/bigtable/internal/async_retry_unary_rpc.h"
#include "google/cloud/bigtable/internal/async_retry_unary_rpc_and_poll.h"
#include "google/cloud/bigtable/internal/grpc_error_delegate.h"
#include "google/cloud/bigtable/metadata_update_policy.h"
#include "google/cloud/bigtable/polling_policy.h"
#include "google/cloud/bigtable/rpc_backoff_policy.h"
#include "google/cloud/bigtable/rpc_retry_policy.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/iam_policy.h"
#include <google/bigtable/admin/v2/bigtable_instance_admin.grpc.pb.h>
#include <memory>
#include <sstream>
#include <thread>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
class InstanceAdmin;
namespace noex {

/**
 * Implements a minimal API to administer Cloud Bigtable instances.
 */
class InstanceAdmin {
 public:
  /**
   * @param client the interface to create grpc stubs, report errors, etc.
   */
  InstanceAdmin(std::shared_ptr<InstanceAdminClient> client)
      : client_(std::move(client)),
        project_name_("projects/" + project_id()),
        rpc_retry_policy_(
            DefaultRPCRetryPolicy(internal::kBigtableInstanceAdminLimits)),
        rpc_backoff_policy_(
            DefaultRPCBackoffPolicy(internal::kBigtableInstanceAdminLimits)),
        polling_policy_(
            DefaultPollingPolicy(internal::kBigtableInstanceAdminLimits)),
        metadata_update_policy_(project_name(), MetadataParamTypes::PARENT) {}

  /**
   * Create a new InstanceAdmin using explicit policies to handle RPC errors.
   *
   * @param client the interface to create grpc stubs, report errors, etc.
   * @param policies the set of policy overrides for this object.
   * @tparam Policies the types of the policies to override, the types must
   *     derive from one of the following types:
   *     - `RPCBackoffPolicy` how to backoff from a failed RPC. Currently only
   *       `ExponentialBackoffPolicy` is implemented. You can also create your
   *       own policies that backoff using a different algorithm.
   *     - `RPCRetryPolicy` for how long to retry failed RPCs. Use
   *       `LimitedErrorCountRetryPolicy` to limit the number of failures
   *       allowed. Use `LimitedTimeRetryPolicy` to bound the time for any
   *       request. You can also create your own policies that combine time and
   *       error counts.
   *     - `PollingPolicy` for how long will the class wait for
   *       `google.longrunning.Operation` to complete. This class combines both
   *       the backoff policy for checking long running operations and the
   *       retry policy.
   *
   * @see GenericPollingPolicy, ExponentialBackoffPolicy,
   *     LimitedErrorCountRetryPolicy, LimitedTimeRetryPolicy.
   */
  template <typename... Policies>
  InstanceAdmin(std::shared_ptr<InstanceAdminClient> client,
                Policies&&... policies)
      : InstanceAdmin(std::move(client)) {
    ChangePolicies(std::forward<Policies>(policies)...);
  }

  /// The full name (`projects/<project_id>`) of the project.
  std::string const& project_name() const { return project_name_; }
  /// The project id, i.e., `project_name()` without the `projects/` prefix.
  std::string const& project_id() const { return client_->project(); }

  /// Return the fully qualified name of the given instance_id.
  std::string InstanceName(std::string const& instance_id) const {
    return project_name() + "/instances/" + instance_id;
  }

  /// Return the fully qualified name of the given cluster_id in give
  /// instance_id.
  std::string ClusterName(bigtable::InstanceId const& instance_id,
                          bigtable::ClusterId const& cluster_id) const {
    return project_name() + "/instances/" + instance_id.get() + "/clusters/" +
           cluster_id.get();
  }

  //@{
  /**
   * @name No exception versions of InstanceAdmin::*
   *
   * These functions provide the same functionality as their counterparts in the
   * `bigtable::InstanceAdmin` class, but do not raise exceptions on errors,
   * instead they return the error on the status parameter.
   */
  InstanceList ListInstances(grpc::Status& status);

  /**
   * Makes an asynchronous request to list instances
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param callback a functor to be called when the operation completes. It
   *     must satisfy (using C++17 types):
   *     static_assert(std::is_invocable_v<
   *         Functor, InstanceList&, grpc::Status const&>);
   * @return a handle to the submitted operation
   *
   * @tparam Functor the type of the callback.
   */
  template <typename Functor,
            typename std::enable_if<google::cloud::internal::is_invocable<
                                        Functor, CompletionQueue&,
                                        InstanceList&, grpc::Status&>::value,
                                    int>::type valid_callback_type = 0>
  std::shared_ptr<AsyncOperation> AsyncListInstances(CompletionQueue& cq,
                                                     Functor&& callback) {
    auto op = std::make_shared<internal::AsyncRetryListInstances<Functor>>(
        __func__, rpc_retry_policy_->clone(), rpc_backoff_policy_->clone(),
        metadata_update_policy_, client_, project_name_,
        std::forward<Functor>(callback));
    return op->Start(cq);
  }

  /**
   * Makes an asynchronous request to create a instance.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param instance_config the desired configuration of the instance.
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param callback a functor to be called when the operation completes. It
   *     must satisfy (using C++17 types):
   *     static_assert(std::is_invocable_v<
   *         Functor, CompletionQueue&,
   *         google::bigtable::admin::v2::Instance&,
   *         grpc::Status&>);
   *
   * @tparam Functor the type of the callback.
   *
   * @tparam valid_callback_type a formal parameter, uses
   *     `std::enable_if<>` to disable this template if Functor has a wrong
   *     signature.
   */
  template <typename Functor,
            typename std::enable_if<google::cloud::internal::is_invocable<
                                        Functor, CompletionQueue&,
                                        google::bigtable::admin::v2::Instance&,
                                        grpc::Status&>::value,
                                    int>::type valid_callback_type = 0>
  std::shared_ptr<AsyncOperation> AsyncCreateInstance(
      CompletionQueue& cq, Functor&& callback,
      bigtable::InstanceConfig instance_config) {
    static_assert(internal::ExtractMemberFunctionType<decltype(
                      &InstanceAdminClient::AsyncCreateInstance)>::value,
                  "Cannot extract member function type");
    using MemberFunction =
        typename internal::ExtractMemberFunctionType<decltype(
            &InstanceAdminClient::AsyncCreateInstance)>::MemberFunction;

    using Operation = internal::AsyncRetryAndPollUnaryRpc<
        InstanceAdminClient, google::bigtable::admin::v2::Instance,
        MemberFunction, internal::ConstantIdempotencyPolicy, Functor>;

    auto request = std::move(instance_config).as_proto();
    request.set_parent(project_name());
    for (auto& kv : *request.mutable_clusters()) {
      kv.second.set_location(project_name() + "/locations/" +
                             kv.second.location());
    }

    auto op = std::make_shared<Operation>(
        __func__, polling_policy_->clone(), rpc_retry_policy_->clone(),
        rpc_backoff_policy_->clone(), internal::ConstantIdempotencyPolicy(true),
        metadata_update_policy_, client_,
        &InstanceAdminClient::AsyncCreateInstance, std::move(request),
        std::forward<Functor>(callback));
    return op->Start(cq);
  }

  /**
   * Makes an asynchronous request to Update an existing instance of Cloud
   * Bigtable.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param instance_update_config config with modified instance..
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param callback a functor to be called when the operation completes. It
   *     must satisfy (using C++17 types):
   *     static_assert(std::is_invocable_v<
   *         Functor, CompletionQueue&,
   *         google::bigtable::admin::v2::Instance&,
   *         grpc::Status&>);
   *
   * @tparam Functor the type of the callback.
   *
   * @tparam valid_callback_type a formal parameter, uses
   *     `std::enable_if<>` to disable this template if Functor has a wrong
   *     signature.
   */
  template <typename Functor,
            typename std::enable_if<google::cloud::internal::is_invocable<
                                        Functor, CompletionQueue&,
                                        google::bigtable::admin::v2::Instance&,
                                        grpc::Status&>::value,
                                    int>::type valid_callback_type = 0>
  std::shared_ptr<AsyncOperation> AsyncUpdateInstance(
      CompletionQueue& cq, Functor&& callback,
      InstanceUpdateConfig instance_update_config) {
    static_assert(internal::ExtractMemberFunctionType<decltype(
                      &InstanceAdminClient::AsyncUpdateInstance)>::value,
                  "Cannot extract member function type");
    using MemberFunction =
        typename internal::ExtractMemberFunctionType<decltype(
            &InstanceAdminClient::AsyncUpdateInstance)>::MemberFunction;

    using Operation = internal::AsyncRetryAndPollUnaryRpc<
        InstanceAdminClient, google::bigtable::admin::v2::Instance,
        MemberFunction, internal::ConstantIdempotencyPolicy, Functor>;

    auto request = std::move(instance_update_config).as_proto();
    auto op = std::make_shared<Operation>(
        __func__, polling_policy_->clone(), rpc_retry_policy_->clone(),
        rpc_backoff_policy_->clone(), internal::ConstantIdempotencyPolicy(true),
        metadata_update_policy_, client_,
        &InstanceAdminClient::AsyncUpdateInstance, std::move(request),
        std::forward<Functor>(callback));
    return op->Start(cq);
  }

  google::bigtable::admin::v2::Instance GetInstance(
      std::string const& instance_id, grpc::Status& status);

  /**
   * Makes an asynchronous request to get the attributes of an instance.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param instance_id the id of the instance in the project that to be
   *     retrieved.
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param callback a functor to be called when the operation completes. It
   *     must satisfy (using C++17 types):
   *     static_assert(std::is_invocable_v<
   *         Functor, google::bigtable::admin::v2::Instance&,
   *         grpc::Status const&>);
   * @return a handle to the submitted operation
   *
   * @tparam Functor the type of the callback.
   */
  template <typename Functor,
            typename std::enable_if<google::cloud::internal::is_invocable<
                                        Functor, CompletionQueue&,
                                        google::bigtable::admin::v2::Instance&,
                                        grpc::Status&>::value,
                                    int>::type valid_callback_type = 0>
  std::shared_ptr<AsyncOperation> AsyncGetInstance(
      CompletionQueue& cq, Functor&& callback, std::string const& instance_id) {
    google::bigtable::admin::v2::GetInstanceRequest request;
    // Setting instance name.
    request.set_name(project_name_ + "/instances/" + instance_id);

    static_assert(internal::ExtractMemberFunctionType<decltype(
                      &InstanceAdminClient::AsyncGetInstance)>::value,
                  "Cannot extract member function type");
    using MemberFunction =
        typename internal::ExtractMemberFunctionType<decltype(
            &InstanceAdminClient::AsyncGetInstance)>::MemberFunction;

    using Retry =
        internal::AsyncRetryUnaryRpc<InstanceAdminClient, MemberFunction,
                                     internal::ConstantIdempotencyPolicy,
                                     Functor>;

    auto retry = std::make_shared<Retry>(
        __func__, rpc_retry_policy_->clone(), rpc_backoff_policy_->clone(),
        internal::ConstantIdempotencyPolicy(true), metadata_update_policy_,
        client_, &InstanceAdminClient::AsyncGetInstance, std::move(request),
        std::forward<Functor>(callback));
    return retry->Start(cq);
  }

  void DeleteInstance(std::string const& instance_id, grpc::Status& status);

  /**
   * Makes an asynchronous request to delete an instance.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param instance_id the id of the instance in the project to be deleted.
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param callback a functor to be called when the operation completes. It
   *     must satisfy (using C++17 types):
   *     static_assert(std::is_invocable_v<Functor, grpc::Status const&>);
   * @return a handle to the submitted operation
   *
   * @tparam Functor the type of the callback.
   *
   */
  template <typename Functor,
            typename std::enable_if<
                google::cloud::internal::is_invocable<Functor, CompletionQueue&,
                                                      grpc::Status&>::value,
                int>::type valid_callback_type = 0>
  std::shared_ptr<AsyncOperation> AsyncDeleteInstance(
      CompletionQueue& cq, Functor&& callback, std::string const& instance_id) {
    google::bigtable::admin::v2::DeleteInstanceRequest request;
    // Setting instance name.
    request.set_name(InstanceName(instance_id));

    static_assert(internal::ExtractMemberFunctionType<decltype(
                      &InstanceAdminClient::AsyncDeleteInstance)>::value,
                  "Cannot extract member function type");
    using MemberFunction =
        typename internal::ExtractMemberFunctionType<decltype(
            &InstanceAdminClient::AsyncDeleteInstance)>::MemberFunction;

    using Retry =
        internal::AsyncRetryUnaryRpc<InstanceAdminClient, MemberFunction,
                                     internal::ConstantIdempotencyPolicy,
                                     internal::EmptyResponseAdaptor<Functor>>;

    auto retry = std::make_shared<Retry>(
        __func__, rpc_retry_policy_->clone(), rpc_backoff_policy_->clone(),
        internal::ConstantIdempotencyPolicy(true), metadata_update_policy_,
        client_, &InstanceAdminClient::AsyncDeleteInstance, std::move(request),
        internal::EmptyResponseAdaptor<Functor>(
            std::forward<Functor>(callback)));
    return retry->Start(cq);
  }

  ClusterList ListClusters(std::string const& instance_id,
                           grpc::Status& status);

  /**
   * Makes an asynchronous request to list clusters
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param instance_id the id of the instance from which clusters will be
   *     listed
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param callback a functor to be called when the operation completes. It
   *     must satisfy (using C++17 types):
   *     static_assert(std::is_invocable_v<
   *         Functor, ClusterList&, grpc::Status const&>);
   * @return a handle to the submitted operation
   *
   * @tparam Functor the type of the callback.
   */
  template <typename Functor,
            typename std::enable_if<google::cloud::internal::is_invocable<
                                        Functor, CompletionQueue&, ClusterList&,
                                        grpc::Status&>::value,
                                    int>::type valid_callback_type = 0>
  std::shared_ptr<AsyncOperation> AsyncListClusters(
      CompletionQueue& cq, Functor&& callback, std::string const& instance_id) {
    auto op = std::make_shared<internal::AsyncRetryListClusters<Functor>>(
        __func__, rpc_retry_policy_->clone(), rpc_backoff_policy_->clone(),
        metadata_update_policy_, client_, InstanceName(instance_id),
        std::forward<Functor>(callback));
    return op->Start(cq);
  }

  void DeleteCluster(bigtable::InstanceId const& instance_id,
                     bigtable::ClusterId const& cluster_id,
                     grpc::Status& status);

  /**
   * Makes an asynchronous request to delete the cluster.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param instance_id the Cloud Bigtable instance that contains the cluster.
   * @param cluster_id the id of the cluster in the project to be deleted.
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param callback a functor to be called when the operation completes. It
   *     must satisfy (using C++17 types):
   *     static_assert(std::is_invocable_v<Functor, grpc::Status const&>);
   * @return a handle to the submitted operation
   *
   * @tparam Functor the type of the callback.
   *
   */
  template <typename Functor,
            typename std::enable_if<
                google::cloud::internal::is_invocable<Functor, CompletionQueue&,
                                                      grpc::Status&>::value,
                int>::type valid_callback_type = 0>
  std::shared_ptr<AsyncOperation> AsyncDeleteCluster(
      CompletionQueue& cq, Functor&& callback,
      bigtable::InstanceId const& instance_id,
      bigtable::ClusterId const& cluster_id) {
    google::bigtable::admin::v2::DeleteClusterRequest request;
    // Setting cluster name.
    request.set_name(ClusterName(instance_id, cluster_id));

    static_assert(internal::ExtractMemberFunctionType<decltype(
                      &InstanceAdminClient::AsyncDeleteCluster)>::value,
                  "Cannot extract member function type");
    using MemberFunction =
        typename internal::ExtractMemberFunctionType<decltype(
            &InstanceAdminClient::AsyncDeleteCluster)>::MemberFunction;

    using Retry =
        internal::AsyncRetryUnaryRpc<InstanceAdminClient, MemberFunction,
                                     internal::ConstantIdempotencyPolicy,
                                     internal::EmptyResponseAdaptor<Functor>>;

    auto retry = std::make_shared<Retry>(
        __func__, rpc_retry_policy_->clone(), rpc_backoff_policy_->clone(),
        internal::ConstantIdempotencyPolicy(true), metadata_update_policy_,
        client_, &InstanceAdminClient::AsyncDeleteCluster, std::move(request),
        internal::EmptyResponseAdaptor<Functor>(
            std::forward<Functor>(callback)));
    return retry->Start(cq);
  }

  /**
   * Makes an asynchronous request to create a cluster.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param cluster_config the desired configuration of the cluster.
   * @param instance_id the Cloud Bigtable instance that contains the cluster.
   * @param cluster_id the id of the cluster in the project to be deleted.
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param callback a functor to be called when the operation completes. It
   *     must satisfy (using C++17 types):
   *     static_assert(std::is_invocable_v<
   *         Functor, CompletionQueue&,
   *         google::bigtable::admin::v2::Cluster&,
   *         grpc::Status&>);
   * @return a handle to the submitted operation
   *
   * @tparam Functor the type of the callback.
   *
   * @tparam valid_callback_type a formal parameter, uses
   *     `std::enable_if<>` to disable this template if Functor has a wrong
   *     signature.
   */
  template <typename Functor,
            typename std::enable_if<google::cloud::internal::is_invocable<
                                        Functor, CompletionQueue&,
                                        google::bigtable::admin::v2::Cluster&,
                                        grpc::Status&>::value,
                                    int>::type valid_callback_type = 0>
  std::shared_ptr<AsyncOperation> AsyncCreateCluster(
      CompletionQueue& cq, Functor&& callback,
      bigtable::ClusterConfig cluster_config,
      bigtable::InstanceId const& instance_id,
      bigtable::ClusterId const& cluster_id) {
    static_assert(internal::ExtractMemberFunctionType<decltype(
                      &InstanceAdminClient::AsyncCreateCluster)>::value,
                  "Cannot extract member function type");
    using MemberFunction =
        typename internal::ExtractMemberFunctionType<decltype(
            &InstanceAdminClient::AsyncCreateCluster)>::MemberFunction;

    using Operation = internal::AsyncRetryAndPollUnaryRpc<
        InstanceAdminClient, google::bigtable::admin::v2::Cluster,
        MemberFunction, internal::ConstantIdempotencyPolicy, Functor>;

    auto cluster = std::move(cluster_config).as_proto();
    cluster.set_location(project_name() + "/locations/" + cluster.location());

    google::bigtable::admin::v2::CreateClusterRequest request;
    request.mutable_cluster()->Swap(&cluster);
    request.set_parent(project_name() + "/instances/" + instance_id.get());
    request.set_cluster_id(cluster_id.get());
    auto op = std::make_shared<Operation>(
        __func__, polling_policy_->clone(), rpc_retry_policy_->clone(),
        rpc_backoff_policy_->clone(), internal::ConstantIdempotencyPolicy(true),
        metadata_update_policy_, client_,
        &InstanceAdminClient::AsyncCreateCluster, std::move(request),
        std::forward<Functor>(callback));
    return op->Start(cq);
  }

  /**
   * Makes an asynchronous request to update an existing cluster of Cloud
   * Bigtable.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param cluster_config cluster with updated values.
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param callback a functor to be called when the operation completes. It
   *     must satisfy (using C++17 types):
   *     static_assert(std::is_invocable_v<
   *         Functor, CompletionQueue&,
   *         google::bigtable::admin::v2::Cluster&,
   *         grpc::Status&>);
   *
   * @tparam Functor the type of the callback.
   *
   * @tparam valid_callback_type a formal parameter, uses
   *     `std::enable_if<>` to disable this template if Functor has a wrong
   *     signature.
   */
  template <typename Functor,
            typename std::enable_if<google::cloud::internal::is_invocable<
                                        Functor, CompletionQueue&,
                                        google::bigtable::admin::v2::Cluster&,
                                        grpc::Status&>::value,
                                    int>::type valid_callback_type = 0>
  std::shared_ptr<AsyncOperation> AsyncUpdateCluster(
      CompletionQueue& cq, Functor&& callback, ClusterConfig cluster_config) {
    static_assert(internal::ExtractMemberFunctionType<decltype(
                      &InstanceAdminClient::AsyncUpdateCluster)>::value,
                  "Cannot extract member function type");
    using MemberFunction =
        typename internal::ExtractMemberFunctionType<decltype(
            &InstanceAdminClient::AsyncUpdateCluster)>::MemberFunction;

    using Operation = internal::AsyncRetryAndPollUnaryRpc<
        InstanceAdminClient, google::bigtable::admin::v2::Cluster,
        MemberFunction, internal::ConstantIdempotencyPolicy, Functor>;

    auto request = std::move(cluster_config).as_proto();
    auto op = std::make_shared<Operation>(
        __func__, polling_policy_->clone(), rpc_retry_policy_->clone(),
        rpc_backoff_policy_->clone(), internal::ConstantIdempotencyPolicy(true),
        metadata_update_policy_, client_,
        &InstanceAdminClient::AsyncUpdateCluster, std::move(request),
        std::forward<Functor>(callback));
    return op->Start(cq);
  }

  google::bigtable::admin::v2::Cluster GetCluster(
      bigtable::InstanceId const& instance_id,
      bigtable::ClusterId const& cluster_id, grpc::Status& status);

  /**
   * Makes an asynchronous request to get the attributes of a cluster.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param instance_id the Cloud Bigtable instance that contains the cluster.
   * @param cluster_id fetch the attributes for this cluster id. The full name
   * of the cluster is
   * `projects/<PROJECT_ID>/instances/<instance_id>/clusters/<cluster_id>`
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param callback a functor to be called when the operation completes. It
   *     must satisfy (using C++17 types):
   *     static_assert(std::is_invocable_v<
   *         Functor, google::bigtable::admin::v2::Cluster&,
   *         grpc::Status const&>);
   * @return a handle to the submitted operation
   *
   * @tparam Functor the type of the callback.
   */
  template <typename Functor,
            typename std::enable_if<google::cloud::internal::is_invocable<
                                        Functor, CompletionQueue&,
                                        google::bigtable::admin::v2::Cluster&,
                                        grpc::Status&>::value,
                                    int>::type valid_callback_type = 0>
  std::shared_ptr<AsyncOperation> AsyncGetCluster(
      CompletionQueue& cq, Functor&& callback,
      bigtable::InstanceId const& instance_id,
      bigtable::ClusterId const& cluster_id) {
    google::bigtable::admin::v2::GetClusterRequest request;
    // Setting cluster name.
    request.set_name(ClusterName(instance_id, cluster_id));

    static_assert(internal::ExtractMemberFunctionType<decltype(
                      &InstanceAdminClient::AsyncGetCluster)>::value,
                  "Cannot extract member function type");
    using MemberFunction =
        typename internal::ExtractMemberFunctionType<decltype(
            &InstanceAdminClient::AsyncGetCluster)>::MemberFunction;

    using Retry =
        internal::AsyncRetryUnaryRpc<InstanceAdminClient, MemberFunction,
                                     internal::ConstantIdempotencyPolicy,
                                     Functor>;

    auto retry = std::make_shared<Retry>(
        __func__, rpc_retry_policy_->clone(), rpc_backoff_policy_->clone(),
        internal::ConstantIdempotencyPolicy(true), metadata_update_policy_,
        client_, &InstanceAdminClient::AsyncGetCluster, std::move(request),
        std::forward<Functor>(callback));
    return retry->Start(cq);
  }

  google::longrunning::Operation UpdateAppProfile(
      bigtable::InstanceId instance_id, bigtable::AppProfileId profile_id,
      AppProfileUpdateConfig config, grpc::Status& status);

  /**
   * Make an asynchronous request to update the profile.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param instance_id the Cloud Bigtable instance that contains the profile.
   * @param profile_id update the attributes for this profile id. The full name
   * of the profile is
   * `projects/<PROJECT_ID>/instances/<instance_id>/appProfiles/<profile_id>`
   * @param config the modified configuration for the profile.
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param callback a functor to be called when the operation completes. It
   *     must satisfy (using C++17 types):
   *     static_assert(std::is_invocable_v<
   *         Functor, CompletionQueue&,
   *         google::bigtable::admin::v2::AppProfile&,
   *         grpc::Status&>);
   *
   * @tparam Functor the type of the callback.
   *
   * @tparam valid_callback_type a formal parameter, uses
   *     `std::enable_if<>` to disable this template if Functor has a wrong
   *     signature.
   */
  template <
      typename Functor,
      typename std::enable_if<
          google::cloud::internal::is_invocable<
              Functor, CompletionQueue&,
              google::bigtable::admin::v2::AppProfile&, grpc::Status&>::value,
          int>::type valid_callback_type = 0>
  std::shared_ptr<AsyncOperation> AsyncUpdateAppProfile(
      CompletionQueue& cq, Functor&& callback,
      bigtable::InstanceId const& instance_id,
      bigtable::AppProfileId profile_id, AppProfileUpdateConfig config) {
    static_assert(internal::ExtractMemberFunctionType<decltype(
                      &InstanceAdminClient::AsyncUpdateAppProfile)>::value,
                  "Cannot extract member function type");
    using MemberFunction =
        typename internal::ExtractMemberFunctionType<decltype(
            &InstanceAdminClient::AsyncUpdateAppProfile)>::MemberFunction;

    using Operation = internal::AsyncRetryAndPollUnaryRpc<
        InstanceAdminClient, google::bigtable::admin::v2::AppProfile,
        MemberFunction, internal::ConstantIdempotencyPolicy, Functor>;

    auto request = std::move(config).as_proto();
    request.mutable_app_profile()->set_name(
        InstanceName(instance_id.get() + "/appProfiles/" + profile_id.get()));

    auto op = std::make_shared<Operation>(
        __func__, polling_policy_->clone(), rpc_retry_policy_->clone(),
        rpc_backoff_policy_->clone(), internal::ConstantIdempotencyPolicy(true),
        metadata_update_policy_, client_,
        &InstanceAdminClient::AsyncUpdateAppProfile, std::move(request),
        std::forward<Functor>(callback));
    return op->Start(cq);
  }

  google::bigtable::admin::v2::AppProfile CreateAppProfile(
      bigtable::InstanceId const& instance_id, AppProfileConfig config,
      grpc::Status& status);

  /**
   * Make an asynchronous request to create the profile.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param instance_id the Cloud Bigtable instance that contains the profile.
   * @param config the initial configuration for the profile.
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param callback a functor to be called when the operation completes. It
   *     must satisfy (using C++17 types):
   *     static_assert(std::is_invocable_v<
   *         Functor, google::bigtable::admin::v2::AppProfile&,
   *         grpc::Status const&>);
   * @return a handle to the submitted operation
   *
   * @tparam Functor the type of the callback.
   */
  template <
      typename Functor,
      typename std::enable_if<
          google::cloud::internal::is_invocable<
              Functor, CompletionQueue&,
              google::bigtable::admin::v2::AppProfile&, grpc::Status&>::value,
          int>::type valid_callback_type = 0>
  std::shared_ptr<AsyncOperation> AsyncCreateAppProfile(
      CompletionQueue& cq, Functor&& callback,
      bigtable::InstanceId const& instance_id, AppProfileConfig config) {
    auto request = std::move(config).as_proto();
    request.set_parent(InstanceName(instance_id.get()));

    static_assert(internal::ExtractMemberFunctionType<decltype(
                      &InstanceAdminClient::AsyncCreateAppProfile)>::value,
                  "Cannot extract member function type");
    using MemberFunction =
        typename internal::ExtractMemberFunctionType<decltype(
            &InstanceAdminClient::AsyncCreateAppProfile)>::MemberFunction;

    using Retry =
        internal::AsyncRetryUnaryRpc<InstanceAdminClient, MemberFunction,
                                     internal::ConstantIdempotencyPolicy,
                                     Functor>;

    auto retry = std::make_shared<Retry>(
        __func__, rpc_retry_policy_->clone(), rpc_backoff_policy_->clone(),
        internal::ConstantIdempotencyPolicy(true), metadata_update_policy_,
        client_, &InstanceAdminClient::AsyncCreateAppProfile,
        std::move(request), std::forward<Functor>(callback));
    return retry->Start(cq);
  }

  google::bigtable::admin::v2::AppProfile GetAppProfile(
      bigtable::InstanceId const& instance_id,
      bigtable::AppProfileId const& profile_id, grpc::Status& status);

  /**
   * Makes an asynchronous request to get the attributes of a profile.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param instance_id the Cloud Bigtable instance that contains the profile.
   * @param profile_id fetch the attributes for this profile id. The full name
   * of the profile is
   * `projects/<PROJECT_ID>/instances/<instance_id>/appProfiles/<profile_id>`
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param callback a functor to be called when the operation completes. It
   *     must satisfy (using C++17 types):
   *     static_assert(std::is_invocable_v<
   *         Functor, google::bigtable::admin::v2::AppProfile&,
   *         grpc::Status const&>);
   * @return a handle to the submitted operation
   *
   * @tparam Functor the type of the callback.
   */
  template <
      typename Functor,
      typename std::enable_if<
          google::cloud::internal::is_invocable<
              Functor, CompletionQueue&,
              google::bigtable::admin::v2::AppProfile&, grpc::Status&>::value,
          int>::type valid_callback_type = 0>
  std::shared_ptr<AsyncOperation> AsyncGetAppProfile(
      CompletionQueue& cq, Functor&& callback,
      bigtable::InstanceId const& instance_id,
      bigtable::AppProfileId const& profile_id) {
    google::bigtable::admin::v2::GetAppProfileRequest request;
    // Setting profile name.
    request.set_name(InstanceName(instance_id.get()) + "/appProfiles/" +
                     profile_id.get());

    static_assert(internal::ExtractMemberFunctionType<decltype(
                      &InstanceAdminClient::AsyncGetAppProfile)>::value,
                  "Cannot extract member function type");
    using MemberFunction =
        typename internal::ExtractMemberFunctionType<decltype(
            &InstanceAdminClient::AsyncGetAppProfile)>::MemberFunction;

    using Retry =
        internal::AsyncRetryUnaryRpc<InstanceAdminClient, MemberFunction,
                                     internal::ConstantIdempotencyPolicy,
                                     Functor>;

    auto retry = std::make_shared<Retry>(
        __func__, rpc_retry_policy_->clone(), rpc_backoff_policy_->clone(),
        internal::ConstantIdempotencyPolicy(true), metadata_update_policy_,
        client_, &InstanceAdminClient::AsyncGetAppProfile, std::move(request),
        std::forward<Functor>(callback));
    return retry->Start(cq);
  }

  std::vector<google::bigtable::admin::v2::AppProfile> ListAppProfiles(
      std::string const& instance_id, grpc::Status& status);

  /**
   * Makes an asynchronous request to list app profiles
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param instance_id the id of the instance from which profiles will be
   *     listed
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param callback a functor to be called when the operation completes. It
   *     must satisfy (using C++17 types):
   *     static_assert(std::is_invocable_v<
   *         Functor, std::vector<google::bigtable::admin::v2::AppProfile>&,
   * grpc::Status const&>);
   * @return a handle to the submitted operation
   *
   * @tparam Functor the type of the callback.
   */
  template <typename Functor,
            typename std::enable_if<
                google::cloud::internal::is_invocable<
                    Functor, CompletionQueue&,
                    std::vector<google::bigtable::admin::v2::AppProfile>&,
                    grpc::Status&>::value,
                int>::type valid_callback_type = 0>
  std::shared_ptr<AsyncOperation> AsyncListAppProfiles(
      CompletionQueue& cq, Functor&& callback, std::string const& instance_id) {
    auto op = std::make_shared<internal::AsyncRetryListAppProfiles<Functor>>(
        __func__, rpc_retry_policy_->clone(), rpc_backoff_policy_->clone(),
        metadata_update_policy_, client_, InstanceName(instance_id),
        std::forward<Functor>(callback));
    return op->Start(cq);
  }

  void DeleteAppProfile(bigtable::InstanceId const& instance_id,
                        bigtable::AppProfileId const& profile_id,
                        bool ignore_warnings, grpc::Status& status);

  /**
   * Makes an asynchronous request to delete the profile.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param instance_id the Cloud Bigtable instance that contains the cluster.
   * @param profile_id the id of the profile in the project to be deleted.
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param callback a functor to be called when the operation completes. It
   *     must satisfy (using C++17 types):
   *     static_assert(std::is_invocable_v<Functor, grpc::Status const&>);
   * @return a handle to the submitted operation
   *
   * @tparam Functor the type of the callback.
   *
   */
  template <typename Functor,
            typename std::enable_if<
                google::cloud::internal::is_invocable<Functor, CompletionQueue&,
                                                      grpc::Status&>::value,
                int>::type valid_callback_type = 0>
  std::shared_ptr<AsyncOperation> AsyncDeleteAppProfile(
      CompletionQueue& cq, Functor&& callback,
      bigtable::InstanceId const& instance_id,
      bigtable::AppProfileId const& profile_id, bool ignore_warnings = true) {
    google::bigtable::admin::v2::DeleteAppProfileRequest request;
    // Setting profile name.
    request.set_name(InstanceName(instance_id.get()) + "/appProfiles/" +
                     profile_id.get());
    request.set_ignore_warnings(ignore_warnings);

    static_assert(internal::ExtractMemberFunctionType<decltype(
                      &InstanceAdminClient::AsyncDeleteAppProfile)>::value,
                  "Cannot extract member function type");
    using MemberFunction =
        typename internal::ExtractMemberFunctionType<decltype(
            &InstanceAdminClient::AsyncDeleteAppProfile)>::MemberFunction;

    using Retry =
        internal::AsyncRetryUnaryRpc<InstanceAdminClient, MemberFunction,
                                     internal::ConstantIdempotencyPolicy,
                                     internal::EmptyResponseAdaptor<Functor>>;

    auto retry = std::make_shared<Retry>(
        __func__, rpc_retry_policy_->clone(), rpc_backoff_policy_->clone(),
        internal::ConstantIdempotencyPolicy(true), metadata_update_policy_,
        client_, &InstanceAdminClient::AsyncDeleteAppProfile,
        std::move(request),
        internal::EmptyResponseAdaptor<Functor>(
            std::forward<Functor>(callback)));
    return retry->Start(cq);
  }

  google::cloud::IamPolicy GetIamPolicy(std::string const& instance_id,
                                        grpc::Status& status);

  /**
   * Makes an asynchronous request to get the IAM policy of an instance.
   *
   * @param instance_id the Cloud Bigtable instance that hold the IAM policy.
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param callback a functor to be called when the operation completes. It
   *     must satisfy (using C++17 types):
   *     static_assert(std::is_invocable_v<
   *         Functor, google::cloud::IamPolicy,
   *         grpc::Status const&>);
   *
   * @tparam Functor the type of the callback.
   */
  template <typename Functor,
            typename std::enable_if<
                google::cloud::internal::is_invocable<Functor, CompletionQueue&,
                                                      google::cloud::IamPolicy,
                                                      grpc::Status&>::value,
                int>::type valid_callback_type = 0>
  std::shared_ptr<AsyncOperation> AsyncGetIamPolicy(
      std::string const& instance_id, CompletionQueue& cq, Functor&& callback) {
    ::google::iam::v1::GetIamPolicyRequest request;
    request.set_resource(InstanceName(instance_id));

    MetadataUpdatePolicy metadata_update_policy(project_name(),
                                                MetadataParamTypes::RESOURCE);

    static_assert(internal::ExtractMemberFunctionType<decltype(
                      &InstanceAdminClient::AsyncGetIamPolicy)>::value,
                  "Cannot extract member function type");
    using MemberFunction =
        typename internal::ExtractMemberFunctionType<decltype(
            &InstanceAdminClient::AsyncGetIamPolicy)>::MemberFunction;

    using Retry =
        internal::AsyncRetryUnaryRpc<InstanceAdminClient, MemberFunction,
                                     internal::ConstantIdempotencyPolicy,
                                     TransformResponseHelper<Functor>>;

    auto retry = std::make_shared<Retry>(
        __func__, rpc_retry_policy_->clone(), rpc_backoff_policy_->clone(),
        internal::ConstantIdempotencyPolicy(true), metadata_update_policy,
        client_, &InstanceAdminClient::AsyncGetIamPolicy, std::move(request),
        TransformResponseHelper<Functor>(std::forward<Functor>(callback)));
    return retry->Start(cq);
  }

  google::cloud::IamPolicy SetIamPolicy(
      std::string const& instance_id,
      google::cloud::IamBindings const& iam_bindings, std::string const& etag,
      grpc::Status& status);

  std::vector<std::string> TestIamPermissions(
      std::string const& instance_id,
      std::vector<std::string> const& permissions, grpc::Status& status);
  //@}

 private:
  static inline google::cloud::IamPolicy ProtoToWrapper(
      google::iam::v1::Policy proto) {
    google::cloud::IamPolicy result;
    result.version = proto.version();
    result.etag = std::move(*proto.mutable_etag());
    for (auto& binding : *proto.mutable_bindings()) {
      for (auto& member : *binding.mutable_members()) {
        result.bindings.AddMember(binding.role(), std::move(member));
      }
    }

    return result;
  }

  template <typename Functor,
            typename std::enable_if<
                google::cloud::internal::is_invocable<Functor, CompletionQueue&,
                                                      google::cloud::IamPolicy,
                                                      grpc::Status&>::value,
                int>::type valid_callback_type = 0>
  struct TransformResponseHelper {
    TransformResponseHelper(Functor callback)
        : application_callback_(std::move(callback)) {}

    void operator()(CompletionQueue& cq, google::iam::v1::Policy& response,
                    grpc::Status& status) {
      application_callback_(cq, ProtoToWrapper(response), status);
    }

    Functor application_callback_;
  };

  //@{
  /// @name Helper functions to implement constructors with changed policies.
  void ChangePolicy(RPCRetryPolicy& policy) {
    rpc_retry_policy_ = policy.clone();
  }

  void ChangePolicy(RPCBackoffPolicy& policy) {
    rpc_backoff_policy_ = policy.clone();
  }

  void ChangePolicy(PollingPolicy& policy) { polling_policy_ = policy.clone(); }

  template <typename Policy, typename... Policies>
  void ChangePolicies(Policy&& policy, Policies&&... policies) {
    ChangePolicy(policy);
    ChangePolicies(std::forward<Policies>(policies)...);
  }
  void ChangePolicies() {}
  //@}

  friend class bigtable::BIGTABLE_CLIENT_NS::InstanceAdmin;
  std::shared_ptr<InstanceAdminClient> client_;
  std::string project_name_;
  std::shared_ptr<RPCRetryPolicy> rpc_retry_policy_;
  std::shared_ptr<RPCBackoffPolicy> rpc_backoff_policy_;
  std::shared_ptr<PollingPolicy> polling_policy_;
  MetadataUpdatePolicy metadata_update_policy_;
};

}  // namespace noex
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_INSTANCE_ADMIN_H_
