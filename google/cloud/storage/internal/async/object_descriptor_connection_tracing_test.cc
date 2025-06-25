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

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#include "google/cloud/storage/internal/async/object_descriptor_connection_tracing.h"
#include "google/cloud/storage/async/object_descriptor_connection.h"
#include "google/cloud/storage/mocks/mock_async_object_descriptor_connection.h"
#include "google/cloud/storage/mocks/mock_async_reader_connection.h"
#include "google/cloud/opentelemetry_options.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <opentelemetry/trace/semantic_conventions.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ReadResponse =
    ::google::cloud::storage_experimental::AsyncReaderConnection::ReadResponse;
using ::google::cloud::storage_experimental::ObjectDescriptorConnection;
using ::google::cloud::storage_mocks::MockAsyncObjectDescriptorConnection;
using ::google::cloud::storage_mocks::MockAsyncReaderConnection;
using ::google::cloud::storage_experimental::ReadPayload;
using ::google::cloud::testing_util::EventNamed;
using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::OTelAttribute;
using ::google::cloud::testing_util::OTelContextCaptured;
using ::google::cloud::testing_util::ThereIsAnActiveSpan;
using ::google::cloud::testing_util::PromiseWithOTelContext;
using ::google::cloud::testing_util::SpanEventAttributesAre;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsClient;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::SpanWithStatus;
using ::testing::_;

// A helper to set expectations on a mock async reader. It captures the OTel
// context and returns a future that can be controlled by the test.
auto expect_context = [](auto& p) {
  return [&p] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    EXPECT_TRUE(OTelContextCaptured());
    return p.get_future();
  };
};

// A helper to be used in a `.then()` clause. It verifies the OTel context
// has been detached before the user receives the result.
auto expect_no_context = [](auto f) {
  auto t = f.get();
  EXPECT_FALSE(ThereIsAnActiveSpan());
  EXPECT_FALSE(OTelContextCaptured());
  return t;
};

TEST(ObjectDescriptorConnectionTracing, Read) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
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
  EXPECT_THAT(spans,
              ElementsAre(AllOf(
                  SpanNamed("test-span-name"),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanHasEvents(AllOf(
                      EventNamed("gl-cpp.open.read"),
                      SpanEventAttributesAre(
                          OTelAttribute<std::int64_t>("read-length", 200),
                          OTelAttribute<std::int64_t>("read-start", 100),
                          OTelAttribute<std::string>(sc::kThreadId, _)))))));
}

TEST(ObjectDescriptorConnectionTracing, ReadThenRead) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();

  auto mock_connection = std::make_shared<MockAsyncObjectDescriptorConnection>();
  auto* mock_reader_ptr = new MockAsyncReaderConnection;
  PromiseWithOTelContext<ReadResponse> p;
  EXPECT_CALL(*mock_reader_ptr, Read).WillOnce(expect_context(p));

  EXPECT_CALL(*mock_connection, Read)
      .WillOnce([&](ObjectDescriptorConnection::ReadParams) {
        return std::unique_ptr<storage_experimental::AsyncReaderConnection>(
            mock_reader_ptr);
      });

  auto connection = MakeTracingObjectDescriptorConnection(
      internal::MakeSpan("test-span"), std::move(mock_connection));

  auto reader = connection->Read({});
  auto f = reader->Read().then(expect_no_context);
  p.set_value(ReadPayload("test-payload").set_offset(123));
  (void)f.get();

  connection.reset();  // End the span

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanNamed("test-span"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanHasEvents(
              AllOf(EventNamed("gl-cpp.open.read"),
                    SpanEventAttributesAre(
                        OTelAttribute<std::int64_t>("read-length", 0),
                        OTelAttribute<std::int64_t>("read-start", 0),
                        OTelAttribute<std::string>(sc::kThreadId, _))),
              AllOf(
                  EventNamed("gl-cpp.read"),
                  SpanEventAttributesAre(
                      OTelAttribute<std::int64_t>("message.starting_offset",
                                                  123),
                      OTelAttribute<std::string>(sc::kThreadId, _),
                      OTelAttribute<std::int64_t>("rpc.message.id", 1),
                      // THIS WAS THE MISSING ATTRIBUTE:
                      OTelAttribute<std::string>("rpc.message.type",
                                                 "RECEIVED")))))));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
