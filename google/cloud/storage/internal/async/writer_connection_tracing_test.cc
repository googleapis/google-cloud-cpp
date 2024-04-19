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

#include "google/cloud/storage/internal/async/writer_connection_tracing.h"
#include "google/cloud/storage/mocks/mock_async_writer_connection.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/opentelemetry_options.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <opentelemetry/trace/semantic_conventions.h>
#include <cstdint>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage_experimental::WritePayload;
using ::google::cloud::storage_mocks::MockAsyncWriterConnection;
using ::google::cloud::testing_util::EventNamed;
using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::OTelAttribute;
using ::google::cloud::testing_util::SpanEventAttributesAre;
using ::google::cloud::testing_util::SpanHasEvents;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsClient;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::SpanWithStatus;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Pair;
using ::testing::Return;
using ::testing::UnorderedElementsAre;
using ::testing::VariantWith;

auto ExpectSent(std::int64_t id, std::uint64_t size) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  return SpanEventAttributesAre(
      OTelAttribute<std::string>(sc::kMessageType, "SENT"),
      OTelAttribute<std::int64_t>(sc::kMessageId, id),
      OTelAttribute<std::string>(sc::kThreadId, _),
      OTelAttribute<std::uint64_t>("gl-cpp.size", size));
}

auto ExpectWrite(std::int64_t id, std::uint64_t size) {
  return AllOf(EventNamed("gl-cpp.write"), ExpectSent(id, size));
}

auto ExpectFlush(std::int64_t id, std::uint64_t size) {
  return AllOf(EventNamed("gl-cpp.flush"), ExpectSent(id, size));
}

auto ExpectQuery(std::int64_t id) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  return AllOf(EventNamed("gl-cpp.query"),
               SpanEventAttributesAre(
                   OTelAttribute<std::string>(sc::kMessageType, "RECEIVE"),
                   OTelAttribute<std::int64_t>(sc::kMessageId, id),
                   OTelAttribute<std::string>(sc::kThreadId, _)));
}

TEST(WriterConnectionTracing, FullCycle) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillOnce(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState).WillOnce(Return(16384));
  EXPECT_CALL(*mock, Write).Times(2).WillRepeatedly([] {
    return make_ready_future(Status{});
  });
  EXPECT_CALL(*mock, Flush).Times(2).WillRepeatedly([] {
    return make_ready_future(Status{});
  });
  EXPECT_CALL(*mock, Query).Times(2).WillRepeatedly([] {
    return make_ready_future(StatusOr<std::int64_t>(1024));
  });
  EXPECT_CALL(*mock, Finalize).WillOnce([] {
    return make_ready_future(make_status_or(google::storage::v2::Object{}));
  });
  EXPECT_CALL(*mock, GetRequestMetadata)
      .WillOnce(Return(RpcMetadata{{{"hk0", "v0"}, {"hk1", "v1"}},
                                   {{"tk0", "v0"}, {"tk1", "v1"}}}));
  auto actual = MakeTracingWriterConnection(
      internal::MakeSpan("test-span-name"), std::move(mock));
  EXPECT_EQ(actual->UploadId(), "test-upload-id");
  EXPECT_THAT(actual->PersistedState(), VariantWith<std::int64_t>(16384));
  EXPECT_STATUS_OK(actual->Write(WritePayload{std::string(1024, 'A')}).get());
  EXPECT_STATUS_OK(actual->Flush(WritePayload{std::string(1024, 'A')}).get());
  EXPECT_STATUS_OK(actual->Query().get());
  EXPECT_STATUS_OK(actual->Write(WritePayload{std::string(2048, 'B')}).get());
  EXPECT_STATUS_OK(actual->Flush(WritePayload{std::string(2048, 'B')}).get());
  EXPECT_STATUS_OK(actual->Query().get());
  auto response = actual->Finalize({}).get();
  EXPECT_STATUS_OK(response);

  auto const metadata = actual->GetRequestMetadata();
  EXPECT_THAT(metadata.headers,
              UnorderedElementsAre(Pair("hk0", "v0"), Pair("hk1", "v1")));
  EXPECT_THAT(metadata.trailers,
              UnorderedElementsAre(Pair("tk0", "v0"), Pair("tk1", "v1")));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans, ElementsAre(AllOf(
                 SpanNamed("test-span-name"),
                 SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                 SpanHasInstrumentationScope(), SpanKindIsClient(),
                 SpanHasEvents(
                     ExpectWrite(1, 1024), ExpectFlush(2, 1024), ExpectQuery(1),
                     ExpectWrite(3, 2048), ExpectFlush(4, 2048), ExpectQuery(2),
                     AllOf(EventNamed("gl-cpp.finalize"), ExpectSent(5, 0))))));
}

