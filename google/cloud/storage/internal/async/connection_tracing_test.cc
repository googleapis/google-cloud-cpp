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

#include "google/cloud/storage/internal/async/connection_tracing.h"
#include "google/cloud/storage/async/object_descriptor_connection.h"
#include "google/cloud/storage/async/reader_connection.h"
#include "google/cloud/storage/mocks/mock_async_connection.h"
#include "google/cloud/storage/mocks/mock_async_object_descriptor_connection.h"
#include "google/cloud/storage/mocks/mock_async_reader_connection.h"
#include "google/cloud/storage/mocks/mock_async_rewriter_connection.h"
#include "google/cloud/storage/mocks/mock_async_writer_connection.h"
#include "google/cloud/storage/options.h"
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

using ::google::cloud::storage::AsyncConnection;
using ::google::cloud::storage::ObjectDescriptorConnection;
using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage_mocks::MockAsyncConnection;
using ::google::cloud::storage_mocks::MockAsyncObjectDescriptorConnection;
using ::google::cloud::storage_mocks::MockAsyncReaderConnection;
using ::google::cloud::storage_mocks::MockAsyncRewriterConnection;
using ::google::cloud::storage_mocks::MockAsyncWriterConnection;
using ::google::cloud::testing_util::EventNamed;
using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::OTelAttribute;
using ::google::cloud::testing_util::OTelContextCaptured;
using ::google::cloud::testing_util::PromiseWithOTelContext;
using ::google::cloud::testing_util::SpanHasAttributes;
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
  PromiseWithOTelContext<StatusOr<google::storage::v2::Object>> p;

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));
  EXPECT_CALL(*mock, InsertObject).WillOnce(expect_context(p));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto result = actual->InsertObject(AsyncConnection::InsertObjectParams{})
                    .then(expect_no_context);

  p.set_value(make_status_or(google::storage::v2::Object{}));
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
      StatusOr<std::unique_ptr<storage::AsyncReaderConnection>>>
      p;

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));
  EXPECT_CALL(*mock, ReadObject).WillOnce(expect_context(p));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto result = actual->ReadObject(AsyncConnection::ReadObjectParams{})
                    .then(expect_no_context);

  p.set_value(StatusOr<std::unique_ptr<storage::AsyncReaderConnection>>(
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
      StatusOr<std::unique_ptr<storage::AsyncReaderConnection>>>
      p;

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));

  EXPECT_CALL(*mock, ReadObject).WillOnce(expect_context(p));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto f = actual->ReadObject(AsyncConnection::ReadObjectParams{})
               .then(expect_no_context);

  using Response =
      ::google::cloud::storage::AsyncReaderConnection::ReadResponse;
  auto mock_reader = std::make_unique<MockAsyncReaderConnection>();
  EXPECT_CALL(*mock_reader, Read)
      .WillOnce(Return(ByMove(make_ready_future(Response(Status{})))));
  p.set_value(StatusOr<std::unique_ptr<storage::AsyncReaderConnection>>(
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
  PromiseWithOTelContext<StatusOr<storage::ReadPayload>> p;

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));
  EXPECT_CALL(*mock, ReadObjectRange).WillOnce(expect_context(p));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto result = actual->ReadObjectRange(AsyncConnection::ReadObjectParams{})
                    .then(expect_no_context);
  p.set_value(storage::ReadPayload{});
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
      StatusOr<std::unique_ptr<storage::AsyncWriterConnection>>>
      p;

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));
  EXPECT_CALL(*mock, StartUnbufferedUpload).WillOnce(expect_context(p));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto result = actual->StartUnbufferedUpload(AsyncConnection::UploadParams{})
                    .then(expect_no_context);

  p.set_value(StatusOr<std::unique_ptr<storage::AsyncWriterConnection>>(
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
      StatusOr<std::unique_ptr<storage::AsyncWriterConnection>>>
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
  p.set_value(StatusOr<std::unique_ptr<storage::AsyncWriterConnection>>(
      std::move(mock_reader)));

  auto result = f.get();
  ASSERT_STATUS_OK(result);
  auto reader = *std::move(result);
  auto r = reader->Finalize(storage::WritePayload{}).get();
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
      StatusOr<std::unique_ptr<storage::AsyncWriterConnection>>>
      p;

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));
  EXPECT_CALL(*mock, StartBufferedUpload).WillOnce(expect_context(p));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto result = actual->StartBufferedUpload(AsyncConnection::UploadParams{})
                    .then(expect_no_context);

  p.set_value(StatusOr<std::unique_ptr<storage::AsyncWriterConnection>>(
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
      StatusOr<std::unique_ptr<storage::AsyncWriterConnection>>>
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
  p.set_value(StatusOr<std::unique_ptr<storage::AsyncWriterConnection>>(
      std::move(mock_reader)));

  auto result = f.get();
  ASSERT_STATUS_OK(result);
  auto reader = *std::move(result);
  auto r = reader->Finalize(storage::WritePayload{}).get();
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
      StatusOr<std::unique_ptr<storage::AsyncWriterConnection>>>
      p;

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));
  EXPECT_CALL(*mock, ResumeUnbufferedUpload).WillOnce(expect_context(p));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto result =
      actual->ResumeUnbufferedUpload(AsyncConnection::ResumeUploadParams{})
          .then(expect_no_context);

  p.set_value(StatusOr<std::unique_ptr<storage::AsyncWriterConnection>>(
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
      StatusOr<std::unique_ptr<storage::AsyncWriterConnection>>>
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
  p.set_value(StatusOr<std::unique_ptr<storage::AsyncWriterConnection>>(
      std::move(mock_reader)));

  auto result = f.get();
  ASSERT_STATUS_OK(result);
  auto reader = *std::move(result);
  auto r = reader->Finalize(storage::WritePayload{}).get();
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
      StatusOr<std::unique_ptr<storage::AsyncWriterConnection>>>
      p;

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));
  EXPECT_CALL(*mock, ResumeBufferedUpload).WillOnce(expect_context(p));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto result =
      actual->ResumeBufferedUpload(AsyncConnection::ResumeUploadParams{})
          .then(expect_no_context);

  p.set_value(StatusOr<std::unique_ptr<storage::AsyncWriterConnection>>(
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
      StatusOr<std::unique_ptr<storage::AsyncWriterConnection>>>
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
  p.set_value(StatusOr<std::unique_ptr<storage::AsyncWriterConnection>>(
      std::move(mock_reader)));

  auto result = f.get();
  ASSERT_STATUS_OK(result);
  auto reader = *std::move(result);
  auto r = reader->Finalize(storage::WritePayload{}).get();
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

TEST(ConnectionTracing, OpenError) {
  auto span_catcher = InstallSpanCatcher();
  PromiseWithOTelContext<
      StatusOr<std::shared_ptr<storage::ObjectDescriptorConnection>>>
      p;

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));
  EXPECT_CALL(*mock, Open).WillOnce(expect_context(p));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto result =
      actual->Open(AsyncConnection::OpenParams{}).then(expect_no_context);

  p.set_value(StatusOr<std::shared_ptr<storage::ObjectDescriptorConnection>>(
      PermanentError()));
  EXPECT_THAT(result.get(), StatusIs(PermanentError().code()));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans, ElementsAre(
                 AllOf(SpanNamed("storage::AsyncConnection::Open"),
                       SpanWithStatus(opentelemetry::trace::StatusCode::kError),
                       SpanHasInstrumentationScope(), SpanKindIsClient())));
}

