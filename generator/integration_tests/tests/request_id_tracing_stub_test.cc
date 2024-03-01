// Copyright 2024 Google LLC
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

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#include "generator/integration_tests/golden/v1/internal/request_id_tracing_stub.h"
#include "generator/integration_tests/golden/v1/internal/request_id_connection_impl.h"
#include "generator/integration_tests/golden/v1/internal/request_id_option_defaults.h"
#include "generator/integration_tests/golden/v1/internal/request_id_stub.h"
#include "generator/integration_tests/tests/mock_request_id_stub.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_propagator.h"
#include <gmock/gmock.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace golden_v1 {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::golden_v1_testing::MockRequestIdServiceStub;
using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::OTelAttribute;
using ::google::cloud::testing_util::OTelContextCaptured;
using ::google::cloud::testing_util::SpanHasAttributes;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsClient;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::SpanWithStatus;
using ::google::cloud::testing_util::ThereIsAnActiveSpan;
using ::google::cloud::testing_util::ValidatePropagator;
using ::google::test::requestid::v1::CreateFooRequest;
using ::google::test::requestid::v1::Foo;
using ::google::test::requestid::v1::ListFoosRequest;
using ::google::test::requestid::v1::ListFoosResponse;
using ::google::test::requestid::v1::RenameFooRequest;
using ::testing::_;
using ::testing::ByMove;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::ResultOf;
using ::testing::Return;

Status TransientError() {
  return Status(StatusCode::kUnavailable, "try-again");
}

auto MakeTestConnection(
    std::shared_ptr<golden_v1_internal::RequestIdServiceStub> stub) {
  auto options = golden_v1_internal::RequestIdServiceDefaultOptions({});
  stub = MakeRequestIdServiceTracingStub(std::move(stub));
  auto background = internal::MakeBackgroundThreadsFactory(options)();
  return std::make_shared<golden_v1_internal::RequestIdServiceConnectionImpl>(
      std::move(background), std::move(stub), std::move(options));
}

TEST(RequestIdTracingStubTest, UnaryRpc) {
  auto span_catcher = InstallSpanCatcher();

  std::vector<std::string> captured_ids;
  auto mock = std::make_shared<MockRequestIdServiceStub>();
  EXPECT_CALL(*mock, CreateFoo)
      .WillOnce([&](auto& context, auto const&, auto const& request) {
        captured_ids.push_back(request.request_id());
        ValidatePropagator(context);
        EXPECT_TRUE(ThereIsAnActiveSpan());
        return StatusOr<Foo>(TransientError());
      })
      .WillOnce([&](auto& context, auto const&, auto const& request) {
        captured_ids.push_back(request.request_id());
        ValidatePropagator(context);
        EXPECT_TRUE(ThereIsAnActiveSpan());
        return make_status_or(Foo{});
      });

  auto connection = MakeTestConnection(mock);
  internal::OptionsSpan span(connection->options());
  CreateFooRequest request;
  auto result = connection->CreateFoo(request);
  EXPECT_STATUS_OK(result);
  ASSERT_THAT(captured_ids, ElementsAre(Not(IsEmpty()), Not(IsEmpty())));
  EXPECT_EQ(captured_ids[0], captured_ids[1]);

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(
          AllOf(
              SpanNamed("google.test.requestid.v1.RequestIdService/CreateFoo"),
              SpanHasAttributes(OTelAttribute<std::string>("gl-cpp.request_id",
                                                           captured_ids[0]))),
          AllOf(
              SpanNamed("google.test.requestid.v1.RequestIdService/CreateFoo"),
              SpanHasAttributes(OTelAttribute<std::string>("gl-cpp.request_id",
                                                           captured_ids[1])))));
}

TEST(RequestIdTracingStubTest, AsyncUnaryRpc) {
  auto span_catcher = InstallSpanCatcher();

  std::vector<std::string> captured_ids;
  auto mock = std::make_shared<MockRequestIdServiceStub>();
  EXPECT_CALL(*mock, AsyncCreateFoo)
      .WillOnce([&](auto, auto context, auto const&, auto const& request) {
        captured_ids.push_back(request.request_id());
        ValidatePropagator(*context);
        EXPECT_TRUE(ThereIsAnActiveSpan());
        return make_ready_future(StatusOr<Foo>(TransientError()));
      })
      .WillOnce([&](auto, auto context, auto const&, auto const& request) {
        captured_ids.push_back(request.request_id());
        ValidatePropagator(*context);
        EXPECT_TRUE(ThereIsAnActiveSpan());
        return make_ready_future(make_status_or(Foo{}));
      });

  auto connection = MakeTestConnection(mock);
  internal::OptionsSpan span(connection->options());
  CreateFooRequest request;
  auto result = connection->AsyncCreateFoo(request).get();
  EXPECT_STATUS_OK(result);
  ASSERT_THAT(captured_ids, ElementsAre(Not(IsEmpty()), Not(IsEmpty())));
  EXPECT_EQ(captured_ids[0], captured_ids[1]);

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(
          AllOf(
              SpanNamed("google.test.requestid.v1.RequestIdService/CreateFoo"),
              SpanHasAttributes(OTelAttribute<std::string>("gl-cpp.request_id",
                                                           captured_ids[0]))),
          AllOf(
              SpanNamed("google.test.requestid.v1.RequestIdService/CreateFoo"),
              SpanHasAttributes(OTelAttribute<std::string>("gl-cpp.request_id",
                                                           captured_ids[1])))));
}

TEST(RequestIdTracingStubTest, Lro) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockRequestIdServiceStub>();
  std::vector<std::string> captured_ids;
  EXPECT_CALL(*mock, AsyncRenameFoo)
      .WillOnce([&](auto, auto context, auto, auto const& request) {
        captured_ids.push_back(request.request_id());
        ValidatePropagator(*context);
        EXPECT_TRUE(ThereIsAnActiveSpan());
        return make_ready_future(
            StatusOr<google::longrunning::Operation>(TransientError()));
      })
      .WillOnce([&](auto, auto context, auto, auto const& request) {
        captured_ids.push_back(request.request_id());
        ValidatePropagator(*context);
        EXPECT_TRUE(ThereIsAnActiveSpan());
        return make_ready_future(
            make_status_or(google::longrunning::Operation{}));
      });
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([](auto&, auto context, auto, auto const&) {
        ValidatePropagator(*context);
        EXPECT_TRUE(ThereIsAnActiveSpan());
        google::longrunning::Operation result;
        result.set_done(true);
        result.mutable_response()->PackFrom(Foo{});
        return make_ready_future(make_status_or(std::move(result)));
      });

  auto connection = MakeTestConnection(mock);
  internal::OptionsSpan span(connection->options());
  RenameFooRequest request;
  auto result = connection->RenameFoo(request).get();
  EXPECT_STATUS_OK(result);
  ASSERT_THAT(captured_ids, ElementsAre(Not(IsEmpty()), Not(IsEmpty())));
  EXPECT_EQ(captured_ids[0], captured_ids[1]);

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(
          AllOf(
              SpanNamed("google.test.requestid.v1.RequestIdService/RenameFoo"),
              SpanHasAttributes(OTelAttribute<std::string>("gl-cpp.request_id",
                                                           captured_ids[0]))),
          AllOf(
              SpanNamed("google.test.requestid.v1.RequestIdService/RenameFoo"),
              SpanHasAttributes(OTelAttribute<std::string>("gl-cpp.request_id",
                                                           captured_ids[1]))),
          SpanNamed("google.longrunning.Operations/GetOperation")));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_v1
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
