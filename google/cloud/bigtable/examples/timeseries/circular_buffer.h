// Copyright 2018 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EXAMPLES_TIMESERIES_CIRCULAR_BUFFER_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EXAMPLES_TIMESERIES_CIRCULAR_BUFFER_H_

#include "google/cloud/version.h"
#include <condition_variable>
#include <mutex>
#include <vector>

namespace bigtable_demos {
template <typename T>
class CircularBuffer {
 public:
  explicit CircularBuffer(std::size_t size)
      : buffer_(size), head_(0), tail_(0), empty_(true), is_shutdown_(false) {}

  void Shutdown() {
    std::unique_lock<std::mutex> lk(mu_);
    is_shutdown_ = true;
    lk.unlock();
    cv_.notify_all();
  }

  bool Pop(T& next) {
    std::unique_lock<std::mutex> lk(mu_);
    cv_.wait(lk, [this]() { return not IsEmpty() or is_shutdown_; });
    if (not IsEmpty()) {
      next = std::move(buffer_[head_]);
      ++head_;
      if (head_ >= buffer_.size()) {
        head_ = 0;
      }
      empty_ = head_ == tail_;
      lk.unlock();
      cv_.notify_all();
      return true;
    }
    return not is_shutdown_;
  }

  void Push(T data) {
    std::unique_lock<std::mutex> lk(mu_);
    cv_.wait(lk, [this]() { return not IsFull(); });
    buffer_[tail_] = std::move(data);
    ++tail_;
    if (tail_ >= buffer_.size()) {
      tail_ = 0;
    }
    empty_ = false;
    lk.unlock();
    cv_.notify_all();
  }

 private:
  bool IsEmpty() const { return head_ == tail_ and empty_; }
  bool IsFull() const { return head_ == tail_ and not empty_; }

 private:
  std::mutex mu_;
  std::condition_variable cv_;
  std::vector<T> buffer_;
  std::size_t head_;
  std::size_t tail_;
  bool empty_;
  bool is_shutdown_;
};
}  // namespace bigtable_demos

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EXAMPLES_TIMESERIES_CIRCULAR_BUFFER_H_
