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

#ifndef GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_INTERNAL_INSTANCE_ADMIN_H_
#define GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_INTERNAL_INSTANCE_ADMIN_H_

#include "bigtable/client/instance_admin_client.h"
#include "bigtable/client/internal/unary_rpc_utils.h"
#include <memory>

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
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
        metadata_update_policy_(project_name(), MetadataParamTypes::PARENT) {}

  /// The full name (`projects/<project_id>`) of the project.
  std::string const& project_name() const { return project_name_; }
  /// The project id, i.e., `project_name()` without the `projects/` prefix.
  std::string const& project_id() const { return client_->project(); }

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

  //@}

  InstanceAdmin(InstanceAdmin const&) = delete;
  InstanceAdmin operator=(InstanceAdmin const&) = delete;

 private:
  /// Shortcuts to avoid typing long names over and over.
  using RpcUtils = bigtable::internal::noex::UnaryRpcUtils<InstanceAdminClient>;
  using StubType = RpcUtils::StubType;

 private:
  std::shared_ptr<InstanceAdminClient> client_;
  std::string project_name_;
  std::unique_ptr<RPCRetryPolicy> rpc_retry_policy_;
  std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy_;
  MetadataUpdatePolicy metadata_update_policy_;
};

}  // namespace noex
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_INTERNAL_INSTANCE_ADMIN_H_
