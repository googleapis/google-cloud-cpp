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

#include "google/cloud/internal/common_options.h"
#include <gmock/gmock.h>
#include <string>
#include <utility>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

namespace {

// Tests a generic option by setting it, then getting it.
template <typename T, typename ValueType = decltype(std::declval<T>().value)>
void TestOption(ValueType const& expected) {
  auto opts = Options{}.template set<T>(expected);
  EXPECT_EQ(expected, opts.template get<T>()->value)
      << "Failed with type: " << typeid(T).name();
}

}  // namespace

TEST(CommonOptions, RegularOptions) {
  TestOption<EndpointOption>("foo.googleapis.com");
  TestOption<UserAgentPrefixOption>({"foo", "bar"});
  TestOption<TracingComponentsOption>({"foo", "bar", "baz"});
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
