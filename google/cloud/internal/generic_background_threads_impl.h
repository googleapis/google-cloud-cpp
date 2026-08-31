// Copyright 2026 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_GENERIC_BACKGROUND_THREADS_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_GENERIC_BACKGROUND_THREADS_IMPL_H

#include "google/cloud/future.h"
#include "google/cloud/log.h"
#include "google/cloud/version.h"
#include <algorithm>
#include <system_error>
#include <thread>
#include <vector>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

template <typename QueueType>
struct DefaultQueueTraits {
  static QueueType Create() { return QueueType(); }
  static void Run(QueueType cq, promise<void>& started) {
    started.set_value();
    cq.Run();
  }
};

template <typename QueueType, typename BaseInterface,
          typename QueueTraits = DefaultQueueTraits<QueueType>>
class AutomaticallyCreatedBackgroundThreadsImpl : public BaseInterface {
 public:
  explicit AutomaticallyCreatedBackgroundThreadsImpl(
      std::size_t thread_count = 1U)
      : cq_(QueueTraits::Create()),
        pool_(thread_count == 0 ? 1 : thread_count) {
    std::generate_n(pool_.begin(), pool_.size(), [this] {
      promise<void> started;
      auto thread = std::thread(
          [](QueueType cq, promise<void>& started) {
            QueueTraits::Run(std::move(cq), started);
          },
          cq_, std::ref(started));
      started.get_future().wait();
      return thread;
    });
  }

  ~AutomaticallyCreatedBackgroundThreadsImpl() override { Shutdown(); }

  QueueType cq() const override { return cq_; }
  std::size_t pool_size() const { return pool_.size(); }

  void Shutdown() {
    cq_.Shutdown();
    for (auto& t : pool_) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
      try {
#endif
        if (t.joinable()) t.join();
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
      } catch (std::system_error const& e) {
        GCP_LOG(FATAL) << "Shutdown error: " << e.what();
      }
#endif
    }
    pool_.clear();
  }

 private:
  QueueType cq_;
  std::vector<std::thread> pool_;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_GENERIC_BACKGROUND_THREADS_IMPL_H
