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

#include "google/cloud/internal/grpc_opentelemetry.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/trace_propagator.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include "google/cloud/testing_util/validate_propagator.h"
#include <gmock/gmock.h>
#include <grpcpp/grpcpp.h>
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include <opentelemetry/context/propagation/global_propagator.h>
#include <opentelemetry/trace/semantic_conventions.h>
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::testing::ByMove;
using ::testing::Return;

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
using ::google::cloud::testing_util::DisableTracing;
using ::google::cloud::testing_util::EnableTracing;
using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::OTelAttribute;
using ::google::cloud::testing_util::SetServerMetadata;
using ::google::cloud::testing_util::SpanHasAttributes;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsClient;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::SpanWithParent;
using ::google::cloud::testing_util::SpanWithStatus;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;

TEST(OpenTelemetry, MakeSpanGrpc) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();

  auto span = MakeSpanGrpc("google.cloud.foo.v1.Foo", "GetBar");
  span->End();

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("google.cloud.foo.v1.Foo/GetBar"),
          SpanHasAttributes(
              OTelAttribute<std::string>(sc::kRpcSystem,
                                         sc::RpcSystemValues::kGrpc),
              OTelAttribute<std::string>(sc::kRpcService,
                                         "google.cloud.foo.v1.Foo"),
              OTelAttribute<std::string>(sc::kRpcMethod, "GetBar"),
              OTelAttribute<std::string>(
                  /*sc::kNetworkTransport=*/"network.transport",
                  sc::NetTransportValues::kIpTcp),
              OTelAttribute<std::string>("grpc.version", grpc::Version())))));
}

TEST(OpenTelemetry, MakeSpanGrpcHandlesNonNullTerminatedStringView) {
  auto span_catcher = InstallSpanCatcher();

  char service[]{'s', 'e', 'r', 'v', 'i', 'c', 'e'};
  char method[]{'m', 'e', 't', 'h', 'o', 'd'};

  auto span = MakeSpanGrpc({service, 7}, {method, 6});
  span->End();

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, ElementsAre(SpanNamed("service/method")));
}

TEST(OpenTelemetry, InjectTraceContextGrpc) {
  auto span_catcher = InstallSpanCatcher();

  opentelemetry::trace::Scope scope(
      MakeSpanGrpc("google.cloud.foo.v1.Foo", "GetBar"));

  grpc::ClientContext context;
  auto propagator = MakePropagator();
  InjectTraceContext(context, *propagator);
  testing_util::ValidatePropagator(context);
}

TEST(OpenTelemetry, EndSpan) {
  auto span_catcher = InstallSpanCatcher();

  grpc::ClientContext context;
  SetServerMetadata(
      context,
      RpcMetadata{{{"header1", "value1"}, {"header2", "value2"}},
                  {{"trailer", "value3"}, {"trailer-bin", "\x09\x85\x75"}}});

  auto span = MakeSpanGrpc("google.cloud.foo.v1.Foo", "GetBar");
  auto status = EndSpan(context, *span, Status());
  EXPECT_STATUS_OK(status);

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
          SpanHasAttributes(
              OTelAttribute<std::string>("rpc.grpc.response.metadata.header1",
                                         "value1"),
              OTelAttribute<std::string>("rpc.grpc.response.metadata.header2",
                                         "value2"),
              OTelAttribute<std::string>("rpc.grpc.response.metadata.trailer",
                                         "value3"),
              OTelAttribute<std::string>(
                  "rpc.grpc.response.metadata.trailer-bin", R"(\x09\x85\x75)"),
              OTelAttribute<std::string>("grpc.compression_algorithm",
                                         "identity"),
              // It is too hard to mock a `grpc::ClientContext`. We will
              // just check that the expected attribute key is set.
              OTelAttribute<std::string>("grpc.peer", _)))));
}

