// Copyright 2023 Google LLC
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

#include "generator/integration_tests/golden/v1/internal/golden_kitchen_sink_tracing_connection.h"
#include "generator/integration_tests/golden/v1/mocks/mock_golden_kitchen_sink_connection.h"
#include "generator/integration_tests/test.pb.h"
#include "google/cloud/mocks/mock_stream_range.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_v1_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::golden_v1_mocks::MockGoldenKitchenSinkConnection;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Return;

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
using ::google::cloud::testing_util::DisableTracing;
using ::google::cloud::testing_util::EnableTracing;
using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::OTelAttribute;
using ::google::cloud::testing_util::OTelContextCaptured;
using ::google::cloud::testing_util::SpanHasAttributes;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsClient;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::SpanWithStatus;
using ::google::cloud::testing_util::ThereIsAnActiveSpan;
using ::google::test::admin::database::v1::Request;
using ::google::test::admin::database::v1::Response;
using ::testing::AllOf;
using ::testing::ByMove;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Not;

auto constexpr kErrorCode = "ABORTED";

TEST(GoldenKitchenSinkTracingConnectionTest, Options) {
  struct TestOption {
    using Type = int;
  };

  auto mock = std::make_shared<MockGoldenKitchenSinkConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(Options{}.set<TestOption>(5)));

  auto under_test = GoldenKitchenSinkTracingConnection(mock);
  auto options = under_test.options();
  EXPECT_EQ(5, options.get<TestOption>());
}

