// Copyright 2018 Google Inc.
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

#include "bigtable/client/read_modify_write_rule.h"
#include <gmock/gmock.h>

namespace btproto = ::google::bigtable::v2;

TEST(ReadModifyWriteRuleTest, AppendValue) {
  auto const proto =
      bigtable::ReadModifyWriteRule::AppendValue("fam", "col", "foo")
          .as_proto();
  EXPECT_EQ(btproto::ReadModifyWriteRule::kAppendValue, proto.rule_case());
  EXPECT_EQ("foo", proto.append_value());
  EXPECT_EQ("fam", proto.family_name());
  EXPECT_EQ("col", proto.column_qualifier());
}

TEST(ReadModifyWriteRuleTest, IncrementAmount) {
  auto const proto =
      bigtable::ReadModifyWriteRule::IncrementAmount("fam", "col", 42)
          .as_proto();
  EXPECT_EQ(btproto::ReadModifyWriteRule::kIncrementAmount, proto.rule_case());
  EXPECT_EQ(42, proto.increment_amount());
  EXPECT_EQ("fam", proto.family_name());
  EXPECT_EQ("col", proto.column_qualifier());
}
