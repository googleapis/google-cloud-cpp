// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#include "google/cloud/storage/internal/async/reader_connection_tracing.h"
#include "google/cloud/storage/mocks/mock_async_reader_connection.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/opentelemetry_options.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include <gmock/gmock.h>
#include <opentelemetry/trace/semantic_conventions.h>
#include <cstdint>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage_experimental::ReadPayload;
using ReadResponse =
    ::google::cloud::storage_experimental::AsyncReaderConnection::ReadResponse;
using ::google::cloud::storage_mocks::MockAsyncReaderConnection;
using ::google::cloud::testing_util::EventNamed;
using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::OTelAttribute;
using ::google::cloud::testing_util::OTelContextCaptured;
using ::google::cloud::testing_util::PromiseWithOTelContext;
using ::google::cloud::testing_util::SpanEventAttributesAre;
using ::google::cloud::testing_util::SpanHasAttributes;
using ::google::cloud::testing_util::SpanHasEvents;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsClient;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::SpanWithStatus;
using ::google::cloud::testing_util::ThereIsAnActiveSpan;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Pair;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

auto expect_context = [](auto& p) {
  return [&p] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    EXPECT_TRUE(OTelContextCaptured());
    return p.get_future();
  };
};

auto expect_no_context = [](auto f) {
  auto t = f.get();
  EXPECT_FALSE(ThereIsAnActiveSpan());
  EXPECT_FALSE(OTelContextCaptured());
  return t;
};

TEST(ReaderConnectionTracing, WithError) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();
  PromiseWithOTelContext<ReadResponse> p1;
  PromiseWithOTelContext<ReadResponse> p2;

  auto mock = std::make_unique<MockAsyncReaderConnection>();
  EXPECT_CALL(*mock, Read)
      .WillOnce(expect_context(p1))
      .WillOnce(expect_context(p2));
  auto actual = MakeTracingReaderConnection(
      internal::MakeSpan("test-span-name"), std::move(mock));

  auto f1 = actual->Read().then(expect_no_context);
  p1.set_value(ReadResponse(ReadPayload("m1")));
  (void)f1.get();

  auto f2 = actual->Read().then(expect_no_context);
  p2.set_value(ReadResponse(PermanentError()));
  (void)f2.get();

  auto spans = span_catcher->GetSpans();
  auto const expected_code = static_cast<std::int32_t>(PermanentError().code());
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanNamed("test-span-name"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError,
                         PermanentError().message()),
          SpanHasAttributes(
              OTelAttribute<std::int32_t>("gl-cpp.status_code", expected_code)),
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanHasEvents(
              AllOf(
                  EventNamed("gl-cpp.read"),
                  SpanEventAttributesAre(
                      OTelAttribute<std::int64_t>(
                          /*sc::kRpcMessageId=*/"rpc.message.id", 1),
                      OTelAttribute<std::string>(
                          /*sc::kRpcMessageType=*/"rpc.message.type",
                          "RECEIVED"),
                      OTelAttribute<std::int64_t>("message.starting_offset", 0),
                      OTelAttribute<std::string>(sc::kThreadId, _))),
              AllOf(EventNamed("gl-cpp.read"),
                    SpanEventAttributesAre(
                        OTelAttribute<std::int64_t>(
                            /*sc::kRpcMessageId=*/"rpc.message.id", 2),
                        OTelAttribute<std::string>(
                            /*sc::kRpcMessageType=*/"rpc.message.type",
                            "RECEIVED"),
                        OTelAttribute<std::string>(sc::kThreadId, _)))))));
}

TEST(ReaderConnectionTracing, WithSuccess) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();
  PromiseWithOTelContext<ReadResponse> p1;
  PromiseWithOTelContext<ReadResponse> p2;
  PromiseWithOTelContext<ReadResponse> p3;

  auto mock = std::make_unique<MockAsyncReaderConnection>();
  EXPECT_CALL(*mock, Read)
      .WillOnce(expect_context(p1))
      .WillOnce(expect_context(p2))
      .WillOnce(expect_context(p3));
  EXPECT_CALL(*mock, GetRequestMetadata)
      .WillOnce(Return(RpcMetadata{{{"hk0", "v0"}, {"hk1", "v1"}},
                                   {{"tk0", "v0"}, {"tk1", "v1"}}}));
  auto actual = MakeTracingReaderConnection(
      internal::MakeSpan("test-span-name"), std::move(mock));

  auto f1 = actual->Read().then(expect_no_context);
  p1.set_value(ReadResponse(ReadPayload("m1")));
  (void)f1.get();

  auto f2 = actual->Read().then(expect_no_context);
  p2.set_value(ReadResponse(ReadPayload("m2").set_offset(1024)));
  (void)f2.get();

  auto f3 = actual->Read().then(expect_no_context);
  p3.set_value(ReadResponse(Status{}));
  (void)f3.get();

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanNamed("test-span-name"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanHasEvents(
              AllOf(
                  EventNamed("gl-cpp.read"),
                  SpanEventAttributesAre(
                      OTelAttribute<std::int64_t>(
                          /*sc::kRpcMessageId=*/"rpc.message.id", 1),
                      OTelAttribute<std::string>(
                          /*sc::kRpcMessageType=*/"rpc.message.type",
                          "RECEIVED"),
                      OTelAttribute<std::int64_t>("message.starting_offset", 0),
                      OTelAttribute<std::string>(sc::kThreadId, _))),
              AllOf(EventNamed("gl-cpp.read"),
                    SpanEventAttributesAre(
                        OTelAttribute<std::int64_t>(
                            /*sc::kRpcMessageId=*/"rpc.message.id", 2),
                        OTelAttribute<std::string>(
                            /*sc::kRpcMessageType=*/"rpc.message.type",
                            "RECEIVED"),
                        OTelAttribute<std::int64_t>("message.starting_offset",
                                                    1024),
                        OTelAttribute<std::string>(sc::kThreadId, _))),
              AllOf(EventNamed("gl-cpp.read"),
                    SpanEventAttributesAre(
                        OTelAttribute<std::int64_t>(
                            /*sc::kRpcMessageId=*/"rpc.message.id", 3),
                        OTelAttribute<std::string>(
                            /*sc::kRpcMessageType=*/"rpc.message.type",
                            "RECEIVED"),
                        OTelAttribute<std::string>(sc::kThreadId, _)))))));

  auto const metadata = actual->GetRequestMetadata();
  EXPECT_THAT(metadata.headers,
              UnorderedElementsAre(Pair("hk0", "v0"), Pair("hk1", "v1")));
  EXPECT_THAT(metadata.trailers,
              UnorderedElementsAre(Pair("tk0", "v0"), Pair("tk1", "v1")));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
