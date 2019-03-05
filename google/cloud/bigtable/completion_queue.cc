// Copyright 2018 Google LLC
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

#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/internal/throw_delegate.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace {
/**
 * Wrap a timer callback into an `AsyncOperation`.
 *
 * Applications (or more likely, other components in the client library) will
 * associate callbacks of many different types with a completion queue. This
 * class is created by the completion queue implementation to type-erase the
 * callbacks, and thus be able to treat them homogenously in the completion
 * queue. Note that this class lives in the `internal` namespace and thus is
 * not intended for general use.
 *
 * @tparam Functor the callback type.
 */
class AsyncTimerFuture : public internal::AsyncGrpcOperation {
 public:
  explicit AsyncTimerFuture(std::unique_ptr<grpc::Alarm> alarm)
      : alarm_(std::move(alarm)) {}

  future<AsyncTimerResult> GetFuture() { return promise_.get_future(); }

  void Set(grpc::CompletionQueue& cq,
           std::chrono::system_clock::time_point deadline, void* tag) {
    std::unique_lock<std::mutex> lk(mu_);
    timer_.deadline = deadline;
    if (alarm_) {
      alarm_->Set(&cq, deadline, tag);
    }
  }

  void Cancel() override {
    std::unique_lock<std::mutex> lk(mu_);
    if (alarm_) {
      alarm_->Cancel();
    }
  }

 private:
  bool Notify(CompletionQueue&, bool ok) override {
    std::unique_lock<std::mutex> lk(mu_);
    alarm_.reset();
    timer_.cancelled = !ok;
    lk.unlock();
    promise_.set_value(timer_);
    return true;
  }

  // It might not be clear why the mutex is needed.
  // We need to make sure that `if (alarm_) { alarm_->Cancel(); }` is atomic.
  // Without the mutex it is not because `Notify` resets `alarm_`.
  std::mutex mu_;
  promise<AsyncTimerResult> promise_;
  AsyncTimerResult timer_;
  std::unique_ptr<grpc::Alarm> alarm_;
};

}  // namespace

CompletionQueue::CompletionQueue() : impl_(new internal::CompletionQueueImpl) {}

void CompletionQueue::Run() { impl_->Run(*this); }

void CompletionQueue::Shutdown() { impl_->Shutdown(); }

google::cloud::future<AsyncTimerResult> CompletionQueue::MakeDeadlineTimer(
    std::chrono::system_clock::time_point deadline) {
  auto op = std::make_shared<AsyncTimerFuture>(impl_->CreateAlarm());
  void* tag = impl_->RegisterOperation(op);
  op->Set(impl_->cq(), deadline, tag);
  return op->GetFuture();
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
