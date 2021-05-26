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

#include "google/cloud/internal/background_threads_impl.h"
#include <algorithm>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

AutomaticallyCreatedBackgroundThreads::AutomaticallyCreatedBackgroundThreads(
    std::size_t thread_count)
    : pool_(thread_count == 0 ? 1 : thread_count) {
  std::generate_n(pool_.begin(), pool_.size(), [this] {
    return std::thread([](CompletionQueue cq) { cq.Run(); }, cq_);
  });
}

AutomaticallyCreatedBackgroundThreads::
    ~AutomaticallyCreatedBackgroundThreads() {
  Shutdown();
}

void AutomaticallyCreatedBackgroundThreads::Shutdown() {
  cq_.Shutdown();
  for (auto& t : pool_) t.join();
  pool_.clear();
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
