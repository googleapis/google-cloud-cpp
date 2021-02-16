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

#include "google/cloud/grpc_options.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>
#include <string>
#include <utility>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {

namespace {

// Tests a generic option by setting it, then getting it.
template <typename T, typename ValueType = decltype(std::declval<T>().value)>
void TestGrpcOption(ValueType const& expected) {
  auto opts = Options{}.template set<T>(expected);
  EXPECT_EQ(expected, opts.template get<T>()->value)
      << "Failed with type: " << typeid(T).name();
}

}  // namespace

TEST(GrpcOptions, RegularOptions) {
  TestGrpcOption<GrpcCredentialOption>(grpc::InsecureChannelCredentials());
  TestGrpcOption<GrpcEndpointOption>("foo.googleapis.com");
  TestGrpcOption<GrpcNumChannelsOption>(42);
  TestGrpcOption<GrpcUserAgentPrefixOption>({"foo", "bar"});
  TestGrpcOption<GrpcChannelArgumentsOption>({{"foo", "bar"}, {"baz", "quux"}});
  TestGrpcOption<GrpcTracingComponentsOption>({"foo", "bar", "baz"});
  TestGrpcOption<GrpcTracingOptionsOption>(TracingOptions{});
}

TEST(GrpcOptions, GrpcBackgroundThreadsOption) {
  struct Fake : BackgroundThreads {
    CompletionQueue cq() const override { return {}; }
  };
  bool invoked = false;
  auto factory = [&invoked] {
    invoked = true;
    return absl::make_unique<Fake>();
  };
  auto opts = Options{}.set<GrpcBackgroundThreadsOption>(factory);
  EXPECT_FALSE(invoked);
  opts.get<GrpcBackgroundThreadsOption>()->value();
  EXPECT_TRUE(invoked);
}

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
