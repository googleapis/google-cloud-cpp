// Copyright 2022 Google LLC
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

#include "google/cloud/bigtable/internal/bigtable_stub_factory.h"
#include "google/cloud/bigtable/options.h"
#include "google/cloud/bigtable/testing/mock_bigtable_stub.h"
#include "google/cloud/common_options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/internal/async_streaming_read_rpc_impl.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/testing_util/mock_grpc_authentication_strategy.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include "google/cloud/testing_util/validate_propagator.h"
#include <gmock/gmock.h>
#include <chrono>
#include <regex>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::bigtable::testing::MockBigtableStub;
using ::google::cloud::testing_util::MakeStubFactoryMockAuth;
using ::google::cloud::testing_util::ScopedLog;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::ValidateMetadataFixture;
using ::testing::_;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::ResultOf;
using ::testing::Return;

MATCHER(IsWebSafeBase64, "") {
  std::regex regex(R"re([A-Za-z0-9_-]*)re");
  return std::regex_match(arg, regex);
}

using MockFactory = ::testing::MockFunction<std::shared_ptr<BigtableStub>(
    std::shared_ptr<grpc::Channel>)>;
static_assert(std::is_same<decltype(MockFactory{}.AsStdFunction()),
                           BaseBigtableStubFactory>::value,
              "Mismatched mock factory type");

TEST(BigtableStubFactory, RoundRobin) {
  auto constexpr kTestChannels = 3;

  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .Times(kTestChannels)
      .WillRepeatedly([](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockBigtableStub>();
        EXPECT_CALL(*mock, MutateRow)
            .WillOnce(Return(internal::AbortedError("fail")));
        return mock;
      });

  auto expect_channel_id = [](int id) {
    return ResultOf(
        "channel ID",
        [](grpc::ChannelArguments const& args) {
          return internal::GetIntChannelArgument(args, "grpc.channel_id");
        },
        Optional(id));
  };

  auto auth = MakeStubFactoryMockAuth();
  EXPECT_CALL(*auth, CreateChannel("localhost:1", expect_channel_id(0)));
  EXPECT_CALL(*auth, CreateChannel("localhost:1", expect_channel_id(1)));
  EXPECT_CALL(*auth, CreateChannel("localhost:1", expect_channel_id(2)));
  EXPECT_CALL(*auth, RequiresConfigureContext).WillOnce(Return(false));

  CompletionQueue cq;
  auto stub = CreateDecoratedStubs(
      std::move(auth), cq,
      Options{}
          .set<GrpcNumChannelsOption>(kTestChannels)
          .set<EndpointOption>("localhost:1")
          .set<UnifiedCredentialsOption>(MakeInsecureCredentials()),
      factory.AsStdFunction());

  grpc::ClientContext context;
  for (int i = 0; i != kTestChannels; ++i) {
    auto response = stub->MutateRow(context, Options{}, {});
    EXPECT_THAT(response, StatusIs(StatusCode::kAborted, "fail"));
  }
}

// Note that the channel refreshing decorator is tested in
// bigtable_channel_refresh_test.cc

TEST(BigtableStubFactory, Auth) {
  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockBigtableStub>();
        EXPECT_CALL(*mock, MutateRow).Times(0);
        return mock;
      });

  auto auth = MakeStubFactoryMockAuth();
  EXPECT_CALL(*auth, RequiresConfigureContext).WillOnce(Return(true));
  EXPECT_CALL(*auth, CreateChannel("localhost:1", _));
  EXPECT_CALL(*auth, ConfigureContext)
      .WillOnce(Return(internal::AbortedError("fail")));

  CompletionQueue cq;
  auto stub = CreateDecoratedStubs(
      std::move(auth), cq,
      Options{}
          .set<GrpcNumChannelsOption>(1)
          .set<EndpointOption>("localhost:1")
          .set<UnifiedCredentialsOption>(MakeInsecureCredentials()),
      factory.AsStdFunction());

  grpc::ClientContext context;
  auto response = stub->MutateRow(context, Options{}, {});
  EXPECT_THAT(response, StatusIs(StatusCode::kAborted, "fail"));
}

TEST(BigtableStubFactory, Metadata) {
  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockBigtableStub>();
        EXPECT_CALL(*mock, MutateRow)
            .WillOnce(
                [](grpc::ClientContext& context, Options const&,
                   google::bigtable::v2::MutateRowRequest const& request) {
                  ValidateMetadataFixture fixture;
                  fixture.IsContextMDValid(
                      context, "google.bigtable.v2.Bigtable.MutateRow", request,
                      internal::HandCraftedLibClientHeader());
                  return internal::AbortedError("fail");
                });
        return mock;
      });

  auto auth = MakeStubFactoryMockAuth();
  CompletionQueue cq;
  auto stub = CreateDecoratedStubs(
      std::move(auth), cq,
      Options{}
          .set<GrpcNumChannelsOption>(1)
          .set<EndpointOption>("localhost:1")
          .set<UnifiedCredentialsOption>(MakeInsecureCredentials()),
      factory.AsStdFunction());

  grpc::ClientContext context;
  auto response = stub->MutateRow(context, Options{}, {});
  EXPECT_THAT(response, StatusIs(StatusCode::kAborted, "fail"));
}

