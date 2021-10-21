// Copyright 2019 Google LLC
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

#include "google/cloud/bigtable/internal/async_bulk_apply.h"
#include "absl/memory/memory.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

future<std::vector<FailedMutation>> AsyncRetryBulkApply::Create(
    CompletionQueue cq, std::unique_ptr<RPCRetryPolicy> rpc_retry_policy,
    std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy,
    IdempotentMutationPolicy& idempotent_policy,
    MetadataUpdatePolicy metadata_update_policy,
    std::shared_ptr<bigtable::DataClient> client,
    std::string const& app_profile_id, std::string const& table_name,
    BulkMutation mut) {
  if (mut.empty()) return make_ready_future(std::vector<FailedMutation>{});

  std::shared_ptr<AsyncRetryBulkApply> bulk_apply(new AsyncRetryBulkApply(
      std::move(rpc_retry_policy), std::move(rpc_backoff_policy),
      idempotent_policy, std::move(metadata_update_policy), std::move(client),
      app_profile_id, table_name, std::move(mut)));
  bulk_apply->StartIteration(std::move(cq));
  return bulk_apply->promise_.get_future();
}

AsyncRetryBulkApply::AsyncRetryBulkApply(
    std::unique_ptr<RPCRetryPolicy> rpc_retry_policy,
    std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy,
    IdempotentMutationPolicy& idempotent_policy,
    MetadataUpdatePolicy metadata_update_policy,
    std::shared_ptr<bigtable::DataClient> client,
    std::string const& app_profile_id, std::string const& table_name,
    BulkMutation mut)
    : rpc_retry_policy_(std::move(rpc_retry_policy)),
      rpc_backoff_policy_(std::move(rpc_backoff_policy)),
      metadata_update_policy_(std::move(metadata_update_policy)),
      client_(std::move(client)),
      state_(app_profile_id, table_name, idempotent_policy, std::move(mut)) {}

void AsyncRetryBulkApply::StartIteration(CompletionQueue cq) {
  auto context = absl::make_unique<grpc::ClientContext>();
  rpc_retry_policy_->Setup(*context);
  rpc_backoff_policy_->Setup(*context);
  metadata_update_policy_.Setup(*context);
  auto client = client_;
  auto self = shared_from_this();
  cq.MakeStreamingReadRpc(
      [client](grpc::ClientContext* context,
               google::bigtable::v2::MutateRowsRequest const& request,
               grpc::CompletionQueue* cq) {
        return client->PrepareAsyncMutateRows(context, request, cq);
      },
      state_.BeforeStart(), std::move(context),
      [self, cq](google::bigtable::v2::MutateRowsResponse r) {
        self->OnRead(std::move(r));
        return make_ready_future(true);
      },
      [self, cq](Status const& s) { self->OnFinish(cq, s); });
}

void AsyncRetryBulkApply::OnRead(
    google::bigtable::v2::MutateRowsResponse response) {
  state_.OnRead(response);
}

void AsyncRetryBulkApply::OnFinish(CompletionQueue cq, Status const& status) {
  auto const is_retryable = status.ok() || rpc_retry_policy_->OnFailure(status);
  state_.OnFinish(status);
  if (!state_.HasPendingMutations() || !is_retryable) {
    SetPromise();
    return;
  }

  using TimerFuture = future<StatusOr<std::chrono::system_clock::time_point>>;

  auto self = this->shared_from_this();
  cq.MakeRelativeTimer(rpc_backoff_policy_->OnCompletion(status))
      .then([self, cq](TimerFuture result) {
        if (result.get()) {
          self->StartIteration(std::move(cq));
        } else {
          self->SetPromise();
        }
      });
}

void AsyncRetryBulkApply::SetPromise() {
  promise_.set_value(std::move(state_).OnRetryDone());
}

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