TEST(ConnectionTracing, OpenSuccess) {
  auto span_catcher = InstallSpanCatcher();
  PromiseWithOTelContext<
      StatusOr<std::shared_ptr<storage::ObjectDescriptorConnection>>>
      p;
  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));
  EXPECT_CALL(*mock, Open).WillOnce(expect_context(p));

  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto f = actual->Open(AsyncConnection::OpenParams{}).then(expect_no_context);

  auto mock_descriptor =
      std::make_shared<MockAsyncObjectDescriptorConnection>();
  p.set_value(StatusOr<std::shared_ptr<storage::ObjectDescriptorConnection>>(
      std::move(mock_descriptor)));
  auto result = f.get();
  ASSERT_STATUS_OK(result);
  auto descriptor = *std::move(result);
  descriptor.reset();

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, ElementsAre(AllOf(
                         SpanNamed("storage::AsyncConnection::Open"),
                         SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                         SpanHasInstrumentationScope(), SpanKindIsClient())));
}

TEST(ConnectionTracing, StartAppendableObjectUploadSuccess) {
  auto span_catcher = InstallSpanCatcher();
  PromiseWithOTelContext<
      StatusOr<std::unique_ptr<storage::AsyncWriterConnection>>>
      p;

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));

  EXPECT_CALL(*mock, StartAppendableObjectUpload).WillOnce(expect_context(p));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto f = actual
               ->StartAppendableObjectUpload(
                   AsyncConnection::AppendableUploadParams{})
               .then(expect_no_context);

  auto mock_header = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock_header, Finalize)
      .WillOnce(Return(ByMove(
          make_ready_future(make_status_or(google::storage::v2::Object{})))));
  p.set_value(StatusOr<std::unique_ptr<storage::AsyncWriterConnection>>(
      std::move(mock_header)));

  auto result = f.get();
  ASSERT_STATUS_OK(result);
  auto writer = *std::move(result);
  auto w = writer->Finalize(storage::WritePayload{}).get();
  EXPECT_STATUS_OK(w);

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanNamed("storage::AsyncConnection::StartAppendableObjectUpload"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
          SpanHasInstrumentationScope(), SpanKindIsClient())));
}