TEST(WriterConnectionTracing, FinalizeError) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, Finalize).WillOnce([] {
    return make_ready_future(
        StatusOr<google::storage::v2::Object>(PermanentError()));
  });
  auto actual = MakeTracingWriterConnection(
      internal::MakeSpan("test-span-name"), std::move(mock));
  auto response = actual->Finalize(WritePayload{std::string(1024, 'A')}).get();
  EXPECT_THAT(response, StatusIs(PermanentError().code()));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(SpanNamed("test-span-name"),
                        SpanWithStatus(opentelemetry::trace::StatusCode::kError,
                                       PermanentError().message()),
                        SpanHasInstrumentationScope(), SpanKindIsClient(),
                        SpanHasEvents(AllOf(EventNamed("gl-cpp.finalize"),
                                            ExpectSent(1, 1024))))));
}

TEST(WriterConnectionTracing, WriteError) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, Write).WillOnce([] {
    return make_ready_future(PermanentError());
  });
  auto actual = MakeTracingWriterConnection(
      internal::MakeSpan("test-span-name"), std::move(mock));
  auto status = actual->Write(WritePayload{std::string(1024, 'A')}).get();
  EXPECT_THAT(status, StatusIs(PermanentError().code()));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(SpanNamed("test-span-name"),
                        SpanWithStatus(opentelemetry::trace::StatusCode::kError,
                                       PermanentError().message()),
                        SpanHasInstrumentationScope(), SpanKindIsClient(),
                        SpanHasEvents(ExpectWrite(1, 1024)))));
}

TEST(WriterConnectionTracing, FlushError) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, Flush).WillOnce([] {
    return make_ready_future(PermanentError());
  });
  auto actual = MakeTracingWriterConnection(
      internal::MakeSpan("test-span-name"), std::move(mock));
  auto status = actual->Flush(WritePayload{std::string(1024, 'A')}).get();
  EXPECT_THAT(status, StatusIs(PermanentError().code()));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(SpanNamed("test-span-name"),
                        SpanWithStatus(opentelemetry::trace::StatusCode::kError,
                                       PermanentError().message()),
                        SpanHasInstrumentationScope(), SpanKindIsClient(),
                        SpanHasEvents(ExpectFlush(1, 1024)))));
}

TEST(WriterConnectionTracing, QueryError) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, Query).WillOnce([] {
    return make_ready_future(StatusOr<std::int64_t>(PermanentError()));
  });
  auto actual = MakeTracingWriterConnection(
      internal::MakeSpan("test-span-name"), std::move(mock));
  auto status = actual->Query().get();
  EXPECT_THAT(status, StatusIs(PermanentError().code()));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(SpanNamed("test-span-name"),
                        SpanWithStatus(opentelemetry::trace::StatusCode::kError,
                                       PermanentError().message()),
                        SpanHasInstrumentationScope(), SpanKindIsClient(),
                        SpanHasEvents(ExpectQuery(1)))));
}

TEST(WriterConnectionTracing, Cancel) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, Cancel).Times(1);
  EXPECT_CALL(*mock, Finalize).WillOnce([] {
    return make_ready_future(make_status_or(google::storage::v2::Object{}));
  });
  auto actual = MakeTracingWriterConnection(
      internal::MakeSpan("test-span-name"), std::move(mock));
  actual->Cancel();
  auto response = actual->Finalize({}).get();
  EXPECT_STATUS_OK(response);

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanNamed("test-span-name"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanHasEvents(AllOf(EventNamed("gl-cpp.cancel"),
                              SpanEventAttributesAre(OTelAttribute<std::string>(
                                  sc::kThreadId, _)))))));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
