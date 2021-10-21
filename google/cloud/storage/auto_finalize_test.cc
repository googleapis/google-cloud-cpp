// Copyright 2021 Google LLC
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

#include "google/cloud/storage/auto_finalize.h"
#include <gmock/gmock.h>
#include <sstream>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

TEST(AutoFinalizeTest, Default) {
  auto const actual = AutoFinalize{};
  ASSERT_TRUE(actual.has_value());
  EXPECT_EQ(actual.value(), AutoFinalizeConfig::kEnabled);
  std::ostringstream os;
  os << actual;
  EXPECT_EQ("auto-finalize=enabled", std::move(os).str());
}

TEST(AutoFinalizeTest, Enabled) {
  auto const actual = AutoFinalizeEnabled();
  ASSERT_TRUE(actual.has_value());
  EXPECT_EQ(actual.value(), AutoFinalizeConfig::kEnabled);
  std::ostringstream os;
  os << actual;
  EXPECT_EQ("auto-finalize=enabled", std::move(os).str());
}

TEST(AutoFinalizeTest, Disabled) {
  auto const actual = AutoFinalizeDisabled();
  ASSERT_TRUE(actual.has_value());
  EXPECT_EQ(actual.value(), AutoFinalizeConfig::kDisabled);
  std::ostringstream os;
  os << actual;
  EXPECT_EQ("auto-finalize=disabled", std::move(os).str());
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
