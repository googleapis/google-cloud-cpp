// Copyright 2020 Google LLC
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

#include "google/cloud/pubsub/internal/session_shutdown_manager.h"
#include "google/cloud/log.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

SessionShutdownManager::~SessionShutdownManager() {
  if (signaled_) return;
  GCP_LOG(DEBUG) << __func__ << "() - do signal"
                 << ", shutdown=" << shutdown_ << ", signaled=" << signaled_
                 << ", outstanding_operations=" << outstanding_operations_
                 << ", result=" << result_;
  signaled_ = true;
  done_.set_value(std::move(result_));
}

void SessionShutdownManager::LogStart(const char* caller, const char* name) {
  GCP_LOG(DEBUG) << "operation <" << name << "> starting from " << caller
                 << ", shutdown=" << shutdown_ << ", signaled=" << signaled_
                 << ", outstanding_operations=" << outstanding_operations_
                 << ", result=" << result_;
}

bool SessionShutdownManager::FinishedOperation(char const* name) {
  std::unique_lock<std::mutex> lk(mu_);
  GCP_LOG(DEBUG) << "operation <" << name << "> finished"
                 << ", shutdown=" << shutdown_ << ", signaled=" << signaled_
                 << ", outstanding_operations=" << outstanding_operations_
                 << ", result=" << result_;
  bool r = shutdown_;
  --outstanding_operations_;
  SignalOnShutdown(std::move(lk));
  return r;
}

void SessionShutdownManager::MarkAsShutdown(char const* caller, Status status) {
  std::unique_lock<std::mutex> lk(mu_);
  GCP_LOG(DEBUG) << __func__ << "() - from " << caller << "() - shutting down"
                 << ", shutdown=" << shutdown_ << ", signaled=" << signaled_
                 << ", outstanding_operations=" << outstanding_operations_
                 << ", result=" << result_;
  shutdown_ = true;
  result_ = std::move(status);
  SignalOnShutdown(std::move(lk));
}

void SessionShutdownManager::SignalOnShutdown(std::unique_lock<std::mutex> lk) {
  GCP_LOG(DEBUG) << __func__ << "() - maybe signal"
                 << ", shutdown=" << shutdown_ << ", signaled=" << signaled_
                 << ", outstanding_operations=" << outstanding_operations_
                 << ", result=" << result_;
  if (outstanding_operations_ > 0 || !shutdown_ || signaled_) return;
  // No other thread will go beyond this point, as `signaled_` is only set
  // once.
  signaled_ = true;
  // As satisfying the `done_` promise might trigger callbacks we should release
  // the lock before doing so. But we also need to modify any variables with
  // the lock held:
  auto p = std::move(done_);
  auto s = std::move(result_);
  lk.unlock();
  p.set_value(std::move(s));
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