TEST(OpenTelemetry, EndSpanFuture) {
  auto span_catcher = InstallSpanCatcher();

  promise<Status> p;
  auto context = std::make_shared<grpc::ClientContext>();
  SetServerMetadata(*context, RpcMetadata{});
  auto f = EndSpan(context, MakeSpanGrpc("google.cloud.foo.v1.Foo", "GetBar"),
                   p.get_future());
  EXPECT_FALSE(f.is_ready());
  p.set_value(Status());
  EXPECT_TRUE(f.is_ready());
  EXPECT_STATUS_OK(f.get());

  auto spans = span_catcher->GetSpans();
  // It is too hard to mock a `grpc::ClientContext`. We will just check that the
  // expected attribute key is set.
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
          SpanHasAttributes(OTelAttribute<std::string>("grpc.peer", _)))));
}

TEST(OpenTelemetry, EndSpanFutureDetachesContext) {
  auto span_catcher = InstallSpanCatcher();

  // Set the `OTelContext` like we do in `AsyncOperation`s
  auto c = opentelemetry::context::Context("key", true);
  ScopedOTelContext scope({c});
  EXPECT_EQ(c, opentelemetry::context::RuntimeContext::GetCurrent());

  promise<Status> p;
  auto context = std::make_shared<grpc::ClientContext>();
  SetServerMetadata(*context, RpcMetadata{});
  auto f =
      EndSpan(context, MakeSpanGrpc("google.cloud.foo.v1.Foo", "GetBar"),
              p.get_future())
          .then([](auto f) {
            auto s = f.get();
            // The `OTelContext` should be cleared by the time we exit
            // `EndSpan()`.
            EXPECT_EQ(opentelemetry::context::Context{},
                      opentelemetry::context::RuntimeContext::GetCurrent());
            return s;
          });

  p.set_value(Status());
  EXPECT_STATUS_OK(f.get());
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(SpanNamed("google.cloud.foo.v1.Foo/GetBar")));
}

class EndSpanGrpcServerMetadataTest : public ::testing::Test {
 protected:
  testing_util::ValidateMetadataFixture metadata_fixture_;
};

TEST_F(EndSpanGrpcServerMetadataTest, UnknownOriginError) {
  auto span_catcher = InstallSpanCatcher();

  promise<Status> p;
  auto context = std::make_shared<grpc::ClientContext>();
  metadata_fixture_.SetServerMetadata(
      *context, {{{"header", "header-value"}}, {{"trailer", "trailer-value"}}});
  auto f = EndSpan(context, MakeSpanGrpc("google.cloud.foo.v1.Foo", "GetBar"),
                   p.get_future());
  EXPECT_FALSE(f.is_ready());
  p.set_value(internal::CancelledError("call cancelled"));
  EXPECT_TRUE(f.is_ready());
  auto status = f.get();
  EXPECT_THAT(status, StatusIs(StatusCode::kCancelled));

  EXPECT_THAT(
      span_catcher->GetSpans(),
      ElementsAre(AllOf(
          SpanWithStatus(opentelemetry::trace::StatusCode::kError),
          SpanHasAttributes(
              OTelAttribute<std::string>("grpc.peer", "inproc"),
              OTelAttribute<std::string>("rpc.grpc.response.metadata.header",
                                         "header-value"),
              OTelAttribute<std::string>("rpc.grpc.response.metadata.trailer",
                                         "trailer-value")))));
}

TEST_F(EndSpanGrpcServerMetadataTest, ClientOrigin) {
  auto span_catcher = InstallSpanCatcher();

  promise<Status> p;
  auto context = std::make_shared<grpc::ClientContext>();
  metadata_fixture_.SetServerMetadata(
      *context, {{{"header", "header-value"}}, {{"trailer", "trailer-value"}}});
  auto f = EndSpan(context, MakeSpanGrpc("google.cloud.foo.v1.Foo", "GetBar"),
                   p.get_future());
  EXPECT_FALSE(f.is_ready());
  p.set_value(internal::CancelledError(
      "call cancelled",
      GCP_ERROR_INFO().WithMetadata("gl-cpp.error.origin", "client")));
  EXPECT_TRUE(f.is_ready());
  auto status = f.get();
  EXPECT_THAT(status, StatusIs(StatusCode::kCancelled));
  auto const& metadata = status.error_info().metadata();
  EXPECT_THAT(metadata, Contains(Pair("gl-cpp.error.origin", "client")));

  EXPECT_THAT(
      span_catcher->GetSpans(),
      ElementsAre(AllOf(
          SpanWithStatus(opentelemetry::trace::StatusCode::kError),
          SpanHasAttributes(OTelAttribute<std::string>("grpc.peer", "inproc")),
          Not(SpanHasAttributes(
              OTelAttribute<std::string>("grpc.peer", "inproc"),
              OTelAttribute<std::string>("rpc.grpc.response.metadata.header",
                                         "header-value"),
              OTelAttribute<std::string>("rpc.grpc.response.metadata.trailer",
                                         "trailer-value"))))));
}

