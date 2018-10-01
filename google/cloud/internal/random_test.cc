// Copyright 2018 Google LLC
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

#include "google/cloud/internal/random.h"
#include <gmock/gmock.h>

using namespace google::cloud::internal;

TEST(BenchmarksRandom, Basic) {
  // This is not a statistical test for PRNG, basically we want to make
  // sure that MakeDefaultPRNG uses different seeds, or at least creates
  // different series:
  auto gen_string = []() {
    auto g = MakeDefaultPRNG();
    return Sample(g, 32, "0123456789abcdefghijklm");
  };
  std::string s0 = gen_string();
  std::string s1 = gen_string();
  EXPECT_NE(s0, s1);
}
