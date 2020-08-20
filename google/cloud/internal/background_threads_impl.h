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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_BACKGROUND_THREADS_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_BACKGROUND_THREADS_IMPL_H

#include "google/cloud/background_threads.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/version.h"
#include <thread>
#include <vector>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

/// Assume the user has provided the background threads and use them.
class CustomerSuppliedBackgroundThreads : public BackgroundThreads {
 public:
  explicit CustomerSuppliedBackgroundThreads(CompletionQueue cq)
      : cq_(std::move(cq)) {}
  ~CustomerSuppliedBackgroundThreads() override = default;

  CompletionQueue cq() const override { return cq_; }

 private:
  CompletionQueue cq_;
};

/// Create a background thread to perform background operations.
class AutomaticallyCreatedBackgroundThreads : public BackgroundThreads {
 public:
  AutomaticallyCreatedBackgroundThreads();
  explicit AutomaticallyCreatedBackgroundThreads(std::size_t thread_count);
  ~AutomaticallyCreatedBackgroundThreads() override;

  CompletionQueue cq() const override { return cq_; }
  void Shutdown();
  std::size_t pool_size() const { return pool_.size(); }

 private:
  struct NormalizeTag {};
  AutomaticallyCreatedBackgroundThreads(std::size_t thread_count, NormalizeTag);

  CompletionQueue cq_;
  std::vector<std::thread> pool_;
};

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_BACKGROUND_THREADS_IMPL_H
