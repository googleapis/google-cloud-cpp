// Copyright 2021 Google LLC
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

#include "google/cloud/bigtable/internal/async_row_sampler.h"
#include "google/cloud/bigtable/internal/async_streaming_read.h"
#include "google/cloud/grpc_options.h"
#include <chrono>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

namespace v2 = ::google::bigtable::v2;

future<StatusOr<std::vector<bigtable::RowKeySample>>> AsyncRowSampler::Create(
    CompletionQueue cq, std::shared_ptr<bigtable_internal::BigtableStub> stub,
    std::unique_ptr<bigtable_internal::DataRetryPolicy> retry_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy,
    std::string const& app_profile_id, std::string const& table_name) {
  std::shared_ptr<AsyncRowSampler> sampler(new AsyncRowSampler(
      std::move(cq), std::move(stub), std::move(retry_policy),
      std::move(backoff_policy), app_profile_id, table_name));
  sampler->StartIteration();
  return sampler->promise_.get_future();
}

AsyncRowSampler::AsyncRowSampler(
    CompletionQueue cq, std::shared_ptr<bigtable_internal::BigtableStub> stub,
    std::unique_ptr<bigtable_internal::DataRetryPolicy> retry_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy,
    std::string const& app_profile_id, std::string const& table_name)
    : cq_(std::move(cq)),
      stub_(std::move(stub)),
      retry_policy_(std::move(retry_policy)),
      backoff_policy_(std::move(backoff_policy)),
      app_profile_id_(std::move(app_profile_id)),
      table_name_(std::move(table_name)),
      promise_([this] { keep_reading_ = false; }) {}

void AsyncRowSampler::StartIteration() {
  v2::SampleRowKeysRequest request;
  request.set_app_profile_id(app_profile_id_);
  request.set_table_name(table_name_);

  auto context = absl::make_unique<grpc::ClientContext>();
  internal::ConfigureContext(*context, internal::CurrentOptions());

  auto self = this->shared_from_this();
  PerformAsyncStreamingRead<v2::SampleRowKeysResponse>(
      stub_->AsyncSampleRowKeys(cq_, std::move(context), request),
      [self](v2::SampleRowKeysResponse response) {
        return self->OnRead(std::move(response));
      },
      [self](Status status) { self->OnFinish(std::move(status)); });
}

future<bool> AsyncRowSampler::OnRead(v2::SampleRowKeysResponse response) {
  bigtable::RowKeySample row_sample;
  row_sample.offset_bytes = response.offset_bytes();
  row_sample.row_key = std::move(*response.mutable_row_key());
  samples_.emplace_back(std::move(row_sample));
  return make_ready_future(keep_reading_.load());
}

void AsyncRowSampler::OnFinish(Status status) {
  if (status.ok()) {
    promise_.set_value(std::move(samples_));
    return;
  }
  if (!retry_policy_->OnFailure(status)) {
    promise_.set_value(std::move(status));
    return;
  }

  using TimerFuture = future<StatusOr<std::chrono::system_clock::time_point>>;

  samples_.clear();
  auto self = this->shared_from_this();
  auto delay = backoff_policy_->OnCompletion();
  cq_.MakeRelativeTimer(delay).then([self](TimerFuture result) {
    if (result.get()) {
      self->StartIteration();
    } else {
      self->promise_.set_value(
          Status(StatusCode::kCancelled, "call cancelled"));
    }
  });
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
