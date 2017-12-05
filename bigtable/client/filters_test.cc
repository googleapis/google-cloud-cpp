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

#include "bigtable/client/filters.h"

#include <gmock/gmock.h>

/// @test Verify that simple filters work as expected.
TEST(FiltersTest, Simple) {
  auto ln = bigtable::Filter::latest(3);
  {
    auto proto = ln.as_proto();
    EXPECT_EQ(3, proto.cells_per_column_limit_filter());
  }
}
