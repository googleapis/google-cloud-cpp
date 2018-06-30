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
   * @tparam RPCRetryPolicy control which operations to retry and for how long.
   * @tparam RPCBackoffPolicy control how does the client backs off after an RPC
   *     error.
   * @param client the interface to create grpc stubs, report errors, etc.
   * @param retry_policy the policy to handle RPC errors.
   * @param backoff_policy the policy to control backoff after an error.
   */
  template <typename RPCRetryPolicy, typename RPCBackoffPolicy>
  InstanceAdmin(std::shared_ptr<InstanceAdminClient> client,
                RPCRetryPolicy retry_policy, RPCBackoffPolicy backoff_policy)
      : client_(std::move(client)),
        project_name_("projects/" + project_id()),
        rpc_retry_policy_(retry_policy.clone()),
        rpc_backoff_policy_(backoff_policy.clone()),
        polling_policy_(DefaultPollingPolicy()),
        metadata_update_policy_(project_name(), MetadataParamTypes::PARENT) {}

  /**
   * Create a new InstanceAdmin using explicit policies to handle RPC errors.
   *
   * @tparam RPCRetryPolicy control which operations to retry and for how long.
   * @tparam RPCBackoffPolicy control how does the client backs off after an RPC
   *     error.
   * @tparam Control polling for long running operations.
   * @param client the interface to create grpc stubs, report errors, etc.
   * @param retry_policy the policy to handle RPC errors.
   * @param backoff_policy the policy to control backoff after an error.
   * @param polling_policy the PollingPolicy instance.
   */
  template <typename RPCRetryPolicy, typename RPCBackoffPolicy,
            typename PollingPolicy>
  InstanceAdmin(std::shared_ptr<InstanceAdminClient> client,
                RPCRetryPolicy retry_policy, RPCBackoffPolicy backoff_policy,
                PollingPolicy polling_policy)
      : client_(std::move(client)),
        project_name_("projects/" + project_id()),
        rpc_retry_policy_(retry_policy.clone()),
        rpc_backoff_policy_(backoff_policy.clone()),
        polling_policy_(polling_policy.clone()),
        metadata_update_policy_(project_name(), MetadataParamTypes::PARENT) {}

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
