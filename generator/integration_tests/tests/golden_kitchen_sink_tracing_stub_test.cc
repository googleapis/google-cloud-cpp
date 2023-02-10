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

#include "generator/integration_tests/golden/v1/internal/golden_kitchen_sink_tracing_stub.h"
#include "generator/integration_tests/tests/mock_golden_kitchen_sink_stub.h"
#include "google/cloud/internal/async_read_write_stream_impl.h"
#include "google/cloud/internal/async_streaming_read_rpc_impl.h"
#include "google/cloud/internal/async_streaming_write_rpc_impl.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/opentelemetry_options.h"
#include "google/cloud/internal/streaming_write_rpc_impl.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace golden_v1_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::Return;

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

using ::google::cloud::testing_util::InstallMockPropagator;
using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::SpanAttribute;
using ::google::cloud::testing_util::SpanHasAttributes;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsClient;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::SpanWithStatus;
using ::google::cloud::testing_util::ThereIsAnActiveSpan;
using ::google::test::admin::database::v1::Request;
using ::google::test::admin::database::v1::Response;
using ::testing::_;
using ::testing::ByMove;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::VariantWith;

auto constexpr kErrorCode = static_cast<int>(StatusCode::kAborted);

TEST(GoldenKitchenSinkTracingStubTest, GenerateAccessToken) {
  auto span_catcher = InstallSpanCatcher();
  auto mock_propagator = InstallMockPropagator();
  EXPECT_CALL(*mock_propagator, Inject);

  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  EXPECT_CALL(*mock, GenerateAccessToken).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return internal::AbortedError("fail");
  });

  auto under_test = GoldenKitchenSinkTracingStub(mock);
  grpc::ClientContext context;
  google::test::admin::database::v1::GenerateAccessTokenRequest request;
  auto result = under_test.GenerateAccessToken(context, request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("google.test.admin.database.v1.GoldenKitchenSink/"
                    "GenerateAccessToken"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              SpanAttribute<std::string>("grpc.peer", _),
              SpanAttribute<int>("gcloud.status_code", kErrorCode)))));
}

TEST(GoldenKitchenSinkTracingStubTest, GenerateIdToken) {
  auto span_catcher = InstallSpanCatcher();
  auto mock_propagator = InstallMockPropagator();
  EXPECT_CALL(*mock_propagator, Inject);

  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  EXPECT_CALL(*mock, GenerateIdToken).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return internal::AbortedError("fail");
  });

  auto under_test = GoldenKitchenSinkTracingStub(mock);
  grpc::ClientContext context;
  google::test::admin::database::v1::GenerateIdTokenRequest request;
  auto result = under_test.GenerateIdToken(context, request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("google.test.admin.database.v1.GoldenKitchenSink/"
                    "GenerateIdToken"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              SpanAttribute<std::string>("grpc.peer", _),
              SpanAttribute<int>("gcloud.status_code", kErrorCode)))));
}

TEST(GoldenKitchenSinkTracingStubTest, WriteLogEntries) {
  auto span_catcher = InstallSpanCatcher();
  auto mock_propagator = InstallMockPropagator();
  EXPECT_CALL(*mock_propagator, Inject);

  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  EXPECT_CALL(*mock, WriteLogEntries).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return internal::AbortedError("fail");
  });

  auto under_test = GoldenKitchenSinkTracingStub(mock);
  grpc::ClientContext context;
  google::test::admin::database::v1::WriteLogEntriesRequest request;
  auto result = under_test.WriteLogEntries(context, request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("google.test.admin.database.v1.GoldenKitchenSink/"
                    "WriteLogEntries"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              SpanAttribute<std::string>("grpc.peer", _),
              SpanAttribute<int>("gcloud.status_code", kErrorCode)))));
}

TEST(GoldenKitchenSinkTracingStubTest, ListLogs) {
  auto span_catcher = InstallSpanCatcher();
  auto mock_propagator = InstallMockPropagator();
  EXPECT_CALL(*mock_propagator, Inject);

  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  EXPECT_CALL(*mock, ListLogs).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return internal::AbortedError("fail");
  });

  auto under_test = GoldenKitchenSinkTracingStub(mock);
  grpc::ClientContext context;
  google::test::admin::database::v1::ListLogsRequest request;
  auto result = under_test.ListLogs(context, request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("google.test.admin.database.v1.GoldenKitchenSink/ListLogs"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              SpanAttribute<std::string>("grpc.peer", _),
              SpanAttribute<int>("gcloud.status_code", kErrorCode)))));
}

