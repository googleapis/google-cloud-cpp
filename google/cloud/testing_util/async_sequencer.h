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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_ASYNC_SEQUENCER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_ASYNC_SEQUENCER_H

#include "google/cloud/future.h"
#include "google/cloud/version.h"
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {

/**
 * A helper to sequence asynchronous operations.
 *
 * Mocks for asynchronous operations often need to create futures that the
 * test controls. The mock creates new futures by calling `PushBack()` and then
 * using `.then()` to convert the `future<void>` into the desired type. The main
 * test calls `.PopFront()` to satisfy the futures as needed.
 *
 * @par Example
 * @code
 * AsyncSequencer async;
 * EXPECT_CALL(mock, SomeFunction)
 *   .WillOnce([&] { return async.PushBack().then([](auto) { return 42; }); })
 *   .WillOnce([&] { return async.PushBack().then([](auto) { return 84; }); })
 *   .WillOnce([&] { return async.PushBack().then([](auto) { return 21; }); })
 *   ;
 *
 * auto f0 = mock->SomeFunction();
 * auto f1 = mock->SomeFunction();
 * auto f2 = mock->SomeFunction();
 *
 * auto p0 = async.PopFront();
 * auto p1 = async.PopFront();
 * auto p2 = async.PopFront();
 *
 * // Satisfy the futures out of order
 * p2.set_value();
 * EXPECT_EQ(f2.get(), 21);
 * p0.set_value();
 * EXPECT_EQ(f0.get(), 42);
 * p1.set_value();
 * EXPECT_EQ(f1.get(), 84);
 * @endcode
 */
template <typename T>
class AsyncSequencer {
 public:
  AsyncSequencer() = default;

  future<T> PushBack() { return PushBack("unnamed"); }
  promise<T> PopFront() { return std::move(PopFrontWithName().first); }

  future<T> PushBack(std::string name) {
    std::unique_lock<std::mutex> lk(mu_);
    queue_.emplace_back(promise<T>{}, std::move(name));
    auto f = queue_.back().first.get_future();
    max_size_ = (std::max)(max_size_, queue_.size());
    lk.unlock();
    cv_.notify_one();
    return f;
  }

  std::pair<promise<T>, std::string> PopFrontWithName() {
    std::unique_lock<std::mutex> lk(mu_);
    cv_.wait(lk, [&] { return !queue_.empty(); });
    auto p = std::move(queue_.front());
    queue_.pop_front();
    return p;
  }

  std::size_t MaxSize() const { return max_size_; }

 private:
  std::mutex mu_;
  std::condition_variable cv_;
  std::deque<std::pair<promise<T>, std::string>> queue_;
  std::size_t max_size_ = 0;
};

}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_ASYNC_SEQUENCER_H
