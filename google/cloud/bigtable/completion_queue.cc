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
 * Wrap a gRPC timer into an `AsyncOperation`.
 *
 * Applications (or more likely, other components in the client library) will
 * associate timers with a completion queue. gRPC timers require applications to
 * create a unique `grpc::Alarm` object for each timer, and then to associate
 * them with the completion queue using a `void*` tag.
 *
 * This class collaborates with our wrapper for `CompletionQueue` to associate
 * a `future<AsyncTimerResult>` for each timer. This class takes care of
 * allocating the `grpc::Alarm`, creating a unique `void*` associated with the
 * timer, and satisfying the future when the timer expires.
 *
 * Note that this class is an implementation detail, hidden from the application
 * developers.
 */
class AsyncTimerFuture : public internal::AsyncGrpcOperation {
 public:
  AsyncTimerFuture(std::unique_ptr<grpc::Alarm> alarm)
      : alarm_(std::move(alarm)) {}

  future<std::chrono::system_clock::time_point> GetFuture() {
    return promise_.get_future();
  }

  void Set(grpc::CompletionQueue& cq,
           std::chrono::system_clock::time_point deadline, void* tag) {
    deadline_ = deadline;

    if (alarm_) {
      alarm_->Set(&cq, deadline, tag);
    }
  }

  void Cancel() override {
    if (alarm_) {
      alarm_->Cancel();
    }
  }

 private:
  bool Notify(CompletionQueue&, bool) override {
    promise_.set_value(deadline_);
    return true;
  }

  promise<std::chrono::system_clock::time_point> promise_;
  std::chrono::system_clock::time_point deadline_;
  /// Holds the underlying handle. It might be a nullptr in tests.
  std::unique_ptr<grpc::Alarm> alarm_;
};

}  // namespace

CompletionQueue::CompletionQueue() : impl_(new internal::CompletionQueueImpl) {}

void CompletionQueue::Run() { impl_->Run(*this); }

void CompletionQueue::Shutdown() { impl_->Shutdown(); }

google::cloud::future<std::chrono::system_clock::time_point>
CompletionQueue::MakeDeadlineTimer(
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
