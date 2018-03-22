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

#include "bigtable/client/internal/instance_admin.h"

namespace btproto = ::google::bigtable::admin::v2;

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace noex {

std::vector<btproto::Instance> InstanceAdmin::ListInstances(
    grpc::Status& status) {
  // Copy the policies in effect for the operation.
  auto rpc_policy = rpc_retry_policy_->clone();
  auto backoff_policy = rpc_backoff_policy_->clone();

  std::string error = "InstanceAdmin::ListInstances(" + project_id() + ")";

  // Build the RPC request, try to minimize copying.
  std::vector<btproto::Instance> result;
  std::string page_token;
  do {
    btproto::ListInstancesRequest request;
    request.set_page_token(std::move(page_token));
    request.set_parent(project_name_);

    auto response = RpcUtils::CallWithRetryBorrow(
        *client_, *rpc_policy, *backoff_policy, metadata_update_policy_,
        &StubType::ListInstances, request, error.c_str(), status);
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

}  // namespace noex
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
