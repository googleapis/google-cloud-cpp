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
#include "google/cloud/common_options.h"
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
using ::testing::NotNull;
using ThreadPool = internal::AutomaticallyCreatedBackgroundThreads;

// Tests a generic option by setting it, then getting it.
template <typename T, typename ValueType = typename T::Type>
void TestGrpcOption(ValueType const& expected) {
  auto opts = Options{}.template set<T>(expected);
  EXPECT_EQ(expected, opts.template get<T>())
      << "Failed with type: " << typeid(T).name();
}

// Checks equality for two grpc::ChannelArguments
void CheckGrpcChannelArguments(grpc::ChannelArguments const& expected,
                               grpc::ChannelArguments const& actual) {
  auto c_args_expected = expected.c_channel_args();
  auto c_args_actual = actual.c_channel_args();
  ASSERT_EQ(c_args_expected.num_args, c_args_actual.num_args)
      << "Number of channel arguments are mismatched";

  for (size_t i = 0; i != c_args_expected.num_args; ++i) {
    auto& expected = c_args_expected.args[i];
    auto& actual = c_args_actual.args[i];
    ASSERT_EQ(std::string(expected.key), std::string(actual.key))
        << "Order of keys are mismatched";
    ASSERT_EQ(expected.type, actual.type)
        << "Type mismatch for key: " << std::string(expected.key);
    if (expected.type == GRPC_ARG_STRING) {
      EXPECT_EQ(std::string(expected.value.string),
                std::string(actual.value.string))
          << "Value (string) mismatch for key: " << std::string(expected.key);
    } else if (expected.type == GRPC_ARG_INTEGER) {
      EXPECT_EQ(expected.value.integer, actual.value.integer)
          << "Value (integer) mismatch for key: " << std::string(expected.key);
    } else if (expected.type == GRPC_ARG_POINTER) {
      auto& ptr_expected = expected.value.pointer;
      auto& ptr_actual = actual.value.pointer;

      // First we check to see if the objects are the same type, by checking to
      // see if they point to the same vtable
      auto e = "Object mismatch for key: " + std::string(expected.key);
      ASSERT_EQ(ptr_expected.vtable->cmp, ptr_actual.vtable->cmp) << e;
      ASSERT_EQ(ptr_expected.vtable->copy, ptr_actual.vtable->copy) << e;
      ASSERT_EQ(ptr_expected.vtable->destroy, ptr_actual.vtable->destroy) << e;

      // Then we use the comparison function to check equality of the two
      // objects
      EXPECT_TRUE(ptr_expected.vtable->cmp(ptr_expected.p, ptr_actual.p))
          << "Value (pointer) mismatch for key: " << std::string(expected.key);
    }
  }
}

}  // namespace

TEST(GrpcOptionList, RegularOptions) {
  TestGrpcOption<GrpcCredentialOption>(grpc::InsecureChannelCredentials());
  TestGrpcOption<GrpcNumChannelsOption>(42);
  TestGrpcOption<GrpcChannelArgumentsOption>({{"foo", "bar"}, {"baz", "quux"}});
  TestGrpcOption<GrpcTracingOptionsOption>(TracingOptions{});
}

TEST(GrpcChannelArguments, MakeChannelArguments) {
  // This test will just set all 3 options related to channel arguments and
  // ensure that `MakeChannelArguments` combines them in the correct order.
  grpc::ChannelArguments native;
  native.SetString("foo", "bar");

  auto opts = Options{}
                  .set<GrpcChannelArgumentsOption>({{"baz", "quux"}})
                  .set<UserAgentProductsOption>({"user_agent"})
                  .set<GrpcChannelArgumentsNativeOption>(native);

  grpc::ChannelArguments expected;
  expected.SetString("foo", "bar");
  expected.SetString("baz", "quux");
  expected.SetUserAgentPrefix("user_agent");

  CheckGrpcChannelArguments(expected, internal::MakeChannelArguments(opts));
}

TEST(GrpcChannelArguments, GetIntChannelArgument) {
  grpc::ChannelArguments args;
  args.SetInt("key", 1);
  args.SetInt("key", 2);
  EXPECT_FALSE(internal::GetIntChannelArgument(args, "key-not-present"));
  auto value = internal::GetIntChannelArgument(args, "key");
  ASSERT_TRUE(value.has_value());
  // Check that the value is extracted, and that the first value is used.
  EXPECT_EQ(1, value.value());
}

TEST(GrpcChannelArguments, GetStringChannelArgument) {
  grpc::ChannelArguments args;
  args.SetString("key", "first-value");
  args.SetString("key", "last-value");
  EXPECT_FALSE(internal::GetStringChannelArgument(args, "key-not-present"));
  auto value = internal::GetStringChannelArgument(args, "key");
  ASSERT_TRUE(value.has_value());
  // Check that the value is extracted, and that the first value is used.
  EXPECT_EQ("first-value", value.value());
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
  auto threads = internal::MakeBackgroundThreadsFactory()();
  auto* tp = dynamic_cast<ThreadPool*>(threads.get());
  ASSERT_THAT(tp, NotNull());
  EXPECT_EQ(1U, tp->pool_size());
}

TEST(GrpcOptionList, GrpcBackgroundThreadPoolSizeOption) {
  auto threads = internal::MakeBackgroundThreadsFactory(
      Options{}.set<GrpcBackgroundThreadPoolSizeOption>(4))();
  auto* tp = dynamic_cast<ThreadPool*>(threads.get());
  ASSERT_THAT(tp, NotNull());
  EXPECT_EQ(4U, tp->pool_size());
}

// Verify that the `GrpcBackgroundThreadsFactoryOption` takes precedence over
// the `GrpcBackgroundThreadPoolSizeOption` when both are set.
TEST(GrpcOptionList, GrpcBackgroundThreadPoolSizeIgnored) {
  auto constexpr kThreadPoolSize = 4;
  auto f = [kThreadPoolSize] {
    return absl::make_unique<ThreadPool>(kThreadPoolSize);
  };
  auto threads = internal::MakeBackgroundThreadsFactory(
      Options{}
          .set<GrpcBackgroundThreadsFactoryOption>(f)
          // This value will be ignored because a custom factory is supplied
          .set<GrpcBackgroundThreadPoolSizeOption>(kThreadPoolSize + 1))();
  auto* tp = dynamic_cast<ThreadPool*>(threads.get());
  ASSERT_THAT(tp, NotNull());
  EXPECT_EQ(kThreadPoolSize, tp->pool_size());
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
