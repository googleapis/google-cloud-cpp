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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_FAKE_COMPLETION_QUEUE_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_FAKE_COMPLETION_QUEUE_IMPL_H

#include "google/cloud/future.h"
#include "google/cloud/internal/completion_queue_impl.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {

// Tests typically create an instance of this class, then create a
// `google::cloud::bigtable::CompletionQueue` to wrap it, keeping a reference to
// the instance to manipulate its state directly.
class FakeCompletionQueueImpl
    : public google::cloud::internal::CompletionQueueImpl {
 public:
  void Run() override;
  void Shutdown() override;
  void CancelAll() override;
  grpc::CompletionQueue& cq() override { return cq_; }

  future<StatusOr<std::chrono::system_clock::time_point>> MakeDeadlineTimer(
      std::chrono::system_clock::time_point deadline) override;
  future<StatusOr<std::chrono::system_clock::time_point>> MakeRelativeTimer(
      std::chrono::nanoseconds duration) override;
  void RunAsync(std::unique_ptr<internal::RunAsyncBase> function) override;

  void StartOperation(std::shared_ptr<internal::AsyncGrpcOperation> op,
                      absl::FunctionRef<void(void*)> start) override;

  void SimulateCompletion(bool ok);

  bool empty() const {
    std::unique_lock<std::mutex> lk(mu_);
    return pending_ops_.empty();
  }

  std::size_t size() const {
    std::unique_lock<std::mutex> lk(mu_);
    return pending_ops_.size();
  }

 private:
  grpc::CompletionQueue cq_;
  mutable std::mutex mu_;
  std::condition_variable cv_;
  bool shutdown_ = false;
  std::vector<std::shared_ptr<internal::AsyncGrpcOperation>> pending_ops_;
};

}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_FAKE_COMPLETION_QUEUE_IMPL_H
