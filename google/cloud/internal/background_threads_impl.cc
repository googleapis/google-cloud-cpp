// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/future.h"
#include "google/cloud/log.h"
#include <algorithm>
#include <system_error>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

AutomaticallyCreatedBackgroundThreads::AutomaticallyCreatedBackgroundThreads(
    std::size_t thread_count)
    : pool_(thread_count == 0 ? 1 : thread_count) {
  std::generate_n(pool_.begin(), pool_.size(), [this] {
    promise<void> started;
    auto thread = std::thread(
        [](CompletionQueue cq, promise<void>& started) {
          started.set_value();
          cq.Run();
        },
        cq_, std::ref(started));
    started.get_future().wait();
    return thread;
  });
}

AutomaticallyCreatedBackgroundThreads::
    ~AutomaticallyCreatedBackgroundThreads() {
  Shutdown();
}

void AutomaticallyCreatedBackgroundThreads::Shutdown() {
  cq_.Shutdown();
  for (auto& t : pool_) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
    try {
#endif
      t.join();
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
    } catch (std::system_error const& e) {
      GCP_LOG(FATAL) << "AutomaticallyCreatedBackgroundThreads::Shutdown: "
                     << e.what();
    }
#endif
  }
  pool_.clear();
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