TEST(GoldenKitchenSinkTracingStubTest, StreamingRead) {
  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  using ErrorStream =
      ::google::cloud::internal::StreamingReadRpcError<Response>;
  EXPECT_CALL(*mock, StreamingRead)
      .WillOnce(Return(ByMove(
          absl::make_unique<ErrorStream>(internal::AbortedError("fail")))));

  auto under_test = GoldenKitchenSinkTracingStub(mock);
  auto stream = under_test.StreamingRead(
      absl::make_unique<grpc::ClientContext>(), Request{});
  auto v = stream->Read();
  EXPECT_THAT(v, VariantWith<Status>(StatusIs(StatusCode::kAborted)));
}

TEST(GoldenKitchenSinkTracingStubTest, ListServiceAccountKeys) {
  auto span_catcher = InstallSpanCatcher();
  auto mock_propagator = InstallMockPropagator();
  EXPECT_CALL(*mock_propagator, Inject);

  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  EXPECT_CALL(*mock, ListServiceAccountKeys).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return internal::AbortedError("fail");
  });

  auto under_test = GoldenKitchenSinkTracingStub(mock);
  grpc::ClientContext context;
  google::test::admin::database::v1::ListServiceAccountKeysRequest request;
  auto result = under_test.ListServiceAccountKeys(context, request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("google.test.admin.database.v1.GoldenKitchenSink/"
                    "ListServiceAccountKeys"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              SpanAttribute<std::string>("grpc.peer", _),
              SpanAttribute<int>("gcloud.status_code", kErrorCode)))));
}

TEST(GoldenKitchenSinkTracingStubTest, DoNothing) {
  auto span_catcher = InstallSpanCatcher();
  auto mock_propagator = InstallMockPropagator();
  EXPECT_CALL(*mock_propagator, Inject);

  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  EXPECT_CALL(*mock, DoNothing).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return internal::AbortedError("fail");
  });

  auto under_test = GoldenKitchenSinkTracingStub(mock);
  grpc::ClientContext context;
  google::protobuf::Empty request;
  auto result = under_test.DoNothing(context, request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed(
              "google.test.admin.database.v1.GoldenKitchenSink/DoNothing"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              SpanAttribute<std::string>("grpc.peer", _),
              SpanAttribute<int>("gcloud.status_code", kErrorCode)))));
}

TEST(GoldenKitchenSinkTracingStubTest, StreamingWrite) {
  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  EXPECT_CALL(*mock, StreamingWrite)
      .WillOnce([](std::unique_ptr<grpc::ClientContext>) {
        auto stream = absl::make_unique<MockStreamingWriteRpc>();
        EXPECT_CALL(*stream, Write).WillOnce(Return(false));
        EXPECT_CALL(*stream, Close)
            .WillOnce(
                Return(StatusOr<Response>(internal::AbortedError("fail"))));
        return stream;
      });

  auto under_test = GoldenKitchenSinkTracingStub(mock);
  auto stream =
      under_test.StreamingWrite(absl::make_unique<grpc::ClientContext>());
  EXPECT_FALSE(stream->Write(Request{}, grpc::WriteOptions()));
  auto response = stream->Close();
  EXPECT_THAT(response, StatusIs(StatusCode::kAborted));
}

TEST(GoldenKitchenSinkTracingStubTest, AsyncStreamingRead) {
  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  using ErrorStream =
      ::google::cloud::internal::AsyncStreamingReadRpcError<Response>;
  EXPECT_CALL(*mock, AsyncStreamingRead)
      .WillOnce(Return(ByMove(
          absl::make_unique<ErrorStream>(internal::AbortedError("fail")))));

  google::cloud::CompletionQueue cq;
  auto under_test = GoldenKitchenSinkTracingStub(mock);
  auto stream = under_test.AsyncStreamingRead(
      cq, absl::make_unique<grpc::ClientContext>(), Request{});
  auto start = stream->Start().get();
  EXPECT_FALSE(start);
  auto finish = stream->Finish().get();
  EXPECT_THAT(finish, StatusIs(StatusCode::kAborted));
}

