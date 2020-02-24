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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BENCHMARKS_BOUNDED_QUEUE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BENCHMARKS_BOUNDED_QUEUE_H

#include "google/cloud/optional.h"
#include <condition_variable>
#include <deque>
#include <mutex>

namespace google {
namespace cloud {
namespace storage_benchmarks {

template <typename T>
class BoundedQueue {
 public:
  BoundedQueue() : BoundedQueue(512, 1024) {}
  explicit BoundedQueue(std::size_t lwm, std::size_t hwm)
      : lwm_(lwm), hwm_(hwm), is_shutdown_(false) {}

  void Shutdown() {
    std::unique_lock<std::mutex> lk(mu_);
    is_shutdown_ = true;
    lk.unlock();
    cv_read_.notify_all();
    cv_write_.notify_all();
  }

  google::cloud::optional<T> Pop() {
    std::unique_lock<std::mutex> lk(mu_);
    cv_read_.wait(lk, [this]() { return is_shutdown_ || !empty(); });
    if (empty()) return {};
    auto next = std::move(buffer_.front());
    buffer_.pop_front();
    if (below_lwm()) {
      cv_write_.notify_all();
    }
    return next;
  }

  void Push(T data) {
    std::unique_lock<std::mutex> lk(mu_);
    cv_write_.wait(lk, [this]() { return is_shutdown_ || below_hwm(); });
    if (is_shutdown_) return;
    buffer_.push_back(std::move(data));
    cv_read_.notify_all();
  }

 private:
  bool empty() const { return buffer_.empty(); }
  bool below_hwm() const { return buffer_.size() <= hwm_; }
  bool below_lwm() const { return buffer_.size() <= lwm_; }

  std::size_t const lwm_;
  std::size_t const hwm_;
  std::mutex mu_;
  std::condition_variable cv_read_;
  std::condition_variable cv_write_;
  std::deque<T> buffer_;
  bool is_shutdown_ = false;
};

}  // namespace storage_benchmarks
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BENCHMARKS_BOUNDED_QUEUE_H
