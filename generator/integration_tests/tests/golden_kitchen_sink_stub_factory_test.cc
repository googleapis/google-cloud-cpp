// Copyright 2020 Google LLC
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

#include "generator/integration_tests/golden/v1/internal/golden_kitchen_sink_stub_factory.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/testing_util/mock_grpc_authentication_strategy.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_v1_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::MakeStubFactoryMockAuth;
using ::google::cloud::testing_util::ScopedLog;
using ::google::cloud::testing_util::StatusIs;
using ::google::test::admin::database::v1::GenerateIdTokenRequest;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Return;

TEST(GoldenKitchenSinkStubFactoryTest, DefaultStubWithoutLogging) {
  ScopedLog log;

  Options options;
  auto auth = MakeStubFactoryMockAuth();
  auto default_stub = CreateDefaultGoldenKitchenSinkStub(auth, options);
  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, IsEmpty());
}

TEST(GoldenKitchenSinkStubFactoryTest, DefaultStubWithLogging) {
  ScopedLog log;

  Options options;
  options.set<LoggingComponentsOption>({"rpc"});
  auto auth = MakeStubFactoryMockAuth();
  auto default_stub = CreateDefaultGoldenKitchenSinkStub(auth, options);
  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("Enabled logging for gRPC calls")));
}

TEST(GoldenKitchenSinkStubFactoryTest, DefaultStubWithAuth) {
  Options options;
  auto auth = MakeStubFactoryMockAuth();
  EXPECT_CALL(*auth, RequiresConfigureContext).WillOnce(Return(true));
  EXPECT_CALL(*auth, ConfigureContext)
      .WillOnce(Return(internal::AbortedError("fail")));
  auto default_stub = CreateDefaultGoldenKitchenSinkStub(auth, options);
  grpc::ClientContext context;
  auto response =
      default_stub->GenerateIdToken(context, options, GenerateIdTokenRequest{});
  EXPECT_THAT(response, StatusIs(StatusCode::kAborted, "fail"));
}

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
using ::google::cloud::testing_util::DisableTracing;
using ::google::cloud::testing_util::EnableTracing;
using ::google::cloud::testing_util::SpanNamed;
using ::testing::Not;

TEST(GoldenKitchenSinkStubFactoryTest, DefaultStubWithTracingEnabled) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto options = EnableTracing(Options{}.set<EndpointOption>("localhost:1"));
  auto auth = MakeStubFactoryMockAuth();
  auto stub = CreateDefaultGoldenKitchenSinkStub(auth, options);
  grpc::ClientContext context;
  (void)stub->DoNothing(context, options, {});

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans, Contains(SpanNamed(
                 "google.test.admin.database.v1.GoldenKitchenSink/DoNothing")));
}

TEST(GoldenKitchenSinkStubFactoryTest, DefaultStubWithTracingDisabled) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto options = DisableTracing(Options{}.set<EndpointOption>("localhost:1"));
  auto auth = MakeStubFactoryMockAuth();
  auto stub = CreateDefaultGoldenKitchenSinkStub(auth, options);
  grpc::ClientContext context;
  (void)stub->DoNothing(context, options, {});

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      Not(Contains(SpanNamed(
          "google.test.admin.database.v1.GoldenKitchenSink/DoNothing"))));
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_v1_internal
}  // namespace cloud
}  // namespace google
