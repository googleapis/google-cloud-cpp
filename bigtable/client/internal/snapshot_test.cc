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

#include "bigtable/client/internal/snapshot.h"

#include <gtest/gtest.h>

/// @test Verify Snapshot construction and trivial accessors.
TEST(SnapshotTest, Basic) {
  int64_t data_size = 100;
  std::string desc = "Test snapshot.";
  std::string name = "test_snapshot";
  bigtable::internal::Snapshot snap1(data_size, desc, name);

  EXPECT_EQ(data_size, snap1.data_size());
  EXPECT_EQ(desc, snap1.description());
  EXPECT_EQ(name, snap1.name());
}