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
using ::google::cloud::storage_mocks::MockAsyncWriterConnection;
using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::OTelContextCaptured;
using ::google::cloud::testing_util::PromiseWithOTelContext;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsClient;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::SpanWithStatus;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::ThereIsAnActiveSpan;
using ::testing::AllOf;
using ::testing::ByMove;
using ::testing::ElementsAre;
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

TEST(ConnectionTracing, AsyncInsertObject) {
  auto span_catcher = InstallSpanCatcher();
  PromiseWithOTelContext<StatusOr<storage::ObjectMetadata>> p;

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));
  EXPECT_CALL(*mock, AsyncInsertObject).WillOnce(expect_context(p));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto result = actual->AsyncInsertObject(AsyncConnection::InsertObjectParams{})
                    .then(expect_no_context);

  p.set_value(make_status_or(storage::ObjectMetadata{}));
  ASSERT_STATUS_OK(result.get());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans, ElementsAre(
                 AllOf(SpanNamed("storage::AsyncConnection::AsyncInsertObject"),
                       SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                       SpanHasInstrumentationScope(), SpanKindIsClient())));
}

TEST(ConnectionTracing, AsyncReadObjectError) {
  auto span_catcher = InstallSpanCatcher();
  PromiseWithOTelContext<
      StatusOr<std::unique_ptr<storage_experimental::AsyncReaderConnection>>>
      p;

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));
  EXPECT_CALL(*mock, AsyncReadObject).WillOnce(expect_context(p));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto result = actual->AsyncReadObject(AsyncConnection::ReadObjectParams{})
                    .then(expect_no_context);

  p.set_value(
      StatusOr<std::unique_ptr<storage_experimental::AsyncReaderConnection>>(
          PermanentError()));
  EXPECT_THAT(result.get(), StatusIs(PermanentError().code()));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans, ElementsAre(
                 AllOf(SpanNamed("storage::AsyncConnection::AsyncReadObject"),
                       SpanWithStatus(opentelemetry::trace::StatusCode::kError),
                       SpanHasInstrumentationScope(), SpanKindIsClient())));
}

TEST(ConnectionTracing, AsyncReadObjectSuccess) {
  auto span_catcher = InstallSpanCatcher();
  PromiseWithOTelContext<
      StatusOr<std::unique_ptr<storage_experimental::AsyncReaderConnection>>>
      p;

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));

  EXPECT_CALL(*mock, AsyncReadObject).WillOnce(expect_context(p));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto f = actual->AsyncReadObject(AsyncConnection::ReadObjectParams{})
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
                         SpanNamed("storage::AsyncConnection::AsyncReadObject"),
                         SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                         SpanHasInstrumentationScope(), SpanKindIsClient())));
}

TEST(ConnectionTracing, AsyncReadObjectRange) {
  auto span_catcher = InstallSpanCatcher();
  PromiseWithOTelContext<StatusOr<storage_experimental::ReadPayload>> p;

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));
  EXPECT_CALL(*mock, AsyncReadObjectRange).WillOnce(expect_context(p));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto result =
      actual->AsyncReadObjectRange(AsyncConnection::ReadObjectParams{})
          .then(expect_no_context);
  p.set_value(storage_experimental::ReadPayload{});
  ASSERT_STATUS_OK(result.get());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              ElementsAre(AllOf(
                  SpanNamed("storage::AsyncConnection::AsyncReadObjectRange"),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                  SpanHasInstrumentationScope(), SpanKindIsClient())));
}

TEST(ConnectionTracing, AsyncWriteObjectError) {
  auto span_catcher = InstallSpanCatcher();
  PromiseWithOTelContext<
      StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>
      p;

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));
  EXPECT_CALL(*mock, AsyncWriteObject).WillOnce(expect_context(p));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto result = actual->AsyncWriteObject(AsyncConnection::WriteObjectParams{})
                    .then(expect_no_context);

  p.set_value(
      StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>(
          PermanentError()));
  EXPECT_THAT(result.get(), StatusIs(PermanentError().code()));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans, ElementsAre(
                 AllOf(SpanNamed("storage::AsyncConnection::AsyncWriteObject"),
                       SpanWithStatus(opentelemetry::trace::StatusCode::kError),
                       SpanHasInstrumentationScope(), SpanKindIsClient())));
}

TEST(ConnectionTracing, AsyncWriteObjectSuccess) {
  auto span_catcher = InstallSpanCatcher();
  PromiseWithOTelContext<
      StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>
      p;

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));

  EXPECT_CALL(*mock, AsyncWriteObject).WillOnce(expect_context(p));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto f = actual->AsyncWriteObject(AsyncConnection::WriteObjectParams{})
               .then(expect_no_context);

  auto mock_reader = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock_reader, Finalize)
      .WillOnce(Return(ByMove(
          make_ready_future(make_status_or(storage::ObjectMetadata{})))));
  p.set_value(
      StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>(
          std::move(mock_reader)));

  auto result = f.get();
  ASSERT_STATUS_OK(result);
  auto reader = *std::move(result);
  auto r = reader->Finalize(storage_experimental::WritePayload{}).get();
  EXPECT_STATUS_OK(r);

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(SpanNamed("storage::AsyncConnection::AsyncWriteObject"),
                        SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                        SpanHasInstrumentationScope(), SpanKindIsClient())));
}

TEST(ConnectionTracing, AsyncComposeObject) {
  auto span_catcher = InstallSpanCatcher();
  PromiseWithOTelContext<StatusOr<storage::ObjectMetadata>> p;

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));
  EXPECT_CALL(*mock, AsyncComposeObject).WillOnce(expect_context(p));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto result =
      actual->AsyncComposeObject(AsyncConnection::ComposeObjectParams{})
          .then(expect_no_context);

  p.set_value(make_status_or(storage::ObjectMetadata{}));
  ASSERT_STATUS_OK(result.get());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              ElementsAre(AllOf(
                  SpanNamed("storage::AsyncConnection::AsyncComposeObject"),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                  SpanHasInstrumentationScope(), SpanKindIsClient())));
}

TEST(ConnectionTracing, AsyncDeleteObject) {
  auto span_catcher = InstallSpanCatcher();
  PromiseWithOTelContext<Status> p;

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));
  EXPECT_CALL(*mock, AsyncDeleteObject).WillOnce(expect_context(p));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto result = actual->AsyncDeleteObject(AsyncConnection::DeleteObjectParams{})
                    .then(expect_no_context);
  p.set_value(Status{});
  ASSERT_STATUS_OK(result.get());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans, ElementsAre(
                 AllOf(SpanNamed("storage::AsyncConnection::AsyncDeleteObject"),
                       SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                       SpanHasInstrumentationScope(), SpanKindIsClient())));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
