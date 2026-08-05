// Copyright 2024 Google LLC
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

#include "google/cloud/storage/internal/async/object_descriptor_connection_tracing.h"
#include "google/cloud/storage/async/object_descriptor_connection.h"
#include "google/cloud/storage/mocks/mock_async_object_descriptor_connection.h"
#include "google/cloud/storage/mocks/mock_async_reader_connection.h"
#include "google/cloud/opentelemetry_options.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <opentelemetry/semconv/incubating/thread_attributes.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ReadResponse =
    ::google::cloud::storage::AsyncReaderConnection::ReadResponse;
using ::google::cloud::storage::ObjectDescriptorConnection;
using ::google::cloud::storage::ReadPayload;
using ::google::cloud::storage_mocks::MockAsyncObjectDescriptorConnection;
using ::google::cloud::storage_mocks::MockAsyncReaderConnection;
using ::google::cloud::testing_util::EventNamed;
using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::OTelAttribute;
using ::google::cloud::testing_util::PromiseWithOTelContext;
using ::google::cloud::testing_util::SpanEventAttributesAre;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsClient;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::SpanWithStatus;
using ::testing::_;

TEST(ObjectDescriptorConnectionTracing, Read) {
  namespace sc = ::opentelemetry::semconv;
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_unique<MockAsyncObjectDescriptorConnection>();
  EXPECT_CALL(*mock, Read)
      .WillOnce([](ObjectDescriptorConnection::ReadParams p) {
        EXPECT_EQ(p.start, 100);
        EXPECT_EQ(p.length, 200);
        return std::make_unique<MockAsyncReaderConnection>();
      });
  auto actual = MakeTracingObjectDescriptorConnection(
      internal::MakeSpan("test-span-name"), std::move(mock));
  auto f1 = actual->Read(ObjectDescriptorConnection::ReadParams{100, 200});

  actual.reset();
  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanNamed("test-span-name"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanHasEvents(AllOf(
              EventNamed("gl-cpp.open.read"),
              SpanEventAttributesAre(
                  OTelAttribute<std::int64_t>("read-length", 200),
                  OTelAttribute<std::int64_t>("read-start", 100),
                  OTelAttribute<std::string>(sc::thread::kThreadId, _)))))));
}

TEST(ObjectDescriptorConnectionTracing,
     SingleReadRangeCompletedDoesNotEndOpenSpan) {
  namespace sc = ::opentelemetry::semconv;
  auto span_catcher = InstallSpanCatcher();

  auto mock_connection =
      std::make_shared<MockAsyncObjectDescriptorConnection>();
  auto mock_reader = std::make_unique<MockAsyncReaderConnection>();
  PromiseWithOTelContext<ReadResponse> p;
  EXPECT_CALL(*mock_reader, Read).WillOnce([&p] { return p.get_future(); });

  EXPECT_CALL(*mock_connection, Read)
      .WillOnce([&, r = std::move(mock_reader)](
                    ObjectDescriptorConnection::ReadParams p) mutable {
        EXPECT_EQ(p.start, 100);
        EXPECT_EQ(p.length, 200);
        return std::move(r);
      });

  auto connection = MakeTracingObjectDescriptorConnection(
      internal::MakeSpan("test-span"), std::move(mock_connection));

  auto reader = connection->Read({100, 200});
  auto f = reader->Read();
  // Simulate stream completion (EOF)
  p.set_value(Status{});
  (void)f.get();

  // Before resetting the connection, the Open span must NOT be ended yet.
  EXPECT_THAT(span_catcher->GetSpans(), ::testing::IsEmpty());

  connection.reset();  // End the span now

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanNamed("test-span"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanEventsAre(AllOf(
              EventNamed("gl-cpp.open.read"),
              SpanEventAttributesAre(
                  OTelAttribute<std::int64_t>("read-length", 200),
                  OTelAttribute<std::int64_t>("read-start", 100),
                  OTelAttribute<std::string>(sc::thread::kThreadId, _)))))));
}

TEST(ObjectDescriptorConnectionTracing, MultipleReadRanges) {
  namespace sc = ::opentelemetry::semconv;
  auto span_catcher = InstallSpanCatcher();

  auto mock_connection =
      std::make_shared<MockAsyncObjectDescriptorConnection>();
  auto mock_reader1 = std::make_unique<MockAsyncReaderConnection>();
  auto mock_reader2 = std::make_unique<MockAsyncReaderConnection>();

  EXPECT_CALL(*mock_connection, Read)
      .WillOnce([&](ObjectDescriptorConnection::ReadParams p) {
        EXPECT_EQ(p.start, 0);
        EXPECT_EQ(p.length, 100);
        return std::move(mock_reader1);
      })
      .WillOnce([&](ObjectDescriptorConnection::ReadParams p) {
        EXPECT_EQ(p.start, 100);
        EXPECT_EQ(p.length, 200);
        return std::move(mock_reader2);
      });

  auto connection = MakeTracingObjectDescriptorConnection(
      internal::MakeSpan("test-span"), std::move(mock_connection));

  auto reader1 = connection->Read({0, 100});
  auto reader2 = connection->Read({100, 200});

  // Span is still active and not ended yet
  EXPECT_THAT(span_catcher->GetSpans(), ::testing::IsEmpty());

  connection.reset();  // End the span

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanNamed("test-span"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanEventsAre(
              AllOf(EventNamed("gl-cpp.open.read"),
                    SpanEventAttributesAre(
                        OTelAttribute<std::int64_t>("read-length", 100),
                        OTelAttribute<std::int64_t>("read-start", 0),
                        OTelAttribute<std::string>(sc::thread::kThreadId, _))),
              AllOf(EventNamed("gl-cpp.open.read"),
                    SpanEventAttributesAre(
                        OTelAttribute<std::int64_t>("read-length", 200),
                        OTelAttribute<std::int64_t>("read-start", 100),
                        OTelAttribute<std::string>(sc::thread::kThreadId,
                                                   _)))))));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
