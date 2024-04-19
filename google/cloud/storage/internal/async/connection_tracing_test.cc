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

#include "google/cloud/storage/internal/async/connection_tracing.h"
#include "google/cloud/storage/async/reader_connection.h"
#include "google/cloud/storage/mocks/mock_async_connection.h"
#include "google/cloud/storage/mocks/mock_async_reader_connection.h"
#include "google/cloud/storage/mocks/mock_async_rewriter_connection.h"
#include "google/cloud/storage/mocks/mock_async_writer_connection.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/opentelemetry_options.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage_experimental::AsyncConnection;
using ::google::cloud::storage_mocks::MockAsyncConnection;
using ::google::cloud::storage_mocks::MockAsyncReaderConnection;
using ::google::cloud::storage_mocks::MockAsyncRewriterConnection;
using ::google::cloud::storage_mocks::MockAsyncWriterConnection;
using ::google::cloud::testing_util::EventNamed;
using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::OTelContextCaptured;
using ::google::cloud::testing_util::PromiseWithOTelContext;
using ::google::cloud::testing_util::SpanHasEvents;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsClient;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::SpanWithStatus;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::ThereIsAnActiveSpan;
using ::testing::AllOf;
using ::testing::ByMove;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::ResultOf;
using ::testing::Return;
using ::testing::VariantWith;

Options TracingEnabled() {
  return Options{}.set<OpenTelemetryTracingOption>(true);
}

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

TEST(ConnectionTracing, Disabled) {
  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).Times(1);
  auto actual = MakeTracingAsyncConnection(mock);
  EXPECT_EQ(actual.get(), mock.get());
}

TEST(ConnectionTracing, Enabled) {
  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));
  auto actual = MakeTracingAsyncConnection(mock);
  EXPECT_NE(actual.get(), mock.get());
}

TEST(ConnectionTracing, InsertObject) {
  auto span_catcher = InstallSpanCatcher();
  PromiseWithOTelContext<StatusOr<storage::ObjectMetadata>> p;

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));
  EXPECT_CALL(*mock, InsertObject).WillOnce(expect_context(p));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto result = actual->InsertObject(AsyncConnection::InsertObjectParams{})
                    .then(expect_no_context);

  p.set_value(make_status_or(storage::ObjectMetadata{}));
  ASSERT_STATUS_OK(result.get());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, ElementsAre(AllOf(
                         SpanNamed("storage::AsyncConnection::InsertObject"),
                         SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                         SpanHasInstrumentationScope(), SpanKindIsClient())));
}

TEST(ConnectionTracing, ReadObjectError) {
  auto span_catcher = InstallSpanCatcher();
  PromiseWithOTelContext<
      StatusOr<std::unique_ptr<storage_experimental::AsyncReaderConnection>>>
      p;

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));
  EXPECT_CALL(*mock, ReadObject).WillOnce(expect_context(p));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto result = actual->ReadObject(AsyncConnection::ReadObjectParams{})
                    .then(expect_no_context);

  p.set_value(
      StatusOr<std::unique_ptr<storage_experimental::AsyncReaderConnection>>(
          PermanentError()));
  EXPECT_THAT(result.get(), StatusIs(PermanentError().code()));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans, ElementsAre(
                 AllOf(SpanNamed("storage::AsyncConnection::ReadObject"),
                       SpanWithStatus(opentelemetry::trace::StatusCode::kError),
                       SpanHasInstrumentationScope(), SpanKindIsClient())));
}

