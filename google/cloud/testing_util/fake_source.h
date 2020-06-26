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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_FAKE_SOURCE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_FAKE_SOURCE_H

#include "google/cloud/future.h"
#include "google/cloud/internal/source_ready_token.h"
#include "google/cloud/internal/throw_delegate.h"
#include "google/cloud/version.h"
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
  FakeSource(std::deque<T> values, E status, int max_outstanding)
      : flow_control_(max_outstanding),
        values_(std::move(values)),
        status_(std::move(status)) {}

  FakeSource(std::deque<T> values, E status)
      : FakeSource(std::move(values), std::move(status), 1) {}

  //@{
  using value_type = T;
  using error_type = E;

  future<internal::ReadyToken> ready() { return flow_control_.Acquire(); }

  future<absl::variant<T, E>> next(internal::ReadyToken token) {
    if (!flow_control_.Release(std::move(token))) {
      // We prefer to crash in this case. The program is buggy, there is little
      // point in returning an error.
      internal::ThrowLogicError("mismatched or invalid ReadyToken");
    }
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
  internal::ReadyTokenFlowControl flow_control_;
  std::deque<T> values_;
  E status_;
};

}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_FAKE_SOURCE_H
