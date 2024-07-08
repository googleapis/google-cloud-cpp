// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#include "google/cloud/storage/internal/tracing_object_read_source.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <utility>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::internal::ReadSourceResult;
using ::google::cloud::storage::testing::MockObjectReadSource;
using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::testing_util::EventNamed;
using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::OTelAttribute;
using ::google::cloud::testing_util::SpanEventAttributesAre;
using ::google::cloud::testing_util::SpanHasEvents;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsClient;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::SpanWithParent;
using ::google::cloud::testing_util::SpanWithStatus;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::ThereIsAnActiveSpan;
using ::testing::AllOf;
using ::testing::ResultOf;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

template <typename M>
auto ResultBytesReceivedAre(M&& matcher) {
  return ResultOf(
      "response byte count",
      [](storage::internal::ReadSourceResult const& r) {
        return r.bytes_received;
      },
      std::forward<M>(matcher));
}

template <typename M>
auto ReadEventWith(int expected_size, M&& matcher) {
  using ::testing::_;
  return AllOf(
      EventNamed("gl-cpp.read"),
      SpanEventAttributesAre(
          OTelAttribute<std::int64_t>("read.latency.us", _),
          OTelAttribute<std::int64_t>("read.buffer.size", expected_size),
          std::forward<M>(matcher)));
}

TEST(TracingObjectReadSourceSpan, FullDownload) {
  auto span_catcher = InstallSpanCatcher();
  auto root = internal::MakeSpan("testing::ObjectReadSource");
  auto mock = std::make_unique<MockObjectReadSource>();
  EXPECT_CALL(*mock, IsOpen).WillOnce(Return(true));
  EXPECT_CALL(*mock, Read)
      .WillOnce([root] {
        EXPECT_TRUE(ThereIsAnActiveSpan());
        EXPECT_EQ(opentelemetry::trace::Tracer::GetCurrentSpan()->GetContext(),
                  root->GetContext());
        auto c1 = internal::MakeSpan("Child1");
        internal::EndSpan(*c1);
        return ReadSourceResult{/*.bytes_received=*/1234, {}};
      })
      .WillOnce([root] {
        EXPECT_TRUE(ThereIsAnActiveSpan());
        EXPECT_EQ(opentelemetry::trace::Tracer::GetCurrentSpan()->GetContext(),
                  root->GetContext());
        auto c2 = internal::MakeSpan("Child2");
        auto c3 = internal::MakeSpan("Child3");
        internal::EndSpan(*c3);
        internal::EndSpan(*c2);
        return ReadSourceResult{/*.bytes_received=*/0, {}};
      });

  {
    TracingObjectReadSource under_test(root, std::move(mock));
    EXPECT_TRUE(under_test.IsOpen());
    ASSERT_THAT(under_test.Read(nullptr, 1024),
                IsOkAndHolds(ResultBytesReceivedAre(1234)));
    ASSERT_THAT(under_test.Read(nullptr, 2048),
                IsOkAndHolds(ResultBytesReceivedAre(0)));
    EXPECT_THAT(
        span_catcher->GetSpans(),
        UnorderedElementsAre(AllOf(SpanNamed("Child1"), SpanWithParent(root)),
                             AllOf(SpanNamed("Child2"), SpanWithParent(root)),
                             AllOf(SpanNamed("Child3"), SpanWithParent(root))));
  }

  EXPECT_THAT(
      span_catcher->GetSpans(),
      UnorderedElementsAre(AllOf(
          SpanNamed("testing::ObjectReadSource"), SpanHasInstrumentationScope(),
          SpanKindIsClient(),
          SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
          SpanHasEvents(ReadEventWith(1024, OTelAttribute<std::int64_t>(
                                                "read.returned.size", 1234)),
                        ReadEventWith(2048, OTelAttribute<std::int64_t>(
                                                "read.returned.size", 0))))));
}

