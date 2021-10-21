// Copyright 2020 Google LLC
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

#include "google/cloud/storage/internal/complex_option.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

struct PlaceholderOption : public ComplexOption<PlaceholderOption, int> {
  using ComplexOption<PlaceholderOption, int>::ComplexOption;
};

TEST(ComplexOptionTest, ValueOrEmptyCase) {
  PlaceholderOption d;
  ASSERT_FALSE(d.has_value());
  EXPECT_EQ(5, d.value_or(5));
}

TEST(ComplexOptionTest, ValueOrNonEmptyCase) {
  PlaceholderOption d(10);
  ASSERT_TRUE(d.has_value());
  EXPECT_EQ(10, d.value_or(5));
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
