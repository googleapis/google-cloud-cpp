// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/testing_util/fake_completion_queue_impl.h"

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {
namespace {
class FakeAsyncTimer : public internal::AsyncGrpcOperation {
 public:
  explicit FakeAsyncTimer(std::chrono::system_clock::time_point deadline)
      : deadline_(deadline) {}

  future<StatusOr<std::chrono::system_clock::time_point>> GetFuture() {
    return promise_.get_future();
  }

  void Cancel() override {}

  bool Notify(bool ok) override {
    if (!ok) {
      promise_.set_value(Status(StatusCode::kCancelled, "timer canceled"));
    } else {
      promise_.set_value(deadline_);
    }
    return true;
  }

 private:
  std::chrono::system_clock::time_point const deadline_;
  promise<StatusOr<std::chrono::system_clock::time_point>> promise_;
};

class FakeAsyncFunction : public internal::AsyncGrpcOperation {
 public:
  explicit FakeAsyncFunction(std::unique_ptr<internal::RunAsyncBase> fun)
      : function_(std::move(fun)) {}

  void Cancel() override {}

 private:
  bool Notify(bool ok) override {
    auto f = std::move(function_);
    if (!ok) return true;
    f->exec();
    return true;
  }

  std::unique_ptr<internal::RunAsyncBase> function_;
};

}  // namespace

void FakeCompletionQueueImpl::Run() {
  std::unique_lock<std::mutex> lk(mu_);
  cv_.wait(lk, [&] { return shutdown_ && pending_ops_.empty(); });
}

void FakeCompletionQueueImpl::Shutdown() {
  std::unique_lock<std::mutex> lk(mu_);
  shutdown_ = true;
  while (!pending_ops_.empty()) {
    auto op = std::move(pending_ops_.back());
    pending_ops_.pop_back();
    lk.unlock();
    op->Notify(false);
    lk.lock();
  }
  cv_.notify_all();
}

void FakeCompletionQueueImpl::CancelAll() {
  auto ops = [this] {
    std::unique_lock<std::mutex> lk(mu_);
    return pending_ops_;
  }();
  for (auto& op : ops) op->Cancel();
}

future<StatusOr<std::chrono::system_clock::time_point>>
FakeCompletionQueueImpl::MakeDeadlineTimer(
    std::chrono::system_clock::time_point deadline) {
  auto op = std::make_shared<FakeAsyncTimer>(deadline);
  std::unique_lock<std::mutex> lk(mu_);
  if (shutdown_) {
    lk.unlock();
    op->Notify(/*ok=*/false);
    return op->GetFuture();
  }
  pending_ops_.push_back(op);
  return op->GetFuture();
}

future<StatusOr<std::chrono::system_clock::time_point>>
FakeCompletionQueueImpl::MakeRelativeTimer(std::chrono::nanoseconds duration) {
  using std::chrono::system_clock;
  auto const d = std::chrono::duration_cast<system_clock::duration>(duration);
  return MakeDeadlineTimer(system_clock::now() + d);
}

void FakeCompletionQueueImpl::RunAsync(
    std::unique_ptr<internal::RunAsyncBase> function) {
  auto op = std::make_shared<FakeAsyncFunction>(std::move(function));
  std::unique_lock<std::mutex> lk(mu_);
  if (shutdown_) {
    return;
  }
  pending_ops_.push_back(op);
}

void FakeCompletionQueueImpl::StartOperation(
    std::shared_ptr<internal::AsyncGrpcOperation> op,
    absl::FunctionRef<void(void*)> start) {
  std::unique_lock<std::mutex> lk(mu_);
  if (shutdown_) {
    lk.unlock();
    op->Notify(/*ok=*/false);
    return;
  }
  pending_ops_.push_back(op);
  start(op.get());
}

void FakeCompletionQueueImpl::SimulateCompletion(bool ok) {
  auto ops = [this] {
    std::unique_lock<std::mutex> lk(mu_);
    std::vector<std::shared_ptr<internal::AsyncGrpcOperation>> ops;
    ops.swap(pending_ops_);
    return ops;
  }();

  for (auto& op : ops) {
    op->Notify(ok);
  }
}

}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
