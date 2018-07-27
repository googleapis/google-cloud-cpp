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
#include "google/cloud/bigtable/internal/grpc_error_delegate.h"
#include "google/cloud/bigtable/metadata_update_policy.h"
#include "google/cloud/bigtable/polling_policy.h"
#include "google/cloud/bigtable/rpc_backoff_policy.h"
#include "google/cloud/bigtable/rpc_retry_policy.h"
#include "google/cloud/iam_policy.h"
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
        rpc_retry_policy_(DefaultRPCRetryPolicy()),
        rpc_backoff_policy_(DefaultRPCBackoffPolicy()),
        polling_policy_(DefaultPollingPolicy()),
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
  std::vector<::google::bigtable::admin::v2::Instance> ListInstances(
      grpc::Status& status);

  ::google::bigtable::admin::v2::Instance GetInstance(
      std::string const& instance_id, grpc::Status& status);

  void DeleteInstance(std::string const& instance_id, grpc::Status& status);

  std::vector<::google::bigtable::admin::v2::Cluster> ListClusters(
      std::string const& instance_id, grpc::Status& status);

  void DeleteCluster(bigtable::InstanceId const& instance_id,
                     bigtable::ClusterId const& cluster_id,
                     grpc::Status& status);

  ::google::bigtable::admin::v2::Cluster GetCluster(
      bigtable::InstanceId const& instance_id,
      bigtable::ClusterId const& cluster_id, grpc::Status& status);

  ::google::longrunning::Operation UpdateAppProfile(
      bigtable::InstanceId instance_id, bigtable::AppProfileId profile_id,
      AppProfileUpdateConfig config, grpc::Status& status);

  ::google::bigtable::admin::v2::AppProfile CreateAppProfile(
      bigtable::InstanceId const& instance_id, AppProfileConfig config,
      grpc::Status& status);

  ::google::bigtable::admin::v2::AppProfile GetAppProfile(
      bigtable::InstanceId const& instance_id,
      bigtable::AppProfileId const& profile_id, grpc::Status& status);

  std::vector<::google::bigtable::admin::v2::AppProfile> ListAppProfiles(
      std::string const& instance_id, grpc::Status& status);

  void DeleteAppProfile(bigtable::InstanceId const& instance_id,
                        bigtable::AppProfileId const& profile_id,
                        bool ignore_warnings, grpc::Status& status);

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

  template <typename ResultType>
  ResultType PollLongRunningOperation(google::longrunning::Operation& operation,
                                      char const* error_message,
                                      grpc::Status& status) {
    auto polling_policy = polling_policy_->clone();
    do {
      if (operation.done()) {
        if (operation.has_response()) {
          auto const& any = operation.response();
          if (not any.Is<ResultType>()) {
            std::ostringstream os;
            os << error_message << "(" << metadata_update_policy_.value()
               << ") - "
               << "invalid result type in operation=" << operation.name();
            status = grpc::Status(grpc::StatusCode::UNKNOWN, os.str());
            return ResultType{};
          }
          ResultType result;
          any.UnpackTo(&result);
          return result;
        }
        if (operation.has_error()) {
          std::ostringstream os;
          os << error_message << "(" << metadata_update_policy_.value()
             << ") - "
             << "error reported by operation=" << operation.name();
          status = grpc::Status(
              static_cast<grpc::StatusCode>(operation.error().code()),
              operation.error().message(), os.str());
          return ResultType{};
        }
      }
      auto delay = polling_policy->WaitPeriod();
      std::this_thread::sleep_for(delay);
      google::longrunning::GetOperationRequest op;
      op.set_name(operation.name());
      grpc::ClientContext context;
      status = client_->GetOperation(&context, op, &operation);
      if (not status.ok() and not polling_policy->OnFailure(status)) {
        return ResultType{};
      }
    } while (not polling_policy->Exhausted());
    std::ostringstream os;
    os << error_message << "(" << metadata_update_policy_.value() << ") - "
       << "polling policy exhausted in operation=" << operation.name();
    status = grpc::Status(grpc::StatusCode::UNKNOWN, os.str());
    return ResultType{};
  }

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
