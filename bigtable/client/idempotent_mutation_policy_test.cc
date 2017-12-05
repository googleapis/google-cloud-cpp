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

#include "bigtable/client/idempotent_mutation_policy.h"

#include <gmock/gmock.h>

/// @test Verify that the default policy works as expected.
TEST(IdempotentMutationPolicyTest, Simple) {
  auto policy = bigtable::DefaultIdempotentMutationPolicy();
  EXPECT_TRUE(policy->is_idempotent(
      bigtable::DeleteFromColumn("fam", "col", 0, 10).op));
  EXPECT_TRUE(policy->is_idempotent(bigtable::DeleteFromFamily("fam").op));

  EXPECT_TRUE(
      policy->is_idempotent(bigtable::SetCell("fam", "col", 0, "v1").op));
  EXPECT_FALSE(policy->is_idempotent(bigtable::SetCell("fam", "c2", "v2").op));
  EXPECT_FALSE(policy->is_idempotent(
      bigtable::SetCell("f", "c", bigtable::ServerSetTimestamp(), "v").op));
}

/// @test Verify that bigtable::AlwaysRetryMutationPolicy works as expected.
TEST(IdempotentMutationPolicyTest, AlwaysRetry) {
  bigtable::AlwaysRetryMutationPolicy policy;
  EXPECT_TRUE(
      policy.is_idempotent(bigtable::DeleteFromColumn("fam", "col", 0, 10).op));
  EXPECT_TRUE(policy.is_idempotent(bigtable::DeleteFromFamily("fam").op));

  EXPECT_TRUE(
      policy.is_idempotent(bigtable::SetCell("fam", "col", 0, "v1").op));
  EXPECT_TRUE(policy.is_idempotent(bigtable::SetCell("fam", "c2", "v2").op));
  EXPECT_TRUE(policy.is_idempotent(
      bigtable::SetCell("f", "c", bigtable::ServerSetTimestamp(), "v").op));

  auto clone = policy.clone();
  EXPECT_TRUE(clone->is_idempotent(
      bigtable::SetCell("f", "c", bigtable::ServerSetTimestamp(), "v").op));
  EXPECT_TRUE(clone->is_idempotent(bigtable::SetCell("f", "c", 10, "v").op));
}