TEST(ConnectionTracing, ReadObjectSuccess) {
  auto span_catcher = InstallSpanCatcher();
  PromiseWithOTelContext<
      StatusOr<std::unique_ptr<storage_experimental::AsyncReaderConnection>>>
      p;

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));

  EXPECT_CALL(*mock, ReadObject).WillOnce(expect_context(p));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto f = actual->ReadObject(AsyncConnection::ReadObjectParams{})
               .then(expect_no_context);

  using Response = ::google::cloud::storage_experimental::
      AsyncReaderConnection::ReadResponse;
  auto mock_reader = std::make_unique<MockAsyncReaderConnection>();
  EXPECT_CALL(*mock_reader, Read)
      .WillOnce(Return(ByMove(make_ready_future(Response(Status{})))));
  p.set_value(
      StatusOr<std::unique_ptr<storage_experimental::AsyncReaderConnection>>(
          std::move(mock_reader)));

  auto result = f.get();
  ASSERT_STATUS_OK(result);
  auto reader = *std::move(result);
  EXPECT_THAT(reader->Read().get(), VariantWith<Status>(IsOk()));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, ElementsAre(AllOf(
                         SpanNamed("storage::AsyncConnection::ReadObject"),
                         SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                         SpanHasInstrumentationScope(), SpanKindIsClient())));
}

TEST(ConnectionTracing, ReadObjectRange) {
  auto span_catcher = InstallSpanCatcher();
  PromiseWithOTelContext<StatusOr<storage_experimental::ReadPayload>> p;

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));
  EXPECT_CALL(*mock, ReadObjectRange).WillOnce(expect_context(p));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto result = actual->ReadObjectRange(AsyncConnection::ReadObjectParams{})
                    .then(expect_no_context);
  p.set_value(storage_experimental::ReadPayload{});
  ASSERT_STATUS_OK(result.get());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, ElementsAre(AllOf(
                         SpanNamed("storage::AsyncConnection::ReadObjectRange"),
                         SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                         SpanHasInstrumentationScope(), SpanKindIsClient())));
}

TEST(ConnectionTracing, StartUnbufferedUploadError) {
  auto span_catcher = InstallSpanCatcher();
  PromiseWithOTelContext<
      StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>
      p;

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));
  EXPECT_CALL(*mock, StartUnbufferedUpload).WillOnce(expect_context(p));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto result = actual->StartUnbufferedUpload(AsyncConnection::UploadParams{})
                    .then(expect_no_context);

  p.set_value(
      StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>(
          PermanentError()));
  EXPECT_THAT(result.get(), StatusIs(PermanentError().code()));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              ElementsAre(AllOf(
                  SpanNamed("storage::AsyncConnection::StartUnbufferedUpload"),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError),
                  SpanHasInstrumentationScope(), SpanKindIsClient())));
}

TEST(ConnectionTracing, StartUnbufferedUploadSuccess) {
  auto span_catcher = InstallSpanCatcher();
  PromiseWithOTelContext<
      StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>
      p;

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));

  EXPECT_CALL(*mock, StartUnbufferedUpload).WillOnce(expect_context(p));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto f = actual->StartUnbufferedUpload(AsyncConnection::UploadParams{})
               .then(expect_no_context);

  auto mock_reader = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock_reader, Finalize)
      .WillOnce(Return(ByMove(
          make_ready_future(make_status_or(google::storage::v2::Object{})))));
  p.set_value(
      StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>(
          std::move(mock_reader)));

  auto result = f.get();
  ASSERT_STATUS_OK(result);
  auto reader = *std::move(result);
  auto r = reader->Finalize(storage_experimental::WritePayload{}).get();
  EXPECT_STATUS_OK(r);

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              ElementsAre(AllOf(
                  SpanNamed("storage::AsyncConnection::StartUnbufferedUpload"),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                  SpanHasInstrumentationScope(), SpanKindIsClient())));
}

TEST(ConnectionTracing, StartBufferedUploadError) {
  auto span_catcher = InstallSpanCatcher();
  PromiseWithOTelContext<
      StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>
      p;

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));
  EXPECT_CALL(*mock, StartBufferedUpload).WillOnce(expect_context(p));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto result = actual->StartBufferedUpload(AsyncConnection::UploadParams{})
                    .then(expect_no_context);

  p.set_value(
      StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>(
          PermanentError()));
  EXPECT_THAT(result.get(), StatusIs(PermanentError().code()));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              ElementsAre(AllOf(
                  SpanNamed("storage::AsyncConnection::StartBufferedUpload"),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError),
                  SpanHasInstrumentationScope(), SpanKindIsClient())));
}

