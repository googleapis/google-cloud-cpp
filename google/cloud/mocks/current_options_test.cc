// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/mocks/current_options.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace mocks {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

TEST(CurrentOptionsTest, Basic) {
  struct IntOption {
    using Type = int;
  };

  EXPECT_FALSE(CurrentOptions().has<IntOption>());
  {
    internal::OptionsSpan span(Options{}.set<IntOption>(1));
    EXPECT_EQ(CurrentOptions().get<IntOption>(), 1);
    {
      internal::OptionsSpan span(Options{}.set<IntOption>(2));
      EXPECT_EQ(CurrentOptions().get<IntOption>(), 2);
    }
    EXPECT_EQ(CurrentOptions().get<IntOption>(), 1);
  }
  EXPECT_FALSE(CurrentOptions().has<IntOption>());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace mocks
}  // namespace cloud
}  // namespace google
