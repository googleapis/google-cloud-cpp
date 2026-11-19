// Copyright 2019 Google LLC
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

#include "google/cloud/bigtable/internal/async_bulk_apply.h"
#include "google/cloud/bigtable/internal/async_streaming_read.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/grpc_opentelemetry.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/retry_loop_helpers.h"
#include <utility>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

Status MakeCancelledStatus() {
  return internal::CancelledError("AsyncBulkApply cancelled", GCP_ERROR_INFO());
}

}  // namespace

future<std::vector<bigtable::FailedMutation>> AsyncBulkApplier::Create(
    CompletionQueue cq, std::shared_ptr<BigtableStub> stub,
    std::shared_ptr<MutateRowsLimiter> limiter,
    std::unique_ptr<bigtable::DataRetryPolicy> retry_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy, bool enable_server_retries,
    bigtable::IdempotentMutationPolicy& idempotent_policy,
    std::string const& app_profile_id, std::string const& table_name,
    bigtable::BulkMutation mut,
    std::shared_ptr<OperationContext> operation_context) {
  if (mut.empty()) {
    return make_ready_future(std::vector<bigtable::FailedMutation>{});
  }

  std::shared_ptr<AsyncBulkApplier> bulk_apply(new AsyncBulkApplier(
      std::move(cq), std::move(stub), std::move(limiter),
      std::move(retry_policy), std::move(backoff_policy), enable_server_retries,
      idempotent_policy, app_profile_id, table_name, std::move(mut),
      std::move(operation_context)));
  return bulk_apply->Start();
}

AsyncBulkApplier::AsyncBulkApplier(
    CompletionQueue cq, std::shared_ptr<BigtableStub> stub,
    std::shared_ptr<MutateRowsLimiter> limiter,
    std::unique_ptr<bigtable::DataRetryPolicy> retry_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy, bool enable_server_retries,
    bigtable::IdempotentMutationPolicy& idempotent_policy,
    std::string const& app_profile_id, std::string const& table_name,
    bigtable::BulkMutation mut,
    std::shared_ptr<OperationContext> operation_context)
    : cq_(std::move(cq)),
      stub_(std::move(stub)),
      limiter_(std::move(limiter)),
      retry_policy_(std::move(retry_policy)),
      backoff_policy_(std::move(backoff_policy)),
      enable_server_retries_(enable_server_retries),
      state_(app_profile_id, table_name, idempotent_policy, std::move(mut)),
      options_(internal::SaveCurrentOptions()),
      call_context_(options_),
      operation_context_(std::move(operation_context)) {}

future<std::vector<bigtable::FailedMutation>> AsyncBulkApplier::Start() {
  std::weak_ptr<AsyncBulkApplier> weak(this->shared_from_this());
  promise_ = promise<std::vector<bigtable::FailedMutation>>([weak] {
    if (std::shared_ptr<AsyncBulkApplier> self = weak.lock()) {
      internal::ScopedCallContext scope(self->call_context_);
      self->Cancel();
    }
  });
  future<std::vector<bigtable::FailedMutation>> result = promise_.get_future();
  StartIteration();
  return result;
}

void AsyncBulkApplier::StartIteration() {
  std::uint_fast32_t operation;
  {
    std::unique_lock<std::mutex> lk(mu_);
    if (cancelled_) {
      lk.unlock();
      FinishCancelled();
      return;
    }
    operation = ++operation_;
  }
  std::shared_ptr<AsyncBulkApplier> self = this->shared_from_this();
  future<void> pending = limiter_->AsyncAcquire().then([self](future<void> f) {
    f.get();
    self->MakeRequest();
  });
  SetPending(operation, std::move(pending));
}

void AsyncBulkApplier::MakeRequest() {
  internal::ScopedCallContext scope(call_context_);
  auto client_context = std::make_shared<grpc::ClientContext>();
  {
    std::unique_lock<std::mutex> lk(mu_);
    if (cancelled_) {
      lk.unlock();
      FinishCancelled();
      return;
    }
    ++operation_;
    pending_operation_ = {};
    client_context_ = client_context;
    pre_call_invoked_ = true;
  }
  internal::ConfigureContext(*client_context, *call_context_.options);
  operation_context_->PreCall(*client_context);

  std::shared_ptr<AsyncBulkApplier> self = this->shared_from_this();
  PerformAsyncStreamingRead(
      stub_->AsyncMutateRows(cq_, std::move(client_context), options_,
                             state_.BeforeStart(), operation_context_),
      [self](google::bigtable::v2::MutateRowsResponse r) {
        self->OnRead(std::move(r));
        return make_ready_future(self->keep_reading_.load());
      },
      [self](Status const& s) { self->OnFinish(s); });
}

