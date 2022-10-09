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
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

namespace {

using ::testing::Contains;
using ::testing::ContainsRegex;
using ::testing::IsEmpty;
using ::testing::IsNull;
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
  expected.SetInt(GRPC_ARG_KEEPALIVE_TIME_MS, 86400000);
  expected.SetInt(GRPC_ARG_KEEPALIVE_TIMEOUT_MS, 60000);

  CheckGrpcChannelArguments(expected, internal::MakeChannelArguments(opts));
}

TEST(GrpcChannelArguments, MakeChannelArgumentsDefaults) {
  grpc::ChannelArguments args;

  // not present
  EXPECT_FALSE(internal::GetIntChannelArgument(args, GRPC_ARG_KEEPALIVE_TIME_MS));

  // present
  args.SetInt(GRPC_ARG_KEEPALIVE_TIME_MS, 86400000);
  auto value = internal::GetIntChannelArgument(args, GRPC_ARG_KEEPALIVE_TIME_MS);
  ASSERT_TRUE(value.has_value());
  EXPECT_GT(value, std::chrono::milliseconds(std::chrono::hours(1)).count());

  // not present
  EXPECT_FALSE(internal::GetIntChannelArgument(args, GRPC_ARG_KEEPALIVE_TIMEOUT_MS));

  // present
  args.SetInt(GRPC_ARG_KEEPALIVE_TIMEOUT_MS, 60000);
  auto value_timeout = internal::GetIntChannelArgument(args, GRPC_ARG_KEEPALIVE_TIMEOUT_MS);
  ASSERT_TRUE(value_timeout.has_value());
  EXPECT_GT(value_timeout, std::chrono::milliseconds(std::chrono::seconds(1)).count());
}

TEST(GrpcChannelArguments, MakeChannelArgumentsDefaults_checkOurDefaultsOnly) {
  auto options = Options{};
  auto args = internal::MakeChannelArguments(options);

  auto value = internal::GetIntChannelArgument(args, GRPC_ARG_KEEPALIVE_TIME_MS);
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(value, std::chrono::milliseconds(std::chrono::hours(24)).count());

  auto value_timeout = internal::GetIntChannelArgument(args, GRPC_ARG_KEEPALIVE_TIMEOUT_MS);
  ASSERT_TRUE(value_timeout.has_value());
  EXPECT_EQ(value_timeout, std::chrono::milliseconds(std::chrono::seconds(60)).count());
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

TEST(GrpcOptionList, GrpcCompletionQueueOption) {
  using ms = std::chrono::milliseconds;
  CompletionQueue cq;
  auto background = internal::MakeBackgroundThreadsFactory(
      Options{}.set<GrpcCompletionQueueOption>(cq))();

  // Schedule some work that cannot execute because there is no thread draining
  // the completion queue.
  promise<std::thread::id> p;
  auto background_thread_id = p.get_future();
  background->cq().RunAsync(
      [&p](CompletionQueue&) { p.set_value(std::this_thread::get_id()); });
  EXPECT_NE(std::future_status::ready, background_thread_id.wait_for(ms(10)));

  // Verify we can create our own threads to drain the completion queue.
  std::thread t([&cq] { cq.Run(); });
  EXPECT_EQ(t.get_id(), background_thread_id.get());

  cq.Shutdown();
  t.join();
}

// Verify that the `GrpcCompletionQueueOption` takes precedence over the
// `GrpcBackgroundThreadsFactoryOption` when both are set.
TEST(GrpcOptionList, GrpcBackgroundThreadsFactoryIgnored) {
  auto f = [] { return absl::make_unique<ThreadPool>(); };
  auto threads = internal::MakeBackgroundThreadsFactory(
      Options{}
          .set<GrpcCompletionQueueOption>(CompletionQueue{})
          // This value will be ignored because a `CompletionQueue` is supplied
          .set<GrpcBackgroundThreadsFactoryOption>(f))();

  // The two options return different classes derived from `BackgroundThreads`.
  // Simply check that the type we received is what we expect from
  // `GrpcCompletionQueueOption` and not `GrpcBackgroundThreadsFactoryOption`.
  auto* custom_tp =
      dynamic_cast<internal::CustomerSuppliedBackgroundThreads*>(threads.get());
  EXPECT_THAT(custom_tp, NotNull());
  auto* default_tp = dynamic_cast<ThreadPool*>(threads.get());
  EXPECT_THAT(default_tp, IsNull());
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
  auto opts = Options{}
                  .set<GrpcCredentialOption>({})
                  .set<GrpcNumChannelsOption>({})
                  .set<GrpcChannelArgumentsOption>({})
                  .set<GrpcChannelArgumentsNativeOption>({})
                  .set<GrpcTracingOptionsOption>({})
                  .set<GrpcBackgroundThreadPoolSizeOption>({})
                  .set<GrpcCompletionQueueOption>({})
                  .set<GrpcBackgroundThreadsFactoryOption>({});
  internal::CheckExpectedOptions<GrpcOptionList>(opts, "caller");
  EXPECT_THAT(log.ExtractLines(), IsEmpty());
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

TEST(GrpcClientContext, Configure) {
  auto setup = [](grpc::ClientContext& context) {
    // This might not be the most useful setting, but it is the most easily
    // tested setting.
    context.set_compression_algorithm(GRPC_COMPRESS_DEFLATE);
  };
  auto opts = Options{}.set<internal::GrpcSetupOption>(setup);

  grpc::ClientContext context;
  EXPECT_EQ(GRPC_COMPRESS_NONE, context.compression_algorithm());
  internal::ConfigureContext(context, std::move(opts));
  EXPECT_EQ(GRPC_COMPRESS_DEFLATE, context.compression_algorithm());
}

TEST(GrpcClientContext, ConfigurePoll) {
  auto setup_poll = [](grpc::ClientContext& context) {
    // This might not be the most useful setting, but it is the most easily
    // tested setting.
    context.set_compression_algorithm(GRPC_COMPRESS_DEFLATE);
  };
  auto opts = Options{}.set<internal::GrpcSetupPollOption>(setup_poll);

  grpc::ClientContext context;
  EXPECT_EQ(GRPC_COMPRESS_NONE, context.compression_algorithm());
  internal::ConfigureContext(context, opts);
  EXPECT_EQ(GRPC_COMPRESS_NONE, context.compression_algorithm());
  internal::ConfigurePollContext(context, std::move(opts));
  EXPECT_EQ(GRPC_COMPRESS_DEFLATE, context.compression_algorithm());
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
