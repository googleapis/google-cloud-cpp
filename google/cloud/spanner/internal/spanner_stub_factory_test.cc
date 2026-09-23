// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/internal/spanner_stub_factory.h"
#include "google/cloud/spanner/internal/operation_context.h"
#include "google/cloud/spanner/internal/spanner_tracing_stub.h"
#include "google/cloud/spanner/testing/mock_spanner_stub.h"
#include "google/cloud/common_options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include "google/cloud/testing_util/validate_propagator.h"
#include <gmock/gmock.h>
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::NotNull;
using ::testing::Pair;
using ::testing::Return;

TEST(DecorateSpannerStub, Auth) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  EXPECT_CALL(*mock, CreateSession)
      .WillOnce([](grpc::ClientContext& context, Options const&,
                   google::spanner::v1::CreateSessionRequest const&,
                   internal::OperationContext&) {
        EXPECT_THAT(context.credentials(), NotNull());
        return internal::AbortedError("fail");
      });
  auto credentials = MakeAccessTokenCredentials(
      "test-only-invalid",
      std::chrono::system_clock::now() + std::chrono::minutes(5));
  auto opts = Options{}
                  .set<UnifiedCredentialsOption>(credentials)
                  .set<LoggingComponentsOption>({"rpc"});
  internal::AutomaticallyCreatedBackgroundThreads background;
  auto auth = internal::CreateAuthenticationStrategy(background.cq(), opts);
  auto stub = DecorateSpannerStub(mock, spanner::Database("foo", "bar", "baz"),
                                  std::move(auth), opts);
  ASSERT_NE(stub, nullptr);

  grpc::ClientContext context;
  OperationContext op_context(nullptr, 1, "CreateSession");
  auto session = stub->CreateSession(context, Options{}, {}, op_context);
  EXPECT_THAT(session, StatusIs(StatusCode::kAborted));
}

TEST(DecorateSpannerStub, Metadata) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto const db = spanner::Database("foo", "bar", "baz");
  EXPECT_CALL(*mock, CreateSession)
      .WillOnce([&db](grpc::ClientContext& context, Options const&,
                      google::spanner::v1::CreateSessionRequest const&,
                      OperationContext&) {
        testing_util::ValidateMetadataFixture fixture;
        auto metadata = fixture.GetMetadata(context);
        EXPECT_THAT(metadata, Contains(Pair("google-cloud-resource-prefix",
                                            db.FullName())));
        return internal::AbortedError("fail");
      });
  auto opts =
      Options{}.set<UnifiedCredentialsOption>(MakeInsecureCredentials());
  internal::AutomaticallyCreatedBackgroundThreads background;
  auto auth = internal::CreateAuthenticationStrategy(background.cq(), opts);
  auto stub = DecorateSpannerStub(mock, db, std::move(auth), opts);
  ASSERT_NE(stub, nullptr);

  grpc::ClientContext context;
  OperationContext op_context(nullptr, 1, "CreateSession");
  auto session = stub->CreateSession(context, Options{}, {}, op_context);
  EXPECT_THAT(session, StatusIs(StatusCode::kAborted));
}

TEST(DecorateSpannerStub, Logging) {
  testing_util::ScopedLog log;

  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  EXPECT_CALL(*mock, CreateSession)
      .WillOnce(Return(internal::AbortedError("fail")));
  auto opts = Options{}
                  .set<UnifiedCredentialsOption>(MakeInsecureCredentials())
                  .set<LoggingComponentsOption>({"rpc"});
  internal::AutomaticallyCreatedBackgroundThreads background;
  auto auth = internal::CreateAuthenticationStrategy(background.cq(), opts);
  auto stub = DecorateSpannerStub(mock, spanner::Database("foo", "bar", "baz"),
                                  std::move(auth), opts);
  ASSERT_NE(stub, nullptr);

  grpc::ClientContext context;
  OperationContext op_context(nullptr, 1, "CreateSession");
  auto session = stub->CreateSession(context, Options{}, {}, op_context);
  EXPECT_THAT(session, StatusIs(StatusCode::kAborted));

  EXPECT_THAT(log.ExtractLines(),
              Contains(AllOf(HasSubstr("CreateSession"), HasSubstr("fail"))));
}

using ::google::cloud::testing_util::DisableTracing;
using ::google::cloud::testing_util::EnableTracing;
using ::google::cloud::testing_util::SpanNamed;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

TEST(DecorateSpannerStub, TracingEnabled) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  EXPECT_CALL(*mock, CreateSession)
      .WillOnce([](auto& context, auto const&, auto const&, auto&) {
        testing_util::ValidatePropagator(context);
        return internal::AbortedError("fail");
      });
  auto opts = EnableTracing(
      Options{}.set<UnifiedCredentialsOption>(MakeInsecureCredentials()));
  internal::AutomaticallyCreatedBackgroundThreads background;
  auto auth = internal::CreateAuthenticationStrategy(background.cq(), opts);
  auto stub = DecorateSpannerStub(mock, spanner::Database("foo", "bar", "baz"),
                                  std::move(auth), opts);
  ASSERT_NE(stub, nullptr);

  grpc::ClientContext context;
  OperationContext op_context(nullptr, 1, "CreateSession");
  auto session = stub->CreateSession(context, Options{}, {}, op_context);
  EXPECT_THAT(session, StatusIs(StatusCode::kAborted));

  EXPECT_THAT(
      span_catcher->GetSpans(),
      ElementsAre(SpanNamed("google.spanner.v1.Spanner/CreateSession")));
}

