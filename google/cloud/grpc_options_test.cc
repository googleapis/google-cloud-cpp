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
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>
#include <string>
#include <utility>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {

namespace {

using ::testing::Contains;
using ::testing::ContainsRegex;

// Tests a generic option by setting it, then getting it.
template <typename T, typename ValueType = typename T::Type>
void TestGrpcOption(ValueType const& expected) {
  auto opts = Options{}.template set<T>(expected);
  EXPECT_EQ(expected, opts.template get<T>())
      << "Failed with type: " << typeid(T).name();
}

}  // namespace

TEST(GrpcOptionList, RegularOptions) {
  TestGrpcOption<GrpcCredentialOption>(grpc::InsecureChannelCredentials());
  TestGrpcOption<GrpcNumChannelsOption>(42);
  TestGrpcOption<GrpcChannelArgumentsOption>({{"foo", "bar"}, {"baz", "quux"}});
  TestGrpcOption<GrpcTracingOptionsOption>(TracingOptions{});
}

TEST(GrpcOptionList, GrpcBackgroundThreadsFactoryOption) {
  struct Fake : BackgroundThreads {
    CompletionQueue cq() const override { return {}; }
  };
  bool invoked = false;
  auto factory = [&invoked] {
    invoked = true;
    return absl::make_unique<Fake>();
  };
  auto opts = Options{}.set<GrpcBackgroundThreadsFactoryOption>(factory);
  EXPECT_FALSE(invoked);
  opts.get<GrpcBackgroundThreadsFactoryOption>()();
  EXPECT_TRUE(invoked);
}

TEST(GrpcOptionList, DefaultBackgroundThreadsFactory) {
  auto f = internal::DefaultBackgroundThreadsFactory();
  auto* tp =
      dynamic_cast<internal::AutomaticallyCreatedBackgroundThreads*>(f.get());
  ASSERT_NE(nullptr, tp);
  EXPECT_EQ(1, tp->pool_size());
}

TEST(GrpcOptionList, Expected) {
  testing_util::ScopedLog log;
  Options opts;
  opts.set<GrpcNumChannelsOption>(42);
  internal::CheckExpectedOptions<GrpcOptionList>(opts, "caller");
  EXPECT_TRUE(log.ExtractLines().empty());
}

TEST(GrpcOptionList, Unexpected) {
  struct UnexpectedOption {
    using Type = int;
  };
  testing_util::ScopedLog log;
  Options opts;
  opts.set<UnexpectedOption>({});
  internal::CheckExpectedOptions<GrpcOptionList>(opts, "caller");
  EXPECT_THAT(
      log.ExtractLines(),
      Contains(ContainsRegex("caller: Unexpected option.+UnexpectedOption")));
}

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