TEST(GoldenKitchenSinkTracingConnectionTest, GenerateAccessToken) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenKitchenSinkConnection>();
  EXPECT_CALL(*mock, GenerateAccessToken).WillOnce([]() {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return internal::AbortedError("fail");
  });

  auto under_test = GoldenKitchenSinkTracingConnection(mock);
  google::test::admin::database::v1::GenerateAccessTokenRequest request;
  auto result = under_test.GenerateAccessToken(request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed(
              "golden_v1::GoldenKitchenSinkConnection::GenerateAccessToken"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<std::string>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenKitchenSinkTracingConnectionTest, GenerateIdToken) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenKitchenSinkConnection>();
  EXPECT_CALL(*mock, GenerateIdToken).WillOnce([]() {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return internal::AbortedError("fail");
  });

  auto under_test = GoldenKitchenSinkTracingConnection(mock);
  google::test::admin::database::v1::GenerateIdTokenRequest request;
  auto result = under_test.GenerateIdToken(request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("golden_v1::GoldenKitchenSinkConnection::GenerateIdToken"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<std::string>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenKitchenSinkTracingConnectionTest, WriteLogEntries) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenKitchenSinkConnection>();
  EXPECT_CALL(*mock, WriteLogEntries).WillOnce([]() {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return internal::AbortedError("fail");
  });

  auto under_test = GoldenKitchenSinkTracingConnection(mock);
  google::test::admin::database::v1::WriteLogEntriesRequest request;
  auto result = under_test.WriteLogEntries(request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("golden_v1::GoldenKitchenSinkConnection::WriteLogEntries"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<std::string>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenKitchenSinkTracingConnectionTest, ListLogs) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenKitchenSinkConnection>();
  EXPECT_CALL(*mock, ListLogs).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    EXPECT_TRUE(OTelContextCaptured());
    return mocks::MakeStreamRange<std::string>({},
                                               internal::AbortedError("fail"));
  });

  auto under_test = GoldenKitchenSinkTracingConnection(mock);
  google::test::admin::database::v1::ListLogsRequest request;
  auto stream = under_test.ListLogs(request);
  auto it = stream.begin();
  ASSERT_FALSE(*it);
  EXPECT_THAT(*it, StatusIs(StatusCode::kAborted));
  EXPECT_EQ(++it, stream.end());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("golden_v1::GoldenKitchenSinkConnection::ListLogs"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<std::string>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenKitchenSinkTracingConnectionTest, ListServiceAccountKeys) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenKitchenSinkConnection>();
  EXPECT_CALL(*mock, ListServiceAccountKeys).WillOnce([]() {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return internal::AbortedError("fail");
  });

  auto under_test = GoldenKitchenSinkTracingConnection(mock);
  google::test::admin::database::v1::ListServiceAccountKeysRequest request;
  auto result = under_test.ListServiceAccountKeys(request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed(
              "golden_v1::GoldenKitchenSinkConnection::ListServiceAccountKeys"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<std::string>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenKitchenSinkTracingConnectionTest, DoNothing) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenKitchenSinkConnection>();
  EXPECT_CALL(*mock, DoNothing).WillOnce([]() {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return internal::AbortedError("fail");
  });

  auto under_test = GoldenKitchenSinkTracingConnection(mock);
  google::protobuf::Empty request;
  auto result = under_test.DoNothing(request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("golden_v1::GoldenKitchenSinkConnection::DoNothing"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<std::string>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenKitchenSinkTracingConnectionTest, Deprecated2) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenKitchenSinkConnection>();
  EXPECT_CALL(*mock, Deprecated2).WillOnce([]() {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return internal::AbortedError("fail");
  });

  auto under_test = GoldenKitchenSinkTracingConnection(mock);
  google::test::admin::database::v1::GenerateAccessTokenRequest request;
  auto result = under_test.Deprecated2(request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("golden_v1::GoldenKitchenSinkConnection::Deprecated2"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<std::string>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenKitchenSinkTracingConnectionTest, StreamingRead) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenKitchenSinkConnection>();
  EXPECT_CALL(*mock, StreamingRead).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    EXPECT_TRUE(OTelContextCaptured());
    return mocks::MakeStreamRange<Response>({}, internal::AbortedError("fail"));
  });

  auto under_test = GoldenKitchenSinkTracingConnection(mock);
  auto stream = under_test.StreamingRead(Request{});
  auto it = stream.begin();
  ASSERT_FALSE(*it);
  EXPECT_THAT(*it, StatusIs(StatusCode::kAborted));
  EXPECT_EQ(++it, stream.end());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("golden_v1::GoldenKitchenSinkConnection::StreamingRead"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<std::string>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenKitchenSinkTracingConnectionTest, AsyncStreamingReadWrite) {
  auto mock = std::make_shared<MockGoldenKitchenSinkConnection>();
  using ErrorStream =
      ::google::cloud::internal::AsyncStreamingReadWriteRpcError<Request,
                                                                 Response>;
  EXPECT_CALL(*mock, AsyncStreamingReadWrite)
      .WillOnce(Return(ByMove(
          std::make_unique<ErrorStream>(internal::AbortedError("fail")))));

  auto under_test = GoldenKitchenSinkTracingConnection(mock);
  auto stream = under_test.AsyncStreamingReadWrite();
  auto start = stream->Start().get();
  EXPECT_FALSE(start);
  auto finish = stream->Finish().get();
  EXPECT_THAT(finish, StatusIs(StatusCode::kAborted));
}

TEST(GoldenKitchenSinkTracingConnectionTest, ExplicitRouting1) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenKitchenSinkConnection>();
  EXPECT_CALL(*mock, ExplicitRouting1).WillOnce([]() {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return internal::AbortedError("fail");
  });

  auto under_test = GoldenKitchenSinkTracingConnection(mock);
  google::test::admin::database::v1::ExplicitRoutingRequest request;
  auto result = under_test.ExplicitRouting1(request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("golden_v1::GoldenKitchenSinkConnection::ExplicitRouting1"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<std::string>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenKitchenSinkTracingConnectionTest, ExplicitRouting2) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenKitchenSinkConnection>();
  EXPECT_CALL(*mock, ExplicitRouting2).WillOnce([]() {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return internal::AbortedError("fail");
  });

  auto under_test = GoldenKitchenSinkTracingConnection(mock);
  google::test::admin::database::v1::ExplicitRoutingRequest request;
  auto result = under_test.ExplicitRouting2(request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("golden_v1::GoldenKitchenSinkConnection::ExplicitRouting2"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<std::string>("gl-cpp.status_code", kErrorCode)))));
}

TEST(MakeGoldenKitchenSinkTracingConnection, TracingEnabled) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenKitchenSinkConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(EnableTracing(Options{})));
  EXPECT_CALL(*mock, DoNothing)
      .WillOnce(Return(internal::AbortedError("fail")));

  auto under_test = MakeGoldenKitchenSinkTracingConnection(mock);
  auto result = under_test->DoNothing({});
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, Not(IsEmpty()));
}

TEST(MakeGoldenKitchenSinkTracingConnection, TracingDisabled) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenKitchenSinkConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(DisableTracing(Options{})));
  EXPECT_CALL(*mock, DoNothing)
      .WillOnce(Return(internal::AbortedError("fail")));

  auto under_test = MakeGoldenKitchenSinkTracingConnection(mock);
  auto result = under_test->DoNothing({});
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, IsEmpty());
}

#else

TEST(MakeGoldenKitchenSinkTracingConnection, NoOpenTelemetry) {
  auto mock = std::make_shared<MockGoldenKitchenSinkConnection>();
  EXPECT_CALL(*mock, DoNothing)
      .WillOnce(Return(internal::AbortedError("fail")));

  auto under_test = MakeGoldenKitchenSinkTracingConnection(mock);
  auto result = under_test->DoNothing({});
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_v1_internal
}  // namespace cloud
}  // namespace google
