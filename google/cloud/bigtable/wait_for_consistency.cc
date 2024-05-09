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

#include "google/cloud/bigtable/wait_for_consistency.h"
#include "google/cloud/bigtable/admin/bigtable_table_admin_options.h"
#include "google/cloud/bigtable/admin/internal/bigtable_table_admin_option_defaults.h"
#include "google/cloud/internal/make_status.h"
#include <chrono>

namespace google {
namespace cloud {
namespace bigtable_admin {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

// This class borrows heavily from `google::cloud::internal::AsyncRetryLoop`
class AsyncWaitForConsistencyImpl
    : public std::enable_shared_from_this<AsyncWaitForConsistencyImpl> {
 public:
  AsyncWaitForConsistencyImpl(CompletionQueue cq,
                              BigtableTableAdminClient client,
                              std::string table_name,
                              std::string consistency_token, Options options)
      : cq_(std::move(cq)),
        client_(std::move(client)),
        options_(bigtable_admin_internal::BigtableTableAdminDefaultOptions(
            std::move(options))),
        polling_policy_(options_.get<BigtableTableAdminPollingPolicyOption>()) {
    request_.set_name(std::move(table_name));
    request_.set_consistency_token(std::move(consistency_token));
  }

  future<Status> Start() {
    auto w = std::weak_ptr<AsyncWaitForConsistencyImpl>(shared_from_this());
    result_ = promise<Status>([w] {
      if (auto self = w.lock()) self->Cancel();
    });

    StartAttempt();
    return result_.get_future();
  }

 private:
  using RespType = StatusOr<bigtable::admin::v2::CheckConsistencyResponse>;
  using TimerResult = StatusOr<std::chrono::system_clock::time_point>;

  struct State {
    bool cancelled;
    std::uint_fast32_t operation;
  };

  State StartOperation() {
    std::unique_lock<std::mutex> lk(mu_);
    if (!cancelled_) return State{false, ++operation_};
    return SetDoneWithCancel(std::move(lk));
  }

  State OnOperation() {
    std::unique_lock<std::mutex> lk(mu_);
    if (!cancelled_) return State{false, operation_};
    return SetDoneWithCancel(std::move(lk));
  }

  void StartAttempt() {
    auto self = this->shared_from_this();
    auto state = StartOperation();
    if (state.cancelled) return;
    SetPending(
        state.operation,
        client_.AsyncCheckConsistency(request_, options_)
            .then([self](future<RespType> f) { self->OnAttempt(f.get()); }));
  }

  void StartBackoff() {
    auto self = this->shared_from_this();
    auto state = StartOperation();
    if (state.cancelled) return;
    SetPending(
        state.operation,
        cq_.MakeRelativeTimer(polling_policy_->WaitPeriod())
            .then([self](future<TimerResult> f) { self->OnBackoff(f.get()); }));
  }

  void OnAttempt(RespType result) {
    // A successful attempt, set the value and finish the loop.
    if (result.ok() && result->consistent()) {
      SetDone(Status());
      return;
    }
    auto status = std::move(result).status();
    if (!polling_policy_->OnFailure(status)) {
      if (!status.ok()) return SetDone(std::move(status));
      return SetDone(internal::DeadlineExceededError(
          "Polling loop terminated by polling policy", GCP_ERROR_INFO()));
    }
    StartBackoff();
  }

  void OnBackoff(TimerResult tp) {
    auto state = OnOperation();
    // Check for the retry loop cancellation first. We want to report that
    // status instead of the timer failure in that case.
    if (state.cancelled) return;
    if (!tp) {
      // Some kind of error in the CompletionQueue, probably shutting down.
      return SetDone(std::move(tp).status());
    }
    StartAttempt();
  }

  void SetPending(std::uint_fast32_t operation, future<void> op) {
    std::unique_lock<std::mutex> lk(mu_);
    if (operation_ == operation) pending_operation_ = std::move(op);
    if (cancelled_) return Cancel(std::move(lk));
  }

  void SetDone(Status value) {
    std::unique_lock<std::mutex> lk(mu_);
    if (done_) return;
    done_ = true;
    lk.unlock();
    result_.set_value(std::move(value));
  }

  State SetDoneWithCancel(std::unique_lock<std::mutex> lk) {
    if (done_) return State{true, 0};
    done_ = true;
    lk.unlock();
    result_.set_value(
        internal::CancelledError("Operation cancelled", GCP_ERROR_INFO()));
    return State{true, 0};
  }

  void Cancel() { return Cancel(std::unique_lock<std::mutex>(mu_)); }

  void Cancel(std::unique_lock<std::mutex> lk) {
    cancelled_ = true;
    future<void> f = std::move(pending_operation_);
    lk.unlock();
    f.cancel();
  }

  CompletionQueue cq_;
  bigtable::admin::v2::CheckConsistencyRequest request_;
  BigtableTableAdminClient client_;
  Options options_;
  std::shared_ptr<PollingPolicy> polling_policy_;
  promise<Status> result_;

  // Only the following variables require synchronization, as they coordinate
  // the work between the retry loop (which would be lock-free) and the cancel
  // requests (which need locks).
  std::mutex mu_;
  bool cancelled_ = false;
  bool done_ = false;
  std::uint_fast32_t operation_ = 0;
  future<void> pending_operation_;
};

}  // namespace

future<Status> AsyncWaitForConsistency(CompletionQueue cq,
                                       BigtableTableAdminClient client,
                                       std::string table_name,
                                       std::string consistency_token,
                                       Options options) {
  auto loop = std::make_shared<AsyncWaitForConsistencyImpl>(
      std::move(cq), std::move(client), std::move(table_name),
      std::move(consistency_token), std::move(options));
  return loop->Start();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_admin
}  // namespace cloud
}  // namespace google