TEST(ConnectionTracing, StartAppendableObjectUploadError) {
  auto span_catcher = InstallSpanCatcher();
  PromiseWithOTelContext<
      StatusOr<std::unique_ptr<storage::AsyncWriterConnection>>>
      p;

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));
  EXPECT_CALL(*mock, StartAppendableObjectUpload).WillOnce(expect_context(p));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto result = actual->StartAppendableObjectUpload(
      AsyncConnection::AppendableUploadParams{});

  p.set_value(StatusOr<std::unique_ptr<storage::AsyncWriterConnection>>(
      PermanentError()));
  EXPECT_THAT(result.get(), StatusIs(PermanentError().code()));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanNamed("storage::AsyncConnection::StartAppendableObjectUpload"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError),
          SpanHasInstrumentationScope(), SpanKindIsClient())));
}

TEST(ConnectionTracing, ResumeAppendableObjectUploadSuccess) {
  auto span_catcher = InstallSpanCatcher();
  PromiseWithOTelContext<
      StatusOr<std::unique_ptr<storage::AsyncWriterConnection>>>
      p;

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));

  EXPECT_CALL(*mock, ResumeAppendableObjectUpload).WillOnce(expect_context(p));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto f = actual
               ->ResumeAppendableObjectUpload(
                   AsyncConnection::AppendableUploadParams{})
               .then(expect_no_context);

  auto mock_writer = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock_writer, Finalize)
      .WillOnce(Return(ByMove(
          make_ready_future(make_status_or(google::storage::v2::Object{})))));
  p.set_value(StatusOr<std::unique_ptr<storage::AsyncWriterConnection>>(
      std::move(mock_writer)));

  auto result = f.get();
  ASSERT_STATUS_OK(result);
  auto writer = *std::move(result);
  auto w = writer->Finalize(storage::WritePayload{}).get();
  EXPECT_STATUS_OK(w);

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanNamed("storage::AsyncConnection::ResumeAppendableObjectUpload"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
          SpanHasInstrumentationScope(), SpanKindIsClient())));
}

TEST(ConnectionTracing, ResumeAppendableObjectUploadError) {
  auto span_catcher = InstallSpanCatcher();
  PromiseWithOTelContext<
      StatusOr<std::unique_ptr<storage::AsyncWriterConnection>>>
      p;

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));
  EXPECT_CALL(*mock, ResumeAppendableObjectUpload).WillOnce(expect_context(p));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto result = actual
                    ->ResumeAppendableObjectUpload(
                        AsyncConnection::AppendableUploadParams{})
                    .then(expect_no_context);

  p.set_value(StatusOr<std::unique_ptr<storage::AsyncWriterConnection>>(
      PermanentError()));
  EXPECT_THAT(result.get(), StatusIs(PermanentError().code()));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanNamed("storage::AsyncConnection::ResumeAppendableObjectUpload"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError),
          SpanHasInstrumentationScope(), SpanKindIsClient())));
}

TEST(ConnectionTracing, GetBucket) {
  auto span_catcher = InstallSpanCatcher();
  PromiseWithOTelContext<StatusOr<google::storage::v2::Bucket>> p;

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));
  EXPECT_CALL(*mock, GetBucket).WillOnce(expect_context(p));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto result = actual->GetBucket(AsyncConnection::GetBucketParams{})
                    .then(expect_no_context);

  p.set_value(make_status_or(google::storage::v2::Bucket{}));
  ASSERT_STATUS_OK(result.get());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, ElementsAre(AllOf(
                         SpanNamed("storage::AsyncConnection::GetBucket"),
                         SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                         SpanHasInstrumentationScope(), SpanKindIsClient())));
}