TEST(ConnectionTracing, StartBufferedUploadSuccess) {
  auto span_catcher = InstallSpanCatcher();
  PromiseWithOTelContext<
      StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>
      p;

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));

  EXPECT_CALL(*mock, StartBufferedUpload).WillOnce(expect_context(p));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto f = actual->StartBufferedUpload(AsyncConnection::UploadParams{})
               .then(expect_no_context);

  auto mock_reader = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock_reader, Finalize)
      .WillOnce(Return(ByMove(
          make_ready_future(make_status_or(google::storage::v2::Object{})))));
  p.set_value(
      StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>(
          std::move(mock_reader)));

  auto result = f.get();
  ASSERT_STATUS_OK(result);
  auto reader = *std::move(result);
  auto r = reader->Finalize(storage_experimental::WritePayload{}).get();
  EXPECT_STATUS_OK(r);

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              ElementsAre(AllOf(
                  SpanNamed("storage::AsyncConnection::StartBufferedUpload"),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                  SpanHasInstrumentationScope(), SpanKindIsClient())));
}

TEST(ConnectionTracing, ResumeUnbufferedUploadError) {
  auto span_catcher = InstallSpanCatcher();
  PromiseWithOTelContext<
      StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>
      p;

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));
  EXPECT_CALL(*mock, ResumeUnbufferedUpload).WillOnce(expect_context(p));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto result =
      actual->ResumeUnbufferedUpload(AsyncConnection::ResumeUploadParams{})
          .then(expect_no_context);

  p.set_value(
      StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>(
          PermanentError()));
  EXPECT_THAT(result.get(), StatusIs(PermanentError().code()));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              ElementsAre(AllOf(
                  SpanNamed("storage::AsyncConnection::ResumeUnbufferedUpload"),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError),
                  SpanHasInstrumentationScope(), SpanKindIsClient())));
}

TEST(ConnectionTracing, ResumeUnbufferedUploadSuccess) {
  auto span_catcher = InstallSpanCatcher();
  PromiseWithOTelContext<
      StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>
      p;

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));

  EXPECT_CALL(*mock, ResumeUnbufferedUpload).WillOnce(expect_context(p));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto f = actual->ResumeUnbufferedUpload(AsyncConnection::ResumeUploadParams{})
               .then(expect_no_context);

  auto mock_reader = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock_reader, Finalize)
      .WillOnce(Return(ByMove(
          make_ready_future(make_status_or(google::storage::v2::Object{})))));
  p.set_value(
      StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>(
          std::move(mock_reader)));

  auto result = f.get();
  ASSERT_STATUS_OK(result);
  auto reader = *std::move(result);
  auto r = reader->Finalize(storage_experimental::WritePayload{}).get();
  EXPECT_STATUS_OK(r);

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              ElementsAre(AllOf(
                  SpanNamed("storage::AsyncConnection::ResumeUnbufferedUpload"),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                  SpanHasInstrumentationScope(), SpanKindIsClient())));
}

TEST(ConnectionTracing, ResumeBufferedUploadError) {
  auto span_catcher = InstallSpanCatcher();
  PromiseWithOTelContext<
      StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>
      p;

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));
  EXPECT_CALL(*mock, ResumeBufferedUpload).WillOnce(expect_context(p));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto result =
      actual->ResumeBufferedUpload(AsyncConnection::ResumeUploadParams{})
          .then(expect_no_context);

  p.set_value(
      StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>(
          PermanentError()));
  EXPECT_THAT(result.get(), StatusIs(PermanentError().code()));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              ElementsAre(AllOf(
                  SpanNamed("storage::AsyncConnection::ResumeBufferedUpload"),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError),
                  SpanHasInstrumentationScope(), SpanKindIsClient())));
}