TEST(DecorateSpannerStub, TracingDisabled) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  EXPECT_CALL(*mock, CreateSession)
      .WillOnce([](auto& context, auto const&, auto const&, auto&) {
        testing_util::ValidateNoPropagator(context);
        return internal::AbortedError("fail");
      });
  auto opts = DisableTracing(
      Options{}.set<UnifiedCredentialsOption>(MakeInsecureCredentials()));
  internal::AutomaticallyCreatedBackgroundThreads background;
  auto auth = internal::CreateAuthenticationStrategy(background.cq(), opts);
  auto stub = DecorateSpannerStub(mock, spanner::Database("foo", "bar", "baz"),
                                  std::move(auth), opts);
  EXPECT_NE(stub, nullptr);

  grpc::ClientContext context;
  OperationContext op_context(nullptr, 1, "CreateSession");
  auto session = stub->CreateSession(context, Options{}, {}, op_context);
  EXPECT_THAT(session, StatusIs(StatusCode::kAborted));

  EXPECT_THAT(span_catcher->GetSpans(), IsEmpty());
}

TEST(DecorateSpannerStub, TracingRequestId) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  EXPECT_CALL(*mock, CreateSession)
      .WillOnce([](auto& context, auto const&, auto const&, auto&) {
        testing_util::ValidatePropagator(context);
        return internal::AbortedError("fail");
      });
  auto opts = EnableTracing(
      Options{}.set<UnifiedCredentialsOption>(MakeInsecureCredentials()));
  internal::AutomaticallyCreatedBackgroundThreads background;
  auto auth = internal::CreateAuthenticationStrategy(background.cq(), opts);
  auto stub = DecorateSpannerStub(mock, spanner::Database("foo", "bar", "baz"),
                                  std::move(auth), opts);
  ASSERT_NE(stub, nullptr);

  grpc::ClientContext context;
  auto static_prefix = std::make_shared<std::string const>("1.abcd1234ef.3.");
  OperationContext op_context(static_prefix, 42, "CreateSession");
  op_context.BindChannel(2);
  op_context.PreCall(context);

  auto session = stub->CreateSession(context, Options{}, {}, op_context);
  EXPECT_THAT(session, StatusIs(StatusCode::kAborted));

  EXPECT_THAT(
      span_catcher->GetSpans(),
      ElementsAre(
          AllOf(SpanNamed("google.spanner.v1.Spanner/CreateSession"),
                testing_util::SpanHasAttributes(
                    testing_util::OTelAttribute<std::string>(
                        "gcp.spanner.request_id", "1.abcd1234ef.3.2.42.1")))));
}

TEST(SpannerTracingStub, CustomOpCtxFn) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  EXPECT_CALL(*mock, CreateSession)
      .WillOnce([](auto& context, auto const&, auto const&, auto&) {
        testing_util::ValidatePropagator(context);
        return internal::AbortedError("fail");
      });

  auto stub = MakeSpannerTracingStub(mock, [](opentelemetry::trace::Span& span,
                                              OperationContext const& op_ctx) {
    span.SetAttribute("test.rpc_name", std::string(op_ctx.rpc_name()));
  });

  grpc::ClientContext context;
  OperationContext op_context(nullptr, 1, "CreateSession");
  auto session = stub->CreateSession(context, Options{}, {}, op_context);
  EXPECT_THAT(session, StatusIs(StatusCode::kAborted));

  EXPECT_THAT(
      span_catcher->GetSpans(),
      ElementsAre(AllOf(SpanNamed("google.spanner.v1.Spanner/CreateSession"),
                        testing_util::SpanHasAttributes(
                            testing_util::OTelAttribute<std::string>(
                                "test.rpc_name", "CreateSession")))));
}

TEST(SpannerTracingStub, DefaultNoopOpCtxFn) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  EXPECT_CALL(*mock, CreateSession)
      .WillOnce([](auto& context, auto const&, auto const&, auto&) {
        testing_util::ValidatePropagator(context);
        return internal::AbortedError("fail");
      });

  auto stub = MakeSpannerTracingStub(mock);

  grpc::ClientContext context;
  auto static_prefix = std::make_shared<std::string const>("1.abcd1234ef.3.");
  OperationContext op_context(static_prefix, 42, "CreateSession");
  op_context.BindChannel(2);
  op_context.PreCall(context);

  auto session = stub->CreateSession(context, Options{}, {}, op_context);
  EXPECT_THAT(session, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, ElementsAre(AllOf(
                         SpanNamed("google.spanner.v1.Spanner/CreateSession"),
                         testing::Not(testing_util::SpanHasAttributes(
                             testing_util::OTelAttribute<std::string>(
                                 "gcp.spanner.request_id", testing::_))))));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
