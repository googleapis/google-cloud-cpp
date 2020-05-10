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
//
#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_MOCK_FAKE_CLOCK_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_MOCK_FAKE_CLOCK_H

#include <gmock/gmock.h>
#include <chrono>

namespace google {
namespace cloud {
namespace storage {
namespace testing {

/**
 * Represents a fake std::chrono::system_clock.
 *
 * When testing functionality that deals with time, it can be useful to
 * reset the clock to arbitrary timepoints.
 */
struct FakeClock : public std::chrono::system_clock {
 public:
  static std::time_t now_value_;

  // gmock doesn't easily allow copying mock objects, but we require this
  // struct to be copyable. So while the usual approach would be mocking this
  // method and defining its return value in each test, we instead override
  // this method and hard-code the return value for all instances.
  static std::chrono::system_clock::time_point now() {
    return std::chrono::system_clock::from_time_t(now_value_);
  }

  static void reset_clock(std::time_t fixed_time_stamp) {
    now_value_ = fixed_time_stamp;
  }
};

}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_MOCK_FAKE_CLOCK_H