void AsyncBulkApplier::OnRead(
    google::bigtable::v2::MutateRowsResponse response) {
  limiter_->Update(response);
  state_.OnRead(std::move(response));
}

void AsyncBulkApplier::OnFinish(Status const& status) {
  internal::ScopedCallContext scope(call_context_);
  std::shared_ptr<grpc::ClientContext> client_context;
  bool cancelled = false;
  {
    std::lock_guard<std::mutex> lk(mu_);
    client_context = std::move(client_context_);
    cancelled = cancelled_;
  }
  operation_context_->PostCall(*client_context, status);
  Status const effective_status =
      cancelled && status.ok() ? MakeCancelledStatus() : status;
  state_.OnFinish(effective_status);

  if (!state_.HasPendingMutations() || cancelled) {
    operation_context_->OnDone(effective_status);
    SetPromise();
    return;
  }
  StatusOr<std::chrono::milliseconds> delay = internal::Backoff(
      status, "AsyncBulkApply", *retry_policy_, *backoff_policy_,
      Idempotency::kIdempotent, enable_server_retries_);
  if (!delay) {
    operation_context_->OnDone(status);
    SetPromise();
    return;
  }

  std::uint_fast32_t operation;
  {
    std::unique_lock<std::mutex> lk(mu_);
    if (cancelled_) {
      lk.unlock();
      Status cancel_status = MakeCancelledStatus();
      state_.OnCancel(cancel_status);
      operation_context_->OnDone(status.ok() ? cancel_status : status);
      SetPromise();
      return;
    }
    operation = ++operation_;
  }
  std::shared_ptr<AsyncBulkApplier> self = this->shared_from_this();
  future<void> pending =
      internal::TracedAsyncBackoff(cq_, *call_context_.options, *delay,
                                   "Async Backoff")
          .then([self](future<StatusOr<std::chrono::system_clock::time_point>>
                           result) { self->OnBackoff(result.get()); });
  SetPending(operation, std::move(pending));
}

void AsyncBulkApplier::OnBackoff(
    StatusOr<std::chrono::system_clock::time_point> result) {
  internal::ScopedCallContext scope(call_context_);
  bool cancelled = false;
  {
    std::lock_guard<std::mutex> lk(mu_);
    cancelled = cancelled_;
  }
  if (!result || cancelled) {
    Status final_status =
        result.ok() ? MakeCancelledStatus() : std::move(result).status();
    state_.OnCancel(final_status);
    operation_context_->OnDone(final_status);
    SetPromise();
    return;
  }
  StartIteration();
}

void AsyncBulkApplier::SetPending(std::uint_fast32_t operation,
                                  future<void> op) {
  std::unique_lock<std::mutex> lk(mu_);
  if (operation_ == operation) pending_operation_ = std::move(op);
  if (cancelled_) Cancel(std::move(lk));
}

void AsyncBulkApplier::Cancel() { Cancel(std::unique_lock<std::mutex>(mu_)); }

void AsyncBulkApplier::Cancel(std::unique_lock<std::mutex> lk) {
  cancelled_ = true;
  keep_reading_ = false;
  std::shared_ptr<grpc::ClientContext> client_context = client_context_;
  future<void> pending_operation = std::move(pending_operation_);
  lk.unlock();
  if (client_context) client_context->TryCancel();
  if (pending_operation.valid()) pending_operation.cancel();
}

void AsyncBulkApplier::FinishCancelled() {
  internal::ScopedCallContext scope(call_context_);
  bool pre_call_invoked = false;
  {
    std::lock_guard<std::mutex> lk(mu_);
    pre_call_invoked = pre_call_invoked_;
  }
  Status status = MakeCancelledStatus();
  state_.OnCancel(status);
  if (pre_call_invoked) {
    operation_context_->OnDone(status);
  }
  SetPromise();
}

void AsyncBulkApplier::SetPromise() {
  promise_.set_value(std::move(state_).OnRetryDone());
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
