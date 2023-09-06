// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/internal/legacy_async_row_sampler.h"
#include "google/cloud/grpc_error_delegate.h"
#include <grpcpp/client_context.h>
#include <grpcpp/completion_queue.h>
#include <chrono>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

namespace btproto = ::google::bigtable::v2;

future<StatusOr<std::vector<RowKeySample>>> LegacyAsyncRowSampler::Create(
    CompletionQueue cq, std::shared_ptr<DataClient> client,
    std::unique_ptr<RPCRetryPolicy> rpc_retry_policy,
    std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy,
    MetadataUpdatePolicy metadata_update_policy, std::string app_profile_id,
    std::string table_name) {
  std::shared_ptr<LegacyAsyncRowSampler> sampler(new LegacyAsyncRowSampler(
      std::move(cq), std::move(client), std::move(rpc_retry_policy),
      std::move(rpc_backoff_policy), std::move(metadata_update_policy),
      std::move(app_profile_id), std::move(table_name)));
  sampler->StartIteration();
  return sampler->promise_.get_future();
}

LegacyAsyncRowSampler::LegacyAsyncRowSampler(
    CompletionQueue cq, std::shared_ptr<DataClient> client,
    std::unique_ptr<RPCRetryPolicy> rpc_retry_policy,
    std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy,
    MetadataUpdatePolicy metadata_update_policy, std::string app_profile_id,
    std::string table_name)
    : cq_(std::move(cq)),
      client_(std::move(client)),
      rpc_retry_policy_(std::move(rpc_retry_policy)),
      rpc_backoff_policy_(std::move(rpc_backoff_policy)),
      metadata_update_policy_(std::move(metadata_update_policy)),
      app_profile_id_(std::move(app_profile_id)),
      table_name_(std::move(table_name)),
      promise_([this] { keep_reading_ = false; }) {}

void LegacyAsyncRowSampler::StartIteration() {
  btproto::SampleRowKeysRequest request;
  request.set_app_profile_id(app_profile_id_);
  request.set_table_name(table_name_);

  auto context = std::make_unique<grpc::ClientContext>();
  rpc_retry_policy_->Setup(*context);
  rpc_backoff_policy_->Setup(*context);
  metadata_update_policy_.Setup(*context);

  auto& client = client_;
  auto self = this->shared_from_this();
  cq_.MakeStreamingReadRpc(
      [client](grpc::ClientContext* context,
               btproto::SampleRowKeysRequest const& request,
               grpc::CompletionQueue* cq) {
        return client->PrepareAsyncSampleRowKeys(context, request, cq);
      },
      request, std::move(context),
      [self](btproto::SampleRowKeysResponse response) {
        return self->OnRead(std::move(response));
      },
      [self](Status const& status) { self->OnFinish(status); });
}

future<bool> LegacyAsyncRowSampler::OnRead(
    btproto::SampleRowKeysResponse response) {
  RowKeySample row_sample;
  row_sample.offset_bytes = response.offset_bytes();
  row_sample.row_key = std::move(*response.mutable_row_key());
  samples_.emplace_back(std::move(row_sample));
  return make_ready_future(keep_reading_.load());
}

void LegacyAsyncRowSampler::OnFinish(Status const& status) {
  if (status.ok()) {
    promise_.set_value(std::move(samples_));
    return;
  }
  if (!rpc_retry_policy_->OnFailure(status)) {
    promise_.set_value(status);
    return;
  }

  using TimerFuture = future<StatusOr<std::chrono::system_clock::time_point>>;

  samples_.clear();
  auto self = this->shared_from_this();
  auto delay = rpc_backoff_policy_->OnCompletion(std::move(status));
  cq_.MakeRelativeTimer(delay).then([self](TimerFuture result) {
    if (result.get()) {
      self->StartIteration();
    } else {
      self->promise_.set_value(
          Status(StatusCode::kCancelled, "call cancelled"));
    }
  });
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
