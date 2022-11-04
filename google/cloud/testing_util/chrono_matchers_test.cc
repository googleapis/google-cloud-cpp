// Copyright 2022 Google LLC
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

#include "google/cloud/testing_util/chrono_matchers.h"
#include <gtest/gtest.h>
#include <chrono>

namespace {

// Tautological tests to at least verify that `std::chrono` values are
// comparable and streamable.

TEST(ChronoMatchers, Duration) {
  auto d = std::chrono::milliseconds(500);
  EXPECT_LE(d, d) << d;
}

TEST(ChronoMatchers, TimePoint) {
  auto t = std::chrono::system_clock::now();
  EXPECT_LE(t, t) << t;
}

}  // namespace
