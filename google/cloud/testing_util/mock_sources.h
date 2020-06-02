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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_MOCK_SOURCES_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_MOCK_SOURCES_H

#include "google/cloud/future.h"
#include "absl/types/variant.h"
#include <chrono>
#include <deque>
#include <thread>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {

/**
 * A type meeting the requirements for source<T, E> used in testing.
 *
 * @tparam T the value type for the source
 * @tparam E the error type for the source
 */
template <class T, typename E>
class FakeSource {
 public:
  FakeSource(std::deque<T> values, E status)
      : values_(std::move(values)), status_(std::move(status)) {}

  //@{
  using value_type = T;
  using error_type = E;

  future<absl::variant<T, E>> next() {
    using event_t = absl::variant<T, E>;

    promise<event_t> p;
    auto f = p.get_future();
    // Create a thread for each call, this is not how production sources would
    // work, but it is good enough for a test.
    (void)std::async(
        std::launch::async,
        [this](promise<event_t> p) {
          std::this_thread::sleep_for(std::chrono::microseconds(100));
          if (values_.empty()) {
            p.set_value(std::move(status_));
            return;
          }
          auto& front = values_.front();
          p.set_value(std::move(front));
          values_.pop_front();
        },
        std::move(p));
    return f;
  }
  //@}

 private:
  std::deque<T> values_;
  E status_;
};

}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_MOCK_SOURCES_H
