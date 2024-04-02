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

#include "google/cloud/storage/internal/async/rewriter_connection_tracing.h"
#include "google/cloud/storage/mocks/mock_async_rewriter_connection.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/opentelemetry_options.h"
#include "google/cloud/testing_util/async_sequencer.h"
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

using ::google::cloud::storage_mocks::MockAsyncRewriterConnection;
using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::EventNamed;
using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::OTelAttribute;
using ::google::cloud::testing_util::OTelContextCaptured;
using ::google::cloud::testing_util::SpanEventAttributesAre;
using ::google::cloud::testing_util::SpanHasEvents;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsClient;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::SpanWithStatus;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::ThereIsAnActiveSpan;
using ::google::storage::v2::RewriteResponse;
using ::testing::IsEmpty;

auto PermanentError() {
  return StatusOr<RewriteResponse>(
      storage::testing::canonical_errors::PermanentError());
}

TEST(RewriterTracingConnection, Basic) {
  auto span_catcher = InstallSpanCatcher();
  AsyncSequencer<void> sequencer;

  auto mock = std::make_unique<MockAsyncRewriterConnection>();
  EXPECT_CALL(*mock, Iterate)
      .WillOnce([&] {
        EXPECT_TRUE(ThereIsAnActiveSpan());
        EXPECT_TRUE(OTelContextCaptured());
        return sequencer.PushBack("Iterate(1)").then([](auto) {
          EXPECT_FALSE(ThereIsAnActiveSpan());
          EXPECT_FALSE(OTelContextCaptured());
          return PermanentError();
        });
      })
      .WillOnce([&] {
        EXPECT_TRUE(ThereIsAnActiveSpan());
        EXPECT_TRUE(OTelContextCaptured());
        return sequencer.PushBack("Iterate(2)").then([](auto) {
          EXPECT_FALSE(ThereIsAnActiveSpan());
          EXPECT_FALSE(OTelContextCaptured());
          RewriteResponse response;
          response.set_total_bytes_rewritten(1000);
          response.set_object_size(3000);
          response.set_rewrite_token("test-token");
          return make_status_or(std::move(response));
        });
      })
      .WillOnce([&] {
        EXPECT_TRUE(ThereIsAnActiveSpan());
        EXPECT_TRUE(OTelContextCaptured());
        return sequencer.PushBack("Iterate(3)").then([](auto) {
          EXPECT_FALSE(ThereIsAnActiveSpan());
          EXPECT_FALSE(OTelContextCaptured());
          RewriteResponse response;
          response.set_total_bytes_rewritten(3000);
          response.set_object_size(3000);
          response.mutable_resource()->set_size(3000);
          return make_status_or(std::move(response));
        });
      });

  auto actual =
      MakeTracingAsyncRewriterConnection(std::move(mock), /*enabled=*/true);
  auto r1 = actual->Iterate();
  sequencer.PopFront().set_value();
  EXPECT_THAT(r1.get(), StatusIs(PermanentError().status().code()));

  auto r2 = actual->Iterate();
  sequencer.PopFront().set_value();
  auto partial = r2.get();
  EXPECT_EQ(partial->total_bytes_rewritten(), 1000);
  EXPECT_EQ(partial->object_size(), 3000);
  EXPECT_EQ(partial->rewrite_token(), "test-token");
  EXPECT_FALSE(partial->has_resource());

  auto r3 = actual->Iterate();
  sequencer.PopFront().set_value();
  auto result = r3.get();
  EXPECT_EQ(result->total_bytes_rewritten(), 3000);
  EXPECT_EQ(result->object_size(), 3000);
  EXPECT_THAT(result->rewrite_token(), IsEmpty());
  ASSERT_TRUE(result->has_resource());
  EXPECT_EQ(result->resource().size(), 3000);

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              ElementsAre(AllOf(
                  SpanNamed("storage::AsyncConnection::RewriteObject"),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanHasEvents(
                      AllOf(EventNamed("gl-cpp.storage.rewrite.iterate"),
                            SpanEventAttributesAre(OTelAttribute<std::int32_t>(
                                "gl-cpp.status_code",
                                static_cast<std::int32_t>(
                                    PermanentError().status().code())))),
                      AllOf(EventNamed("gl-cpp.storage.rewrite.iterate"),
                            SpanEventAttributesAre(
                                OTelAttribute<std::int32_t>(
                                    "gl-cpp.status_code",
                                    static_cast<std::int32_t>(StatusCode::kOk)),
                                OTelAttribute<std::int64_t>(
                                    "total_bytes_rewritten",
                                    static_cast<std::int64_t>(1000)),
                                OTelAttribute<std::int64_t>(
                                    "object_size",
                                    static_cast<std::int64_t>(3000))))))));
}

TEST(RewriterTracingConnection, Disabled) {
  auto mock = std::make_unique<MockAsyncRewriterConnection>();
  auto* const expected = mock.get();

  auto actual =
      MakeTracingAsyncRewriterConnection(std::move(mock), /*enabled=*/false);
  EXPECT_EQ(actual.get(), expected);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
