// Copyright 2021 Google LLC
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

#include "google/cloud/bigtable/admin/wait_for_consistency.h"
#include "google/cloud/bigtable/admin/bigtable_table_admin_client.h"
#include "google/cloud/bigtable/admin/bigtable_table_admin_options.h"
#include "google/cloud/bigtable/admin/internal/bigtable_table_admin_option_defaults.h"
#include <chrono>

namespace google {
namespace cloud {
namespace bigtable_admin {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using RespType = StatusOr<bigtable::admin::v2::CheckConsistencyResponse>;
using TimerFuture = future<StatusOr<std::chrono::system_clock::time_point>>;

// This class borrows heavily from `google::cloud::internal::AsyncRetryLoop`
class AsyncWaitForConsistencyImpl
    : public std::enable_shared_from_this<AsyncWaitForConsistencyImpl> {
 public:
  AsyncWaitForConsistencyImpl(CompletionQueue cq,
                              BigtableTableAdminClient client,
                              std::string table_name,
                              std::string consistency_token,
                              Options const& options)
      : cq_(std::move(cq)),
        client_(std::move(client)),
        table_name_(std::move(table_name)),
        consistency_token_(std::move(consistency_token)),
        options_(bigtable_admin_internal::BigtableTableAdminDefaultOptions(
            std::move(options))),
        polling_policy_(options_.get<BigtableTableAdminPollingPolicyOption>()) {
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
  enum State {
    kIdle,
    kWaiting,
    kDone,
  };

  void StartAttempt() {
    auto self = this->shared_from_this();
    auto op =
        client_.AsyncCheckConsistency(table_name_, consistency_token_, options_)
            .then([self](future<RespType> f) { self->OnAttempt(f.get()); });
    SetWaiting(std::move(op));
  }

  void OnAttempt(RespType result) {
    SetIdle();
    // A successful attempt, set the value and finish the loop.
    if (result.ok() && result->consistent()) {
      SetDone(Status());
      return;
    }
    auto status = std::move(result).status();
    if (!polling_policy_->OnFailure(status)) {
      if (status.ok()) {
        SetDone(Status(StatusCode::kDeadlineExceeded,
                       "Polling loop terminated by polling policy"));
      } else {
        SetDone(std::move(status));
      }
      return;
    }
    if (Cancelled()) return;
    auto self = this->shared_from_this();
    auto op =
        cq_.MakeRelativeTimer(polling_policy_->WaitPeriod())
            .then([self](TimerFuture f) { self->OnBackoffTimer(f.get()); });
    SetWaiting(std::move(op));
  }

  void OnBackoffTimer(StatusOr<std::chrono::system_clock::time_point> tp) {
    SetIdle();
    if (Cancelled()) return;
    if (!tp) {
      // Some kind of error in the CompletionQueue, probably shutting down.
      SetDone(std::move(tp).status());
      return;
    }
    StartAttempt();
  }

  void SetIdle() {
    std::unique_lock<std::mutex> lk(mu_);
    switch (state_) {
      case kIdle:
      case kDone:
        break;
      case kWaiting:
        state_ = kIdle;
        break;
    }
  }

  void SetWaiting(future<void> op) {
    std::unique_lock<std::mutex> lk(mu_);
    if (state_ != kIdle) return;
    state_ = kWaiting;
    pending_operation_ = std::move(op);
  }

  void SetDone(Status value) {
    std::unique_lock<std::mutex> lk(mu_);
    if (state_ == kDone) return;
    state_ = kDone;
    lk.unlock();
    result_.set_value(std::move(value));
  }

  void Cancel() {
    std::unique_lock<std::mutex> lk(mu_);
    cancelled_ = true;
    if (state_ != kWaiting) return;
    future<void> f = std::move(pending_operation_);
    state_ = kIdle;
    lk.unlock();
    f.cancel();
  }

  bool Cancelled() {
    std::unique_lock<std::mutex> lk(mu_);
    if (!cancelled_) return false;
    state_ = kDone;
    lk.unlock();
    result_.set_value(Status(StatusCode::kCancelled, "Operation cancelled"));
    return true;
  }

  CompletionQueue cq_;
  BigtableTableAdminClient client_;
  std::string table_name_;
  std::string consistency_token_;
  Options options_;
  std::shared_ptr<PollingPolicy> polling_policy_;
  promise<Status> result_;
  std::mutex mu_;
  State state_ = kIdle;
  bool cancelled_ = false;
  future<void> pending_operation_;
};

}  // namespace

future<Status> AsyncWaitForConsistency(CompletionQueue cq,
                                       BigtableTableAdminClient client,
                                       std::string table_name,
                                       std::string consistency_token,
                                       Options const& options) {
  auto loop = std::make_shared<AsyncWaitForConsistencyImpl>(
      std::move(cq), std::move(client), std::move(table_name),
      std::move(consistency_token), options);
  return loop->Start();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_admin
}  // namespace cloud
}  // namespace google