TEST(ConnectionTracing, GetBucketError) {
  auto span_catcher = InstallSpanCatcher();
  PromiseWithOTelContext<StatusOr<google::storage::v2::Bucket>> p;

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));
  EXPECT_CALL(*mock, GetBucket).WillOnce(expect_context(p));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto result = actual->GetBucket(AsyncConnection::GetBucketParams{})
                    .then(expect_no_context);

  p.set_value(StatusOr<google::storage::v2::Bucket>(PermanentError()));
  EXPECT_THAT(result.get(), StatusIs(PermanentError().code()));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans, ElementsAre(
                 AllOf(SpanNamed("storage::AsyncConnection::GetBucket"),
                       SpanWithStatus(opentelemetry::trace::StatusCode::kError),
                       SpanHasInstrumentationScope(), SpanKindIsClient())));
}

TEST(ConnectionTracing, GetBucketSpanEnrichment) {
  auto span_catcher = InstallSpanCatcher();
  PromiseWithOTelContext<StatusOr<google::storage::v2::Bucket>> p;

  auto options =
      TracingEnabled()
          .set<google::cloud::storage_experimental::OTelSpanEnrichmentOption>(
              true);
  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillRepeatedly(Return(options));
  EXPECT_CALL(*mock, GetBucket).WillOnce(expect_context(p));
  auto actual = MakeTracingAsyncConnection(std::move(mock));

  google::storage::v2::GetBucketRequest req;
  req.set_name("projects/_/buckets/test-bucket");
  auto result =
      actual->GetBucket({std::move(req), options}).then(expect_no_context);

  google::storage::v2::Bucket bucket_meta;
  bucket_meta.set_project("projects/123456");
  bucket_meta.set_location("us-east1");
  bucket_meta.set_location_type("regional");
  p.set_value(make_status_or(std::move(bucket_meta)));
  ASSERT_STATUS_OK(result.get());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanNamed("storage::AsyncConnection::GetBucket"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanHasAttributes(
              OTelAttribute<std::string>("gcp.resource.destination.id",
                                         "projects/123456/buckets/test-bucket"),
              OTelAttribute<std::string>("gcp.resource.destination.location",
                                         "us-east1")))));
}

TEST(ConnectionTracing, BucketMetadataCacheSuccess) {
  auto span_catcher = InstallSpanCatcher();
  PromiseWithOTelContext<Status> delete_promise_1;
  PromiseWithOTelContext<StatusOr<google::storage::v2::Bucket>> bucket_promise;
  PromiseWithOTelContext<Status> delete_promise_2;

  auto options =
      TracingEnabled()
          .set<google::cloud::storage_experimental::OTelSpanEnrichmentOption>(
              true);

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillRepeatedly(Return(options));

  // 1st call: DeleteObject on a cache miss triggers background GetBucket
  EXPECT_CALL(*mock, DeleteObject)
      .WillOnce(expect_context(delete_promise_1))
      .WillOnce(expect_context(delete_promise_2));

  EXPECT_CALL(*mock, GetBucket).WillOnce([&](auto const&) {
    return bucket_promise.get_future();
  });

  auto actual = MakeTracingAsyncConnection(std::move(mock));

  google::storage::v2::DeleteObjectRequest req;
  req.set_bucket("projects/_/buckets/test-bucket");
  req.set_object("test-object-1");

  auto res1 = actual->DeleteObject({req, options}).then(expect_no_context);
  delete_promise_1.set_value(Status{});
  ASSERT_STATUS_OK(res1.get());

  // Complete the background GetBucket fetch to populate the cache
  google::storage::v2::Bucket bucket_meta;
  bucket_meta.set_project("projects/123456");
  bucket_meta.set_location("us-east1");
  bucket_meta.set_location_type("regional");
  bucket_promise.set_value(make_status_or(std::move(bucket_meta)));

  // Clear spans from the first operation and background GetBucket
  (void)span_catcher->GetSpans();

  // 2nd call: DeleteObject hits the cache and span is enriched
  req.set_object("test-object-2");
  auto res2 = actual->DeleteObject({req, options}).then(expect_no_context);
  delete_promise_2.set_value(Status{});
  ASSERT_STATUS_OK(res2.get());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanNamed("storage::AsyncConnection::DeleteObject"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanHasAttributes(
              OTelAttribute<std::string>("gcp.resource.destination.id",
                                         "projects/123456/buckets/test-bucket"),
              OTelAttribute<std::string>("gcp.resource.destination.location",
                                         "us-east1")))));
}

