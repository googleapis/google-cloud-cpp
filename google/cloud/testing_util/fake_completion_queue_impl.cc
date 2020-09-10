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

 private:
  bool Notify(bool ok) override {
    if (!ok) {
      promise_.set_value(Status(StatusCode::kCancelled, "timer canceled"));
    } else {
      promise_.set_value(deadline_);
    }
    return true;
  }

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

future<StatusOr<std::chrono::system_clock::time_point>>
FakeCompletionQueueImpl::MakeDeadlineTimer(
    std::chrono::system_clock::time_point deadline) {
  auto op = std::make_shared<FakeAsyncTimer>(deadline);
  StartOperation(op, [](void*) {});
  return op->GetFuture();
}

void FakeCompletionQueueImpl::RunAsync(
    std::unique_ptr<internal::RunAsyncBase> function) {
  auto op = std::make_shared<FakeAsyncFunction>(std::move(function));
  StartOperation(op, [](void*) {});
}

}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
