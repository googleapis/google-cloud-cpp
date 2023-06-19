// Copyright 2021 Google LLC
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

#include "google/cloud/proxy_options.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/scoped_log.h"
#include <gmock/gmock.h>
#include <string>
#include <utility>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

namespace {

using ::testing::Contains;
using ::testing::ContainsRegex;

// Tests a generic option by setting it, then getting it.
template <typename T, typename ValueType = typename T::Type>
void TestOption(ValueType const& expected) {
  auto opts = Options{}.template set<T>(expected);
  EXPECT_EQ(expected, opts.template get<T>())
      << "Failed with type: " << typeid(T).name();
}

}  // namespace

TEST(ProxyServerOptionList, RegularOptions) {
  TestOption<ProxyServerAddressPortOption>({"address", "port"});
  TestOption<ProxyServerCredentialsOption>({"username", "password"});
}

TEST(ProxyServerOptionList, Expected) {
  testing_util::ScopedLog log;
  auto opts = Options{}
                .set<ProxyServerAddressPortOption>({"address", "port"})
                .set<ProxyServerCredentialsOption>({"username", "password"});

  internal::CheckExpectedOptions<ProxyServerOptionList>(opts, "caller");
  EXPECT_TRUE(log.ExtractLines().empty());
}

TEST(ProxyServerOptionList, Unexpected) {
  struct UnexpectedOption {
    using Type = int;
  };
  testing_util::ScopedLog log;
  Options opts;
  opts.set<UnexpectedOption>({});
  internal::CheckExpectedOptions<ProxyServerOptionList>(opts, "caller");
  EXPECT_THAT(
      log.ExtractLines(),
      Contains(ContainsRegex("caller: Unexpected option.+UnexpectedOption")));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
