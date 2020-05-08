// Copyright 2017 Google Inc.
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

#include "google/cloud/bigtable/benchmarks/random_mutation.h"
#include "google/cloud/bigtable/benchmarks/constants.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
namespace benchmarks {
namespace {
TEST(BenchmarksRandomMutation, RandomValue) {
  auto g = google::cloud::internal::MakeDefaultPRNG();
  std::string val = MakeRandomValue(g);
  EXPECT_EQ(static_cast<std::size_t>(kFieldSize), val.size());
  std::string val2 = MakeRandomValue(g);
  EXPECT_NE(val, val2);
}

TEST(BenchmarksRandomMutation, RandomMutation) {
  auto g = google::cloud::internal::MakeDefaultPRNG();
  auto m = MakeRandomMutation(g, 0).op;

  ASSERT_TRUE(m.has_set_cell());
  EXPECT_EQ(kColumnFamily, m.set_cell().family_name());
  EXPECT_EQ("field0", m.set_cell().column_qualifier());
  EXPECT_EQ(0, m.set_cell().timestamp_micros());
  EXPECT_EQ(static_cast<std::size_t>(kFieldSize), m.set_cell().value().size());
}

}  // namespace
}  // namespace benchmarks
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
