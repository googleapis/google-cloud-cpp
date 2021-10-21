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

#include "google/cloud/bigtable/idempotent_mutation_policy.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace {

using ::google::cloud::testing_util::chrono_literals::operator"" _ms;
using ::google::cloud::testing_util::chrono_literals::operator"" _us;

/// @test Verify that the default policy works as expected.
TEST(IdempotentMutationPolicyTest, Simple) {
  auto policy = DefaultIdempotentMutationPolicy();
  EXPECT_TRUE(
      policy->is_idempotent(DeleteFromColumn("fam", "col", 0_us, 10_us).op));
  EXPECT_TRUE(policy->is_idempotent(DeleteFromFamily("fam").op));

  EXPECT_TRUE(policy->is_idempotent(SetCell("fam", "col", 0_ms, "v1").op));
  EXPECT_FALSE(policy->is_idempotent(SetCell("fam", "c2", "v2").op));
}

/// @test Verify that bigtable::AlwaysRetryMutationPolicy works as expected.
TEST(IdempotentMutationPolicyTest, AlwaysRetry) {
  AlwaysRetryMutationPolicy policy;
  EXPECT_TRUE(
      policy.is_idempotent(DeleteFromColumn("fam", "col", 0_us, 10_us).op));
  EXPECT_TRUE(policy.is_idempotent(DeleteFromFamily("fam").op));

  EXPECT_TRUE(policy.is_idempotent(SetCell("fam", "col", 0_ms, "v1").op));
  EXPECT_TRUE(policy.is_idempotent(SetCell("fam", "c2", "v2").op));

  auto clone = policy.clone();
  EXPECT_TRUE(clone->is_idempotent(SetCell("f", "c", "v").op));
  EXPECT_TRUE(clone->is_idempotent(SetCell("f", "c", 10_ms, "v").op));
}

}  // namespace
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
