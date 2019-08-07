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

#include "google/cloud/storage/benchmarks/benchmark_utils.h"
#include <gmock/gmock.h>
#include <thread>

namespace google {
namespace cloud {
namespace storage_benchmarks {
namespace {

TEST(ProgressReporterTest, Trivial) {
  ProgressReporter rep;
  rep.Start();
  std::this_thread::sleep_for(std::chrono::milliseconds(2));
  rep.Advance(5);
  std::this_thread::sleep_for(std::chrono::milliseconds(3));
  rep.Advance(7);
  auto res = rep.GetAccumulatedProgress();
  EXPECT_EQ(3U, res.size());
  EXPECT_EQ(0, res[0].bytes);
  EXPECT_EQ(0, res[0].elapsed.count());
  EXPECT_EQ(5, res[1].bytes);
  EXPECT_LE(2000, res[1].elapsed.count());
  EXPECT_EQ(7, res[2].bytes);
  EXPECT_LE(3000, res[2].elapsed.count());
}

}  // namespace
}  // namespace storage_benchmarks
}  // namespace cloud
}  // namespace google
