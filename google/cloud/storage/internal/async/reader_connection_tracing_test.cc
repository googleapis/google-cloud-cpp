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

using ::google::cloud::storage_experimental::ReadPayload;
using ::google::cloud::storage_mocks::MockAsyncReaderConnection;
using ::google::cloud::testing_util::InstallSpanCatcher;
using ReadResponse =
    ::google::cloud::storage_experimental::AsyncReaderConnection::ReadResponse;
using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::testing_util::EventNamed;
using ::google::cloud::testing_util::OTelAttribute;
using ::google::cloud::testing_util::SpanEventAttributesAre;
using ::google::cloud::testing_util::SpanHasAttributes;
using ::google::cloud::testing_util::SpanHasEvents;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsClient;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::SpanWithStatus;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;

TEST(ReaderConnectionTracing, WithError) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_unique<MockAsyncReaderConnection>();
  EXPECT_CALL(*mock, Read)
      .WillOnce(
          [] { return make_ready_future(ReadResponse(ReadPayload("m1"))); })
      .WillOnce(
          [] { return make_ready_future(ReadResponse(PermanentError())); });
  auto actual = MakeTracingReaderConnection(
      internal::MakeSpan("test-span-name"), std::move(mock));
  actual->Read().get();
  actual->Read().get();

  auto spans = span_catcher->GetSpans();
  auto const expected_code = static_cast<std::int32_t>(PermanentError().code());
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanNamed("test-span-name"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError,
                         PermanentError().message()),
          SpanHasAttributes(
              OTelAttribute<std::int32_t>("gcloud.status_code", expected_code)),
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanHasEvents(
              AllOf(
                  EventNamed("gl-cpp.read"),
                  SpanEventAttributesAre(
                      OTelAttribute<std::int64_t>(sc::kMessageId, 1),
                      OTelAttribute<std::string>(sc::kMessageType, "RECEIVED"),
                      OTelAttribute<std::int64_t>("message.starting_offset", 0),
                      OTelAttribute<std::string>(sc::kThreadId, _))),
              AllOf(
                  EventNamed("gl-cpp.read"),
                  SpanEventAttributesAre(
                      OTelAttribute<std::int64_t>(sc::kMessageId, 2),
                      OTelAttribute<std::string>(sc::kMessageType, "RECEIVED"),
                      OTelAttribute<std::string>(sc::kThreadId, _)))))));
}

TEST(ReaderConnectionTracing, WithSuccess) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_unique<MockAsyncReaderConnection>();
  EXPECT_CALL(*mock, Read)
      .WillOnce(
          [] { return make_ready_future(ReadResponse(ReadPayload("m1"))); })
      .WillOnce([] {
        return make_ready_future(
            ReadResponse(ReadPayload("m2").set_offset(1024)));
      })
      .WillOnce([] { return make_ready_future(ReadResponse(Status{})); });
  auto actual = MakeTracingReaderConnection(
      internal::MakeSpan("test-span-name"), std::move(mock));
  (void)actual->Read().get();
  (void)actual->Read().get();
  (void)actual->Read().get();

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
                      OTelAttribute<std::int64_t>(sc::kMessageId, 1),
                      OTelAttribute<std::string>(sc::kMessageType, "RECEIVED"),
                      OTelAttribute<std::int64_t>("message.starting_offset", 0),
                      OTelAttribute<std::string>(sc::kThreadId, _))),
              AllOf(
                  EventNamed("gl-cpp.read"),
                  SpanEventAttributesAre(
                      OTelAttribute<std::int64_t>(sc::kMessageId, 2),
                      OTelAttribute<std::string>(sc::kMessageType, "RECEIVED"),
                      OTelAttribute<std::int64_t>("message.starting_offset",
                                                  1024),
                      OTelAttribute<std::string>(sc::kThreadId, _))),
              AllOf(
                  EventNamed("gl-cpp.read"),
                  SpanEventAttributesAre(
                      OTelAttribute<std::int64_t>(sc::kMessageId, 3),
                      OTelAttribute<std::string>(sc::kMessageType, "RECEIVED"),
                      OTelAttribute<std::string>(sc::kThreadId, _)))))));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