TEST(TracingObjectReadSourceSpan, FailedDownload) {
  auto span_catcher = InstallSpanCatcher();
  auto root = internal::MakeSpan("testing::ObjectReadSource");
  auto mock = std::make_unique<MockObjectReadSource>();
  EXPECT_CALL(*mock, IsOpen).WillOnce(Return(true));
  EXPECT_CALL(*mock, Read)
      .WillOnce([root] {
        EXPECT_TRUE(ThereIsAnActiveSpan());
        EXPECT_EQ(opentelemetry::trace::Tracer::GetCurrentSpan()->GetContext(),
                  root->GetContext());
        auto c1 = internal::MakeSpan("Child1");
        internal::EndSpan(*c1);
        return ReadSourceResult{/*.bytes_received=*/1234, {}};
      })
      .WillOnce([root] {
        EXPECT_TRUE(ThereIsAnActiveSpan());
        EXPECT_EQ(opentelemetry::trace::Tracer::GetCurrentSpan()->GetContext(),
                  root->GetContext());
        auto c2 = internal::MakeSpan("Child2");
        auto c3 = internal::MakeSpan("Child3");
        internal::EndSpan(*c3);
        internal::EndSpan(*c2);
        return PermanentError();
      });

  TracingObjectReadSource under_test(root, std::move(mock));
  EXPECT_TRUE(under_test.IsOpen());
  ASSERT_THAT(under_test.Read(nullptr, 1024),
              IsOkAndHolds(ResultBytesReceivedAre(1234)));
  ASSERT_THAT(under_test.Read(nullptr, 2048),
              StatusIs(PermanentError().code()));
  EXPECT_THAT(
      span_catcher->GetSpans(),
      UnorderedElementsAre(
          AllOf(SpanNamed("testing::ObjectReadSource"),
                SpanHasInstrumentationScope(), SpanKindIsClient(),
                SpanWithStatus(opentelemetry::trace::StatusCode::kError),
                SpanHasEvents(
                    ReadEventWith(1024, OTelAttribute<std::int64_t>(
                                            "read.returned.size", 1234)),
                    ReadEventWith(2048, OTelAttribute<std::string>(
                                            "read.status.code",
                                            StatusCodeToString(
                                                PermanentError().code()))))),
          AllOf(SpanNamed("Child1"), SpanWithParent(root)),
          AllOf(SpanNamed("Child2"), SpanWithParent(root)),
          AllOf(SpanNamed("Child3"), SpanWithParent(root))));
}

TEST(TracingObjectReadSourceSpan, CloseDownload) {
  auto span_catcher = InstallSpanCatcher();
  auto root = internal::MakeSpan("testing::ObjectReadSource");
  auto mock = std::make_unique<MockObjectReadSource>();
  EXPECT_CALL(*mock, IsOpen).WillOnce(Return(true));
  EXPECT_CALL(*mock, Read).WillOnce([root] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    EXPECT_EQ(opentelemetry::trace::Tracer::GetCurrentSpan()->GetContext(),
              root->GetContext());
    auto c1 = internal::MakeSpan("Child1");
    internal::EndSpan(*c1);
    return ReadSourceResult{/*.bytes_received=*/1234, {}};
  });
  EXPECT_CALL(*mock, Close).Times(1);

  {
    TracingObjectReadSource under_test(root, std::move(mock));
    EXPECT_TRUE(under_test.IsOpen());
    ASSERT_THAT(under_test.Read(nullptr, 1024),
                IsOkAndHolds(ResultBytesReceivedAre(1234)));
    under_test.Close();
  }

  EXPECT_THAT(span_catcher->GetSpans(),
              UnorderedElementsAre(
                  AllOf(SpanNamed("testing::ObjectReadSource"),
                        SpanHasInstrumentationScope(), SpanKindIsClient(),
                        SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                        // Cannot collapse these two SpanHasEvents() calls. They
                        // are implemented using an initializer list, and the
                        // elements are of different types.
                        SpanHasEvents(EventNamed("gl-cpp.close")),
                        SpanHasEvents(ReadEventWith(
                            1024, OTelAttribute<std::int64_t>(
                                      "read.returned.size", 1234)))),
                  AllOf(SpanNamed("Child1"), SpanWithParent(root))));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