TEST(ConnectionTracing, GetBucketMaybeInvalidateEvicts) {
  auto span_catcher = InstallSpanCatcher();
  PromiseWithOTelContext<StatusOr<google::storage::v2::Bucket>> p1;
  PromiseWithOTelContext<StatusOr<google::storage::v2::Bucket>> p2;

  auto options =
      TracingEnabled()
          .set<google::cloud::storage_experimental::OTelSpanEnrichmentOption>(
              true);
  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillRepeatedly(Return(options));

  EXPECT_CALL(*mock, GetBucket)
      .WillOnce(expect_context(p1))
      .WillOnce(expect_context(p2));

  auto actual = MakeTracingAsyncConnection(std::move(mock));

  // 1st call populates the cache
  google::storage::v2::GetBucketRequest req;
  req.set_name("projects/_/buckets/test-bucket");
  auto res1 = actual->GetBucket({req, options}).then(expect_no_context);
  google::storage::v2::Bucket bucket_meta;
  bucket_meta.set_project("projects/123456");
  bucket_meta.set_location("us-east1");
  bucket_meta.set_location_type("regional");
  p1.set_value(make_status_or(std::move(bucket_meta)));
  ASSERT_STATUS_OK(res1.get());

  // 2nd call returns kNotFound and evicts the cached metadata
  auto res2 = actual->GetBucket({req, options}).then(expect_no_context);
  p2.set_value(StatusOr<google::storage::v2::Bucket>(
      Status(StatusCode::kNotFound, "not found")));
  EXPECT_THAT(res2.get(), StatusIs(StatusCode::kNotFound));
}

TEST(ConnectionTracing, DeleteObjectNoEvictOnError) {
  auto span_catcher = InstallSpanCatcher();
  PromiseWithOTelContext<StatusOr<google::storage::v2::Bucket>> bucket_p;
  PromiseWithOTelContext<Status> delete_p1;
  PromiseWithOTelContext<Status> delete_p2;

  auto options =
      TracingEnabled()
          .set<google::cloud::storage_experimental::OTelSpanEnrichmentOption>(
              true);
  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillRepeatedly(Return(options));

  EXPECT_CALL(*mock, GetBucket).WillOnce(expect_context(bucket_p));
  EXPECT_CALL(*mock, DeleteObject)
      .WillOnce(expect_context(delete_p1))
      .WillOnce(expect_context(delete_p2));

  auto actual = MakeTracingAsyncConnection(std::move(mock));

  // 1st call: GetBucket populates the cache
  google::storage::v2::GetBucketRequest bucket_req;
  bucket_req.set_name("projects/_/buckets/test-bucket");
  auto res_bucket =
      actual->GetBucket({bucket_req, options}).then(expect_no_context);
  google::storage::v2::Bucket bucket_meta;
  bucket_meta.set_project("projects/123456");
  bucket_meta.set_location("us-east1");
  bucket_meta.set_location_type("regional");
  bucket_p.set_value(make_status_or(std::move(bucket_meta)));
  ASSERT_STATUS_OK(res_bucket.get());

  // 2nd call: DeleteObject fails with kNotFound (object missing)
  google::storage::v2::DeleteObjectRequest del_req;
  del_req.set_bucket("projects/_/buckets/test-bucket");
  del_req.set_object("missing-object");
  auto res_del1 =
      actual->DeleteObject({del_req, options}).then(expect_no_context);
  delete_p1.set_value(Status(StatusCode::kNotFound, "not found"));
  EXPECT_THAT(res_del1.get(), StatusIs(StatusCode::kNotFound));

  // Clear spans to verify next span enrichment
  (void)span_catcher->GetSpans();

  // 3rd call: DeleteObject proves bucket metadata was NOT evicted by object
  // error
  del_req.set_object("another-object");
  auto res_del2 =
      actual->DeleteObject({del_req, options}).then(expect_no_context);
  delete_p2.set_value(Status{});
  ASSERT_STATUS_OK(res_del2.get());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanNamed("storage::AsyncConnection::DeleteObject"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
          SpanHasAttributes(
              OTelAttribute<std::string>("gcp.resource.destination.id",
                                         "projects/123456/buckets/test-bucket"),
              OTelAttribute<std::string>("gcp.resource.destination.location",
                                         "us-east1")))));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