TEST(OpenTelemetry, TracedAsyncBackoffEnabled) {
  auto span_catcher = InstallSpanCatcher();

  auto const duration = std::chrono::nanoseconds(100);
  auto mock_cq = std::make_shared<testing_util::MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer(duration))
      .WillOnce(Return(ByMove(make_ready_future(
          make_status_or(std::chrono::system_clock::now())))));
  CompletionQueue cq(mock_cq);

  auto f = TracedAsyncBackoff(cq, EnableTracing(Options{}), duration,
                              "Async Backoff");
  EXPECT_STATUS_OK(f.get());

  // Verify that a span was made.
  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans, ElementsAre(
                 AllOf(SpanNamed("Async Backoff"),
                       SpanWithStatus(opentelemetry::trace::StatusCode::kOk))));
}

TEST(OpenTelemetry, TracedAsyncBackoffDisabled) {
  auto span_catcher = InstallSpanCatcher();

  auto const duration = std::chrono::nanoseconds(100);
  auto mock_cq = std::make_shared<testing_util::MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer(duration))
      .WillOnce(Return(ByMove(
          make_ready_future<StatusOr<std::chrono::system_clock::time_point>>(
              CancelledError("cancelled")))));
  CompletionQueue cq(mock_cq);

  auto f = TracedAsyncBackoff(cq, DisableTracing(Options{}), duration,
                              "Async Backoff");
  EXPECT_THAT(f.get(), StatusIs(StatusCode::kCancelled, "cancelled"));

  // Verify that no spans were made.
  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, IsEmpty());
}

TEST(OpenTelemetry, TracedAsyncBackoffPreservesContext) {
  auto span_catcher = InstallSpanCatcher();
  auto parent = MakeSpan("parent");

  OTelScope scope(parent);
  ASSERT_THAT(CurrentOTelContext(), Not(IsEmpty()));
  auto oc = CurrentOTelContext().back();

  auto const duration = std::chrono::nanoseconds(100);
  auto mock_cq = std::make_shared<testing_util::MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer(duration))
      .WillOnce(Return(ByMove(make_ready_future(
          make_status_or(std::chrono::system_clock::now())))));

  CompletionQueue cq(mock_cq);

  auto f = TracedAsyncBackoff(cq, EnableTracing(Options{}), duration,
                              "Async Backoff");
  EXPECT_STATUS_OK(f.get());

  EXPECT_THAT(CurrentOTelContext(), ElementsAre(oc));
  parent->End();

  EXPECT_THAT(
      span_catcher->GetSpans(),
      ElementsAre(AllOf(SpanNamed("Async Backoff"), SpanWithParent(parent)),
                  SpanNamed("parent")));
}

#else

TEST(NoOpenTelemetry, TracedAsyncBackoff) {
  auto const duration = std::chrono::nanoseconds(100);
  auto mock_cq = std::make_shared<testing_util::MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer(duration))
      .WillOnce(Return(ByMove(make_ready_future(
          make_status_or(std::chrono::system_clock::now())))));
  CompletionQueue cq(mock_cq);

  auto f = TracedAsyncBackoff(cq, Options{}, duration, "Async Backoff");
  EXPECT_STATUS_OK(f.get());
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
