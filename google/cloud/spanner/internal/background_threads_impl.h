// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_BACKGROUND_THREADS_IMPL_H_
#define GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_BACKGROUND_THREADS_IMPL_H_

#include "google/cloud/spanner/background_threads.h"
#include "google/cloud/grpc_utils/completion_queue.h"
#include <thread>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {

/// Assume the user has provided the background threads and use them.
class CustomerSuppliedBackgroundThreads : public BackgroundThreads {
 public:
  explicit CustomerSuppliedBackgroundThreads(grpc_utils::CompletionQueue cq)
      : cq_(std::move(cq)) {}
  ~CustomerSuppliedBackgroundThreads() override = default;

  grpc_utils::CompletionQueue cq() const override { return cq_; }

 private:
  grpc_utils::CompletionQueue cq_;
};

/// Create a background thread to perform background operations.
class AutomaticallyCreatedBackgroundThreads : public BackgroundThreads {
 public:
  AutomaticallyCreatedBackgroundThreads();
  ~AutomaticallyCreatedBackgroundThreads() override;

  grpc_utils::CompletionQueue cq() const override { return cq_; }
  void Shutdown();

 private:
  grpc_utils::CompletionQueue cq_;
  std::thread runner_;
};

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_BACKGROUND_THREADS_IMPL_H_
