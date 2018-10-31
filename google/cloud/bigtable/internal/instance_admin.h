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
#include "google/cloud/bigtable/instance_admin_client.h"
#include "google/cloud/bigtable/internal/async_retry_unary_rpc.h"
#include "google/cloud/bigtable/internal/grpc_error_delegate.h"
#include "google/cloud/bigtable/metadata_update_policy.h"
#include "google/cloud/bigtable/polling_policy.h"
#include "google/cloud/bigtable/rpc_backoff_policy.h"
#include "google/cloud/bigtable/rpc_retry_policy.h"
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
  std::vector<google::bigtable::admin::v2::Instance> ListInstances(
      grpc::Status& status);

  google::bigtable::admin::v2::Instance GetInstance(
      std::string const& instance_id, grpc::Status& status);

  /**
   * Makes an asynchronous request to get the attributes of an instance.
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
   *
   * @tparam Functor the type of the callback.
   */
  template <typename Functor,
            typename std::enable_if<google::cloud::internal::is_invocable<
                                        Functor, CompletionQueue&,
                                        google::bigtable::admin::v2::Instance&,
                                        grpc::Status&>::value,
                                    int>::type valid_callback_type = 0>
  void AsyncGetInstance(std::string const& instance_id, CompletionQueue& cq,
                        Functor&& callback) {
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
    retry->Start(cq);
  }

  void DeleteInstance(std::string const& instance_id, grpc::Status& status);

  /**
   * Makes an asynchronous request to delete an instance.
   *
   * @param instance_id the id of the instance in the project to be deleted.
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param callback a functor to be called when the operation completes. It
   *     must satisfy (using C++17 types):
   *     static_assert(std::is_invocable_v<
   *         Functor, google::protobuf::Empty&,
   *         grpc::Status const&>);
   *
   * @tparam Functor the type of the callback.
   *
   * TODO(#1325) - eliminate usage of google::protobuf::Empty from Asysnc APIs.
   */
  template <typename Functor,
            typename std::enable_if<
                google::cloud::internal::is_invocable<Functor, CompletionQueue&,
                                                      google::protobuf::Empty&,
                                                      grpc::Status&>::value,
                int>::type valid_callback_type = 0>
  void AsyncDeleteInstance(std::string const& instance_id, CompletionQueue& cq,
                           Functor&& callback) {
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
                                     Functor>;

    auto retry = std::make_shared<Retry>(
        __func__, rpc_retry_policy_->clone(), rpc_backoff_policy_->clone(),
        internal::ConstantIdempotencyPolicy(true), metadata_update_policy_,
        client_, &InstanceAdminClient::AsyncDeleteInstance, std::move(request),
        std::forward<Functor>(callback));
    retry->Start(cq);
  }

  std::vector<google::bigtable::admin::v2::Cluster> ListClusters(
      std::string const& instance_id, grpc::Status& status);

  void DeleteCluster(bigtable::InstanceId const& instance_id,
                     bigtable::ClusterId const& cluster_id,
                     grpc::Status& status);

  /**
   * Makes an asynchronous request to delete the cluster.
   *
   * @param instance_id the Cloud Bigtable instance that contains the cluster.
   * @param cluster_id the id of the cluster in the project to be deleted.
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param callback a functor to be called when the operation completes. It
   *     must satisfy (using C++17 types):
   *     static_assert(std::is_invocable_v<
   *         Functor, google::protobuf::Empty&,
   *         grpc::Status const&>);
   *
   * @tparam Functor the type of the callback.
   *
   * TODO(#1325) - eliminate usage of google::protobuf::Empty from Asysnc APIs.
   */
  template <typename Functor,
            typename std::enable_if<
                google::cloud::internal::is_invocable<Functor, CompletionQueue&,
                                                      google::protobuf::Empty&,
                                                      grpc::Status&>::value,
                int>::type valid_callback_type = 0>
  void AsyncDeleteCluster(bigtable::InstanceId const& instance_id,
                          bigtable::ClusterId const& cluster_id,
                          CompletionQueue& cq, Functor&& callback) {
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
                                     Functor>;

    auto retry = std::make_shared<Retry>(
        __func__, rpc_retry_policy_->clone(), rpc_backoff_policy_->clone(),
        internal::ConstantIdempotencyPolicy(true), metadata_update_policy_,
        client_, &InstanceAdminClient::AsyncDeleteCluster, std::move(request),
        std::forward<Functor>(callback));
    retry->Start(cq);
  }

  google::bigtable::admin::v2::Cluster GetCluster(
      bigtable::InstanceId const& instance_id,
      bigtable::ClusterId const& cluster_id, grpc::Status& status);

  /**
   * Makes an asynchronous request to get the attributes of a cluster.
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
   *
   * @tparam Functor the type of the callback.
   */
  template <typename Functor,
            typename std::enable_if<google::cloud::internal::is_invocable<
                                        Functor, CompletionQueue&,
                                        google::bigtable::admin::v2::Cluster&,
                                        grpc::Status&>::value,
                                    int>::type valid_callback_type = 0>
  void AsyncGetCluster(bigtable::InstanceId const& instance_id,
                       bigtable::ClusterId const& cluster_id,
                       CompletionQueue& cq, Functor&& callback) {
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
    retry->Start(cq);
  }

  google::longrunning::Operation UpdateAppProfile(
      bigtable::InstanceId instance_id, bigtable::AppProfileId profile_id,
      AppProfileUpdateConfig config, grpc::Status& status);

  google::bigtable::admin::v2::AppProfile CreateAppProfile(
      bigtable::InstanceId const& instance_id, AppProfileConfig config,
      grpc::Status& status);

  /**
   * Make an asynchronous request to create the profile.
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
  void AsyncCreateAppProfile(bigtable::InstanceId const& instance_id,
                             AppProfileConfig config, CompletionQueue& cq,
                             Functor&& callback) {
    auto request = config.as_proto_move();
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
    retry->Start(cq);
  }

  google::bigtable::admin::v2::AppProfile GetAppProfile(
      bigtable::InstanceId const& instance_id,
      bigtable::AppProfileId const& profile_id, grpc::Status& status);

  /**
   * Makes an asynchronous request to get the attributes of a profile.
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
  void AsyncGetAppProfile(bigtable::InstanceId const& instance_id,
                          bigtable::AppProfileId const& profile_id,
                          CompletionQueue& cq, Functor&& callback) {
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
    retry->Start(cq);
  }

  std::vector<google::bigtable::admin::v2::AppProfile> ListAppProfiles(
      std::string const& instance_id, grpc::Status& status);

  void DeleteAppProfile(bigtable::InstanceId const& instance_id,
                        bigtable::AppProfileId const& profile_id,
                        bool ignore_warnings, grpc::Status& status);

  /**
   * Makes an asynchronous request to delete the profile.
   *
   * @param instance_id the Cloud Bigtable instance that contains the cluster.
   * @param profile_id the id of the profile in the project to be deleted.
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param callback a functor to be called when the operation completes. It
   *     must satisfy (using C++17 types):
   *     static_assert(std::is_invocable_v<
   *         Functor, google::protobuf::Empty&,
   *         grpc::Status const&>);
   *
   * @tparam Functor the type of the callback.
   *
   * TODO(#1325) - eliminate usage of google::protobuf::Empty from Asysnc APIs.
   */
  template <typename Functor,
            typename std::enable_if<
                google::cloud::internal::is_invocable<Functor, CompletionQueue&,
                                                      google::protobuf::Empty&,
                                                      grpc::Status&>::value,
                int>::type valid_callback_type = 0>
  void AsyncDeleteAppProfile(bigtable::InstanceId const& instance_id,
                             bigtable::AppProfileId const& profile_id,
                             CompletionQueue& cq, Functor&& callback) {
    google::bigtable::admin::v2::DeleteAppProfileRequest request;
    // Setting profile name.
    request.set_name(InstanceName(instance_id.get()) + "/appProfiles/" +
                     profile_id.get());

    static_assert(internal::ExtractMemberFunctionType<decltype(
                      &InstanceAdminClient::AsyncDeleteAppProfile)>::value,
                  "Cannot extract member function type");
    using MemberFunction =
        typename internal::ExtractMemberFunctionType<decltype(
            &InstanceAdminClient::AsyncDeleteAppProfile)>::MemberFunction;

    using Retry =
        internal::AsyncRetryUnaryRpc<InstanceAdminClient, MemberFunction,
                                     internal::ConstantIdempotencyPolicy,
                                     Functor>;

    auto retry = std::make_shared<Retry>(
        __func__, rpc_retry_policy_->clone(), rpc_backoff_policy_->clone(),
        internal::ConstantIdempotencyPolicy(true), metadata_update_policy_,
        client_, &InstanceAdminClient::AsyncDeleteAppProfile,
        std::move(request), std::forward<Functor>(callback));
    retry->Start(cq);
  }

  google::cloud::IamPolicy GetIamPolicy(std::string const& instance_id,
                                        grpc::Status& status);

  google::cloud::IamPolicy SetIamPolicy(
      std::string const& instance_id,
      google::cloud::IamBindings const& iam_bindings, std::string const& etag,
      grpc::Status& status);

  std::vector<std::string> TestIamPermissions(
      std::string const& instance_id,
      std::vector<std::string> const& permissions, grpc::Status& status);
  //@}

 private:
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
