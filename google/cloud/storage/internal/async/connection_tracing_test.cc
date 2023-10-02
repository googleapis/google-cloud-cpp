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
#include "google/cloud/storage/async_reader_connection.h"
#include "google/cloud/storage/mocks/mock_async_connection.h"
#include "google/cloud/storage/mocks/mock_async_reader_connection.h"
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

using ::google::cloud::storage_experimental::AsyncConnection;
using ::google::cloud::storage_mocks::MockAsyncConnection;
using ::google::cloud::storage_mocks::MockAsyncReaderConnection;
using ::google::cloud::testing_util::InstallSpanCatcher;
using ReadResponse =
    ::google::cloud::storage_experimental::AsyncReaderConnection;
using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsClient;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::SpanWithStatus;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AllOf;
using ::testing::ByMove;
using ::testing::ElementsAre;
using ::testing::Return;

Options TracingEnabled() {
  return Options{}.set<OpenTelemetryTracingOption>(true);
}

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

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));
  EXPECT_CALL(*mock, AsyncInsertObject)
      .WillOnce(Return(ByMove(
          make_ready_future(make_status_or(storage::ObjectMetadata{})))));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto const result =
      actual->AsyncInsertObject(AsyncConnection::InsertObjectParams{}).get();
  ASSERT_STATUS_OK(result);

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans, ElementsAre(
                 AllOf(SpanNamed("storage::AsyncConnection::AsyncInsertObject"),
                       SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                       SpanHasInstrumentationScope(), SpanKindIsClient())));
}

TEST(ConnectionTracing, AsyncReadObjectError) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));
  EXPECT_CALL(*mock, AsyncReadObject)
      .WillOnce(Return(ByMove(make_ready_future(
          StatusOr<
              std::unique_ptr<storage_experimental::AsyncReaderConnection>>(
              PermanentError())))));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto result =
      actual->AsyncReadObject(AsyncConnection::ReadObjectParams{}).get();
  EXPECT_THAT(result, StatusIs(PermanentError().code()));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans, ElementsAre(
                 AllOf(SpanNamed("storage::AsyncConnection::AsyncReadObject"),
                       SpanWithStatus(opentelemetry::trace::StatusCode::kError),
                       SpanHasInstrumentationScope(), SpanKindIsClient())));
}

TEST(ConnectionTracing, AsyncReadObjectSuccess) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));
  EXPECT_CALL(*mock, AsyncReadObject).WillOnce([] {
    using Response = ::google::cloud::storage_experimental::
        AsyncReaderConnection::ReadResponse;

    auto reader = std::make_unique<MockAsyncReaderConnection>();
    EXPECT_CALL(*reader, Read)
        .WillOnce(Return(ByMove(make_ready_future(Response(Status{})))));
    return make_ready_future(
        StatusOr<std::unique_ptr<storage_experimental::AsyncReaderConnection>>(
            std::move(reader)));
  });
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto result =
      actual->AsyncReadObject(AsyncConnection::ReadObjectParams{}).get();
  ASSERT_STATUS_OK(result);
  auto reader = *std::move(result);
  auto r = reader->Read().get();
  ASSERT_TRUE(absl::holds_alternative<Status>(r));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, ElementsAre(AllOf(
                         SpanNamed("storage::AsyncConnection::AsyncReadObject"),
                         SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                         SpanHasInstrumentationScope(), SpanKindIsClient())));
}

TEST(ConnectionTracing, AsyncReadObjectRange) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));
  EXPECT_CALL(*mock, AsyncReadObjectRange)
      .WillOnce(Return(ByMove(make_ready_future(
          storage_experimental::AsyncReadObjectRangeResponse{}))));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto const result =
      actual->AsyncReadObjectRange(AsyncConnection::ReadObjectParams{}).get();
  ASSERT_STATUS_OK(result.status);

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              ElementsAre(AllOf(
                  SpanNamed("storage::AsyncConnection::AsyncReadObjectRange"),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                  SpanHasInstrumentationScope(), SpanKindIsClient())));
}

TEST(ConnectionTracing, AsyncComposeObject) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));
  EXPECT_CALL(*mock, AsyncComposeObject)
      .WillOnce(Return(ByMove(
          make_ready_future(make_status_or(storage::ObjectMetadata{})))));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto const result =
      actual->AsyncComposeObject(AsyncConnection::ComposeObjectParams{}).get();
  ASSERT_STATUS_OK(result);

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              ElementsAre(AllOf(
                  SpanNamed("storage::AsyncConnection::AsyncComposeObject"),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                  SpanHasInstrumentationScope(), SpanKindIsClient())));
}

TEST(ConnectionTracing, AsyncDeleteObject) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_unique<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(TracingEnabled()));
  EXPECT_CALL(*mock, AsyncDeleteObject)
      .WillOnce(Return(ByMove(make_ready_future(Status{}))));
  auto actual = MakeTracingAsyncConnection(std::move(mock));
  auto const result =
      actual->AsyncDeleteObject(AsyncConnection::DeleteObjectParams{}).get();
  ASSERT_STATUS_OK(result);

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
