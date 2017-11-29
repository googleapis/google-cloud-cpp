// Copyright 2017 Google Inc.
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

#include "bigtable/client/data.h"

#include <cstdlib>
#include <iostream>
#include <thread>

#include <absl/memory/memory.h>

namespace btproto = ::google::bigtable::v2;

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {

void Table::Apply(SingleRowMutation&& mut) {
  // This is the RPC Retry Policy in effect for the complete operation ...
  auto rpc_policy = rpc_retry_policy_->clone();

  // ... build the RPC request, try to minimize copying ...
  btproto::MutateRowRequest request;
  request.set_table_name(table_name_);
  request.set_row_key(std::move(mut.row_key_));
  request.mutable_mutations()->Swap(&mut.ops_);

  btproto::MutateRowResponse response;
  while (true) {
    // TODO(coryan) - the RPCRetryPolicy should configure the context.
    // TODO(coryan) - consider renaming the policy to RPCControlPolicy.
    grpc::ClientContext client_context;
    grpc::Status status =
        client_->Stub().MutateRow(&client_context, request, &response);
    if (status.ok()) {
      return;
    }
    // ... it is up to the policy to terminate this loop, it could run
    // forever, but that would be a bad policy (pun intended) ...
    std::chrono::milliseconds delay;
    if (not rpc_policy->on_failure(status, &delay)) {
      throw std::runtime_error("retry policy exhausted or permanent error");
    }
    std::this_thread::sleep_for(delay);
  }
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
