// Copyright 2023 Google LLC
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
#include "google/cloud/internal/tracing_http_payload.h"
#include "google/cloud/internal/rest_opentelemetry.h"
#include "google/cloud/testing_util/mock_http_payload.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <opentelemetry/context/propagation/global_propagator.h>
#include <opentelemetry/trace/semantic_conventions.h>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::MakeMockHttpPayloadSuccess;
using ::google::cloud::testing_util::MockHttpPayload;
using ::google::cloud::testing_util::SpanAttribute;
using ::google::cloud::testing_util::SpanHasAttributes;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsClient;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AllOf;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

std::string MockContents() {
  return "The quick brown fox jumps over the lazy dog";
}

TEST(TracingHttpPayload, Success) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();

  auto impl = MakeMockHttpPayloadSuccess(MockContents());
  RestRequest request("https://example.com/ignored");
  auto request_span = MakeSpanHttp(request, "GET");

  auto span = MakeSpanHttpPayload(*request_span);
  TracingHttpPayload payload(std::move(impl), span);
  std::vector<char> buffer(16);
  auto read = [&] { return payload.Read(absl::Span<char>(buffer)); };
  for (auto s = read(); s && s.value() != 0; s = read()) continue;

  auto spans = span_catcher->GetSpans();
  auto make_read_event_matcher = [](auto bs, auto rs) {
    return AllOf(SpanNamed("Read"), SpanHasInstrumentationScope(),
                 SpanKindIsClient(),
                 SpanHasAttributes(
                     SpanAttribute<std::int64_t>("read.buffer.size", bs),
                     SpanAttribute<std::int64_t>("read.returned.size", rs)));
  };
  EXPECT_THAT(
      spans,
      UnorderedElementsAre(
          AllOf(SpanNamed("HTTP/Response"), SpanHasInstrumentationScope(),
                SpanKindIsClient(),
                SpanHasAttributes(SpanAttribute<std::string>(
                    sc::kNetTransport, sc::NetTransportValues::kIpTcp))),
          make_read_event_matcher(16, 16), make_read_event_matcher(16, 16),
          make_read_event_matcher(16, 11), make_read_event_matcher(16, 0)));
}

TEST(TracingHttpPayload, Failure) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();

  RestRequest request("https://example.com/ignored");
  auto request_span = MakeSpanHttp(request, "GET");

  auto impl = std::make_unique<MockHttpPayload>();
  EXPECT_CALL(*impl, Read)
      .WillOnce(Return(16))
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")));

  auto span = MakeSpanHttpPayload(*request_span);
  TracingHttpPayload payload(std::move(impl), span);
  std::vector<char> buffer(16);
  auto status = payload.Read(absl::Span<char>(buffer.data(), buffer.size()));
  ASSERT_STATUS_OK(status);
  EXPECT_EQ(*status, 16);
  status = payload.Read(absl::Span<char>(buffer.data(), buffer.size()));
  EXPECT_THAT(status, StatusIs(StatusCode::kUnavailable));

  auto spans = span_catcher->GetSpans();
  auto make_read_success_matcher = [](auto bs, auto rs) {
    return AllOf(SpanNamed("Read"), SpanHasInstrumentationScope(),
                 SpanKindIsClient(),
                 SpanHasAttributes(
                     SpanAttribute<std::int64_t>("read.buffer.size", bs),
                     SpanAttribute<std::int64_t>("read.returned.size", rs)));
  };
  auto make_read_error_matcher = [](auto bs) {
    return AllOf(
        SpanNamed("Read"), SpanHasInstrumentationScope(), SpanKindIsClient(),
        SpanHasAttributes(SpanAttribute<std::int64_t>("read.buffer.size", bs)));
  };
  EXPECT_THAT(
      spans,
      UnorderedElementsAre(
          AllOf(SpanNamed("HTTP/Response"), SpanHasInstrumentationScope(),
                SpanKindIsClient(),
                SpanHasAttributes(
                    SpanAttribute<std::string>(sc::kNetTransport,
                                               sc::NetTransportValues::kIpTcp),
                    SpanAttribute<int>(
                        "gcloud.status_code",
                        static_cast<int>(StatusCode::kUnavailable)))),
          make_read_success_matcher(16, 16), make_read_error_matcher(16)));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