TEST(GoldenKitchenSinkTracingStubTest, AsyncStreamingWrite) {
  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  using ErrorStream =
      ::google::cloud::internal::AsyncStreamingWriteRpcError<Request, Response>;
  EXPECT_CALL(*mock, AsyncStreamingWrite)
      .WillOnce(Return(ByMove(
          absl::make_unique<ErrorStream>(internal::AbortedError("fail")))));

  google::cloud::CompletionQueue cq;
  auto under_test = GoldenKitchenSinkTracingStub(mock);

  auto stream = under_test.AsyncStreamingWrite(
      cq, absl::make_unique<grpc::ClientContext>());
  auto start = stream->Start().get();
  EXPECT_FALSE(start);
  auto finish = stream->Finish().get();
  EXPECT_THAT(finish, StatusIs(StatusCode::kAborted));
}

TEST(GoldenKitchenSinkTracingStubTest, AsyncStreamingReadWrite) {
  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  using ErrorStream =
      ::google::cloud::internal::AsyncStreamingReadWriteRpcError<Request,
                                                                 Response>;
  EXPECT_CALL(*mock, AsyncStreamingReadWrite)
      .WillOnce(Return(ByMove(
          absl::make_unique<ErrorStream>(internal::AbortedError("fail")))));

  google::cloud::CompletionQueue cq;
  auto under_test = GoldenKitchenSinkTracingStub(mock);

  auto stream = under_test.AsyncStreamingReadWrite(
      cq, absl::make_unique<grpc::ClientContext>());
  auto start = stream->Start().get();
  EXPECT_FALSE(start);
  auto finish = stream->Finish().get();
  EXPECT_THAT(finish, StatusIs(StatusCode::kAborted));
}

TEST(GoldenKitchenSinkTracingStubTest, ExplicitRouting1) {
  auto span_catcher = InstallSpanCatcher();
  auto mock_propagator = InstallMockPropagator();
  EXPECT_CALL(*mock_propagator, Inject);

  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  EXPECT_CALL(*mock, ExplicitRouting1).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return internal::AbortedError("fail");
  });

  auto under_test = GoldenKitchenSinkTracingStub(mock);
  grpc::ClientContext context;
  google::test::admin::database::v1::ExplicitRoutingRequest request;
  auto result = under_test.ExplicitRouting1(context, request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("google.test.admin.database.v1.GoldenKitchenSink/"
                    "ExplicitRouting1"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              SpanAttribute<std::string>("grpc.peer", _),
              SpanAttribute<int>("gcloud.status_code", kErrorCode)))));
}

TEST(GoldenKitchenSinkTracingStubTest, ExplicitRouting2) {
  auto span_catcher = InstallSpanCatcher();
  auto mock_propagator = InstallMockPropagator();
  EXPECT_CALL(*mock_propagator, Inject);

  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  EXPECT_CALL(*mock, ExplicitRouting2).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return internal::AbortedError("fail");
  });

  auto under_test = GoldenKitchenSinkTracingStub(mock);
  grpc::ClientContext context;
  google::test::admin::database::v1::ExplicitRoutingRequest request;
  auto result = under_test.ExplicitRouting2(context, request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("google.test.admin.database.v1.GoldenKitchenSink/"
                    "ExplicitRouting2"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              SpanAttribute<std::string>("grpc.peer", _),
              SpanAttribute<int>("gcloud.status_code", kErrorCode)))));
}

TEST(MakeGoldenKitchenSinkTracingStub, OpenTelemetry) {
  auto span_catcher = InstallSpanCatcher();
  auto mock_propagator = InstallMockPropagator();
  EXPECT_CALL(*mock_propagator, Inject);

  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  EXPECT_CALL(*mock, DoNothing)
      .WillOnce(Return(internal::AbortedError("fail")));

  auto under_test = MakeGoldenKitchenSinkTracingStub(mock);
  grpc::ClientContext context;
  auto result = under_test->DoNothing(context, {});
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, Not(IsEmpty()));
}

#else

TEST(MakeGoldenKitchenSinkTracingStub, NoOpenTelemetry) {
  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  EXPECT_CALL(*mock, DoNothing)
      .WillOnce(Return(internal::AbortedError("fail")));

  auto under_test = MakeGoldenKitchenSinkTracingStub(mock);
  grpc::ClientContext context;
  auto result = under_test->DoNothing(context, {});
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_v1_internal
}  // namespace cloud
}  // namespace google