TEST(ConnectionTracing, ResumeBufferedUploadSuccess) {
  auto span_catcher = InstallSpanCatcher();
  PromiseWithOTelContext<
      StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>
      p;

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));

  EXPECT_CALL(*mock, ResumeBufferedUpload).WillOnce(expect_context(p));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto f = actual->ResumeBufferedUpload(AsyncConnection::ResumeUploadParams{})
               .then(expect_no_context);

  auto mock_reader = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock_reader, Finalize)
      .WillOnce(Return(ByMove(
          make_ready_future(make_status_or(google::storage::v2::Object{})))));
  p.set_value(
      StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>(
          std::move(mock_reader)));

  auto result = f.get();
  ASSERT_STATUS_OK(result);
  auto reader = *std::move(result);
  auto r = reader->Finalize(storage_experimental::WritePayload{}).get();
  EXPECT_STATUS_OK(r);

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              ElementsAre(AllOf(
                  SpanNamed("storage::AsyncConnection::ResumeBufferedUpload"),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                  SpanHasInstrumentationScope(), SpanKindIsClient())));
}

TEST(ConnectionTracing, ComposeObject) {
  auto span_catcher = InstallSpanCatcher();
  PromiseWithOTelContext<StatusOr<google::storage::v2::Object>> p;

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));
  EXPECT_CALL(*mock, ComposeObject).WillOnce(expect_context(p));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto result = actual->ComposeObject(AsyncConnection::ComposeObjectParams{})
                    .then(expect_no_context);

  p.set_value(make_status_or(google::storage::v2::Object{}));
  ASSERT_STATUS_OK(result.get());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, ElementsAre(AllOf(
                         SpanNamed("storage::AsyncConnection::ComposeObject"),
                         SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                         SpanHasInstrumentationScope(), SpanKindIsClient())));
}

TEST(ConnectionTracing, DeleteObject) {
  auto span_catcher = InstallSpanCatcher();
  PromiseWithOTelContext<Status> p;

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));
  EXPECT_CALL(*mock, DeleteObject).WillOnce(expect_context(p));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto result = actual->DeleteObject(AsyncConnection::DeleteObjectParams{})
                    .then(expect_no_context);
  p.set_value(Status{});
  ASSERT_STATUS_OK(result.get());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, ElementsAre(AllOf(
                         SpanNamed("storage::AsyncConnection::DeleteObject"),
                         SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                         SpanHasInstrumentationScope(), SpanKindIsClient())));
}

using ::google::storage::v2::RewriteResponse;

auto MakeRewriteResponse() {
  RewriteResponse response;
  response.set_total_bytes_rewritten(3000);
  response.set_object_size(3000);
  response.mutable_resource()->set_size(3000);
  return response;
}

auto MatchRewriteResponse() {
  return AllOf(
      ResultOf(
          "total bytes",
          [](RewriteResponse const& v) { return v.total_bytes_rewritten(); },
          3000),
      ResultOf(
          "size", [](RewriteResponse const& v) { return v.object_size(); },
          3000),
      ResultOf(
          "token", [](RewriteResponse const& v) { return v.rewrite_token(); },
          IsEmpty()),
      ResultOf(
          "has_resource",
          [](RewriteResponse const& v) { return v.has_resource(); }, true),
      ResultOf(
          "resource.size",
          [](RewriteResponse const& v) { return v.resource().size(); }, 3000));
}

TEST(ConnectionTracing, RewriteObject) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillRepeatedly(Return(TracingEnabled()));
  EXPECT_CALL(*mock, RewriteObject).WillOnce([] {
    auto rewriter = std::make_shared<MockAsyncRewriterConnection>();
    EXPECT_CALL(*rewriter, Iterate).WillOnce([] {
      EXPECT_TRUE(ThereIsAnActiveSpan());
      EXPECT_TRUE(OTelContextCaptured());
      return make_ready_future(make_status_or(MakeRewriteResponse()));
    });
    return rewriter;
  });
  auto connection = MakeTracingAsyncConnection(std::move(mock));
  auto rewriter = connection->RewriteObject(
      AsyncConnection::RewriteObjectParams{{}, connection->options()});
  auto r1 = rewriter->Iterate().get();
  EXPECT_THAT(r1, IsOkAndHolds(MatchRewriteResponse()));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans, ElementsAre(AllOf(
                 SpanNamed("storage::AsyncConnection::RewriteObject"),
                 SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                 SpanHasInstrumentationScope(), SpanKindIsClient(),
                 SpanHasEvents(EventNamed("gl-cpp.storage.rewrite.iterate")))));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
