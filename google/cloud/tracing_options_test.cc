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

#include "google/cloud/tracing_options.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace {

TEST(TracingOptionsTest, Defaults) {
  TracingOptions tracing_options;
  EXPECT_TRUE(tracing_options.single_line_mode());
  EXPECT_TRUE(tracing_options.use_short_repeated_primitives());
  EXPECT_EQ(128, tracing_options.truncate_string_field_longer_than());

  // Unknown/unparseable options are ignored.
  tracing_options.SetOptions("foo=1,bar=T,baz=no");
  EXPECT_TRUE(tracing_options.single_line_mode());
  EXPECT_TRUE(tracing_options.use_short_repeated_primitives());
  EXPECT_EQ(128, tracing_options.truncate_string_field_longer_than());
}

TEST(TracingOptionsTest, Override) {
  TracingOptions tracing_options;
  tracing_options.SetOptions(
      ",single_line_mode=F"
      ",use_short_repeated_primitives=n"
      ",truncate_string_field_longer_than=256");
  EXPECT_FALSE(tracing_options.single_line_mode());
  EXPECT_FALSE(tracing_options.use_short_repeated_primitives());
  EXPECT_EQ(256, tracing_options.truncate_string_field_longer_than());
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
