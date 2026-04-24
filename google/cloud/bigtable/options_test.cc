// Copyright 2026 Google LLC
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

#include "google/cloud/bigtable/options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/fake_clock.h"
#include <gmock/gmock.h>
#include <chrono>
#include <memory>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::bigtable::DeadlineOption;
using ::google::cloud::internal::GrpcSetupOption;
using ::google::cloud::testing_util::FakeSystemClock;
using ::testing::Eq;

TEST(MergeOptions, ConnectionDeadlineOption) {
  FakeSystemClock clock;
  clock.SetTime(std::chrono::system_clock::now());
  auto now_fn = [&] { return clock.Now(); };

  auto connection_opts = Options{}.set<DeadlineOption>(std::chrono::seconds(1));
  Options client_opts;

  // Merge in a deadline. This should populate the GrpcSetupOption with a call
  // to ClientContext::set_deadline
  auto merged_client_opts =
      MergeOptions(std::move(client_opts), std::move(connection_opts), now_fn);

  ASSERT_TRUE(merged_client_opts.has<GrpcSetupOption>());
  grpc::ClientContext context;
  merged_client_opts.get<GrpcSetupOption>()(context);
  EXPECT_THAT(context.deadline() - clock.Now(), Eq(std::chrono::seconds(1)));
}

TEST(MergeOptions, ClientOverridesDeadlineOption) {
  FakeSystemClock clock;
  clock.SetTime(std::chrono::system_clock::now());
  auto now_fn = [&] { return clock.Now(); };

  auto connection_opts = Options{}.set<DeadlineOption>(std::chrono::seconds(1));
  auto client_opts = Options{}.set<DeadlineOption>(std::chrono::seconds(2));

  // Merge in a deadline. This should populate the GrpcSetupOption with a call
  // to ClientContext::set_deadline
  auto merged_client_opts =
      MergeOptions(std::move(client_opts), std::move(connection_opts), now_fn);

  ASSERT_TRUE(merged_client_opts.has<GrpcSetupOption>());
  grpc::ClientContext context;
  merged_client_opts.get<GrpcSetupOption>()(context);
  EXPECT_THAT(context.deadline() - clock.Now(), Eq(std::chrono::seconds(2)));
}

TEST(MergeOptions, CallOverridesAllOtherDeadlineOption) {
  FakeSystemClock clock;
  clock.SetTime(std::chrono::system_clock::now());
  auto now_fn = [&] { return clock.Now(); };

  auto connection_opts = Options{}.set<DeadlineOption>(std::chrono::seconds(1));
  auto client_opts = Options{}.set<DeadlineOption>(std::chrono::seconds(2));

  // Merge in a deadline. This should populate the GrpcSetupOption with a call
  // to ClientContext::set_deadline
  auto merged_client_opts =
      MergeOptions(std::move(client_opts), std::move(connection_opts), now_fn);

  auto call_opts = Options{}.set<DeadlineOption>(std::chrono::seconds(5));

  auto merged_call_opts =
      MergeOptions(std::move(call_opts), std::move(merged_client_opts), now_fn);

  ASSERT_TRUE(merged_call_opts.has<GrpcSetupOption>());
  grpc::ClientContext context;
  merged_call_opts.get<GrpcSetupOption>()(context);
  EXPECT_THAT(context.deadline() - clock.Now(), Eq(std::chrono::seconds(5)));
}

TEST(MergeOptions, OnlyDeadlineOptionSupersedesGrpcSetupOptionDeadline) {
  FakeSystemClock clock;
  clock.SetTime(std::chrono::system_clock::now());
  auto now_fn = [&] { return clock.Now(); };

  auto setup_fn = [&](grpc::ClientContext& context) {
    EXPECT_THAT(context.compression_algorithm(), Eq(GRPC_COMPRESS_NONE));
    // compression_algorithm is set to verify the non-deadline aspects of the
    // GrpcSetupOption are preserved.
    context.set_compression_algorithm(GRPC_COMPRESS_GZIP);
    context.set_deadline(now_fn() + std::chrono::seconds(10));
  };

  auto connection_opts = Options{}.set<GrpcSetupOption>(setup_fn);
  Options client_opts;

  auto merged_client_opts =
      MergeOptions(std::move(client_opts), std::move(connection_opts), now_fn);
  {
    ASSERT_TRUE(merged_client_opts.has<GrpcSetupOption>());
    grpc::ClientContext context;
    merged_client_opts.get<GrpcSetupOption>()(context);
    EXPECT_THAT(context.compression_algorithm(), Eq(GRPC_COMPRESS_GZIP));
    EXPECT_THAT(context.deadline() - clock.Now(), Eq(std::chrono::seconds(10)));
  }

  connection_opts =
      Options{}.set<GrpcSetupOption>(setup_fn).set<DeadlineOption>(
          std::chrono::seconds(1));
  client_opts = Options{};

  // Merge in a deadline. This should populate the GrpcSetupOption with a call
  // to ClientContext::set_deadline after calling the GrpcSetupOption function.
  merged_client_opts =
      MergeOptions(std::move(client_opts), std::move(connection_opts), now_fn);

  ASSERT_TRUE(merged_client_opts.has<GrpcSetupOption>());
  grpc::ClientContext context;
  merged_client_opts.get<GrpcSetupOption>()(context);
  EXPECT_THAT(context.compression_algorithm(), Eq(GRPC_COMPRESS_GZIP));
  EXPECT_THAT(context.deadline() - clock.Now(), Eq(std::chrono::seconds(1)));
}

TEST(MergeOptions, ClientDeadlineOptionCallGrpcSetupOption) {
  FakeSystemClock clock;
  clock.SetTime(std::chrono::system_clock::now());
  auto now_fn = [&] { return clock.Now(); };

  auto setup_fn = [&](grpc::ClientContext& context) {
    EXPECT_THAT(context.compression_algorithm(), Eq(GRPC_COMPRESS_NONE));
    // compression_algorithm is set to verify the non-deadline aspects of the
    // GrpcSetupOption are preserved.
    context.set_compression_algorithm(GRPC_COMPRESS_GZIP);
    context.set_deadline(now_fn() + std::chrono::seconds(10));
  };

  auto call_setup_fn = [&](grpc::ClientContext& context) {
    EXPECT_THAT(context.compression_algorithm(), Eq(GRPC_COMPRESS_NONE));
    context.set_compression_algorithm(GRPC_COMPRESS_DEFLATE);
  };

  auto connection_opts = Options{}.set<GrpcSetupOption>(setup_fn);
  auto client_opts = Options{}.set<DeadlineOption>(std::chrono::seconds(5));
  auto call_opts = Options{}.set<GrpcSetupOption>(call_setup_fn);

  auto merged_client_opts =
      MergeOptions(std::move(client_opts), std::move(connection_opts), now_fn);
  auto merged_call_opts =
      MergeOptions(std::move(call_opts), std::move(merged_client_opts), now_fn);

  ASSERT_TRUE(merged_call_opts.has<GrpcSetupOption>());
  grpc::ClientContext context;
  merged_call_opts.get<GrpcSetupOption>()(context);
  EXPECT_THAT(context.compression_algorithm(), Eq(GRPC_COMPRESS_DEFLATE));
  EXPECT_THAT(context.deadline() - clock.Now(), Eq(std::chrono::seconds(5)));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