TEST(BigtableStubFactory, LoggingEnabled) {
  ScopedLog log;

  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockBigtableStub>();
        EXPECT_CALL(*mock, MutateRow)
            .WillOnce(Return(internal::AbortedError("fail")));
        return mock;
      });

  auto auth = MakeStubFactoryMockAuth();
  CompletionQueue cq;
  auto stub = CreateDecoratedStubs(
      std::move(auth), cq,
      Options{}
          .set<GrpcNumChannelsOption>(1)
          .set<LoggingComponentsOption>({"rpc"})
          .set<EndpointOption>("localhost:1")
          .set<UnifiedCredentialsOption>(MakeInsecureCredentials()),
      factory.AsStdFunction());

  grpc::ClientContext context;
  auto response = stub->MutateRow(context, Options{}, {});
  EXPECT_THAT(response, StatusIs(StatusCode::kAborted, "fail"));

  EXPECT_THAT(log.ExtractLines(), Contains(HasSubstr("MutateRow")));
}

TEST(BigtableStubFactory, LoggingDisabled) {
  ScopedLog log;

  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockBigtableStub>();
        EXPECT_CALL(*mock, MutateRow)
            .WillOnce(Return(internal::AbortedError("fail")));
        return mock;
      });

  auto auth = MakeStubFactoryMockAuth();
  CompletionQueue cq;
  auto stub = CreateDecoratedStubs(
      std::move(auth), cq,
      Options{}
          .set<GrpcNumChannelsOption>(1)
          .set<LoggingComponentsOption>({})
          .set<EndpointOption>("localhost:1")
          .set<UnifiedCredentialsOption>(MakeInsecureCredentials()),
      factory.AsStdFunction());

  grpc::ClientContext context;
  auto response = stub->MutateRow(context, Options{}, {});
  EXPECT_THAT(response, StatusIs(StatusCode::kAborted, "fail"));

  EXPECT_THAT(log.ExtractLines(), Not(Contains(HasSubstr("MutateRow"))));
}

TEST(BigtableStubFactory, FeaturesFlags) {
  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockBigtableStub>();
        EXPECT_CALL(*mock, MutateRow)
            .WillOnce([](grpc::ClientContext& context, Options const&,
                         google::bigtable::v2::MutateRowRequest const&) {
              ValidateMetadataFixture fixture;
              auto headers = fixture.GetMetadata(context);
              EXPECT_THAT(
                  headers,
                  Contains(Pair("bigtable-features",
                                AllOf(Not(IsEmpty()), IsWebSafeBase64()))));
              return internal::AbortedError("fail");
            });
        return mock;
      });

  auto auth = MakeStubFactoryMockAuth();
  CompletionQueue cq;
  auto stub = CreateDecoratedStubs(
      std::move(auth), std::move(cq),
      Options{}
          .set<EndpointOption>("localhost:1")
          .set<GrpcNumChannelsOption>(1)
          .set<UnifiedCredentialsOption>(MakeInsecureCredentials()),
      factory.AsStdFunction());
  grpc::ClientContext context;
  (void)stub->MutateRow(context, Options{}, {});
}

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
using ::google::cloud::testing_util::DisableTracing;
using ::google::cloud::testing_util::EnableTracing;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::ValidateNoPropagator;
using ::google::cloud::testing_util::ValidatePropagator;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

TEST(BigtableStubFactory, TracingEnabled) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockBigtableStub>();
        EXPECT_CALL(*mock, MutateRow)
            .WillOnce([](auto& context, auto const&, auto const&) {
              ValidatePropagator(context);
              return internal::AbortedError("fail");
            });
        return mock;
      });

  auto auth = MakeStubFactoryMockAuth();
  CompletionQueue cq;
  auto stub = CreateDecoratedStubs(
      std::move(auth), std::move(cq),
      EnableTracing(
          Options{}
              .set<EndpointOption>("localhost:1")
              .set<GrpcNumChannelsOption>(1)
              .set<UnifiedCredentialsOption>(MakeInsecureCredentials())),
      factory.AsStdFunction());
  grpc::ClientContext context;
  (void)stub->MutateRow(context, Options{}, {});

  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(SpanNamed("google.bigtable.v2.Bigtable/MutateRow")));
}

TEST(BigtableStubFactory, TracingDisabled) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockBigtableStub>();
        EXPECT_CALL(*mock, MutateRow)
            .WillOnce([](auto& context, auto const&, auto const&) {
              ValidateNoPropagator(context);
              return internal::AbortedError("fail");
            });
        return mock;
      });

  auto auth = MakeStubFactoryMockAuth();
  CompletionQueue cq;
  auto stub = CreateDecoratedStubs(
      std::move(auth), std::move(cq),
      DisableTracing(
          Options{}
              .set<EndpointOption>("localhost:1")
              .set<GrpcNumChannelsOption>(1)
              .set<UnifiedCredentialsOption>(MakeInsecureCredentials())),
      factory.AsStdFunction());
  grpc::ClientContext context;
  (void)stub->MutateRow(context, Options{}, {});

  EXPECT_THAT(span_catcher->GetSpans(), IsEmpty());
}
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
