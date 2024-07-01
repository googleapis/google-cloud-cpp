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

using ::google::cloud::testing_util::EventNamed;
using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::MakeMockHttpPayloadSuccess;
using ::google::cloud::testing_util::MockHttpPayload;
using ::google::cloud::testing_util::OTelAttribute;
using ::google::cloud::testing_util::SpanEventAttributesAre;
using ::google::cloud::testing_util::SpanHasAttributes;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsClient;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

std::string MockContents() {
  return "The quick brown fox jumps over the lazy dog";
}

auto MakeReadMatcher(std::int64_t buffer_size, std::int64_t read_size) {
  return AllOf(EventNamed("gl-cpp.read"),
               SpanEventAttributesAre(
                   OTelAttribute<std::int64_t>("read.buffer.size", buffer_size),
                   OTelAttribute<std::int64_t>("read.returned.size", read_size),
                   OTelAttribute<std::int64_t>("read.latency.us", _)));
}

auto MakeReadMatcher(std::int64_t buffer_size) {
  return AllOf(
      EventNamed("gl-cpp.read"),
      SpanEventAttributesAre(
          OTelAttribute<std::string>("read.status.code", "UNAVAILABLE"),
          OTelAttribute<std::int64_t>("read.buffer.size", buffer_size),
          OTelAttribute<std::int64_t>("read.latency.us", _)));
}

TEST(TracingHttpPayload, Success) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();

  auto impl = MakeMockHttpPayloadSuccess(MockContents());
  RestRequest request("https://example.com/ignored");
  auto span = MakeSpanHttp(request, "GET");

  TracingHttpPayload payload(std::move(impl), std::move(span));
  std::vector<char> buffer(16);
  auto read = [&] { return payload.Read(absl::Span<char>(buffer)); };
  for (auto s = read(); s && s.value() != 0; s = read()) continue;

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      UnorderedElementsAre(AllOf(
          SpanNamed("HTTP/GET"), SpanHasInstrumentationScope(),
          SpanKindIsClient(),
          SpanHasAttributes(OTelAttribute<std::string>(
              /*sc::kNetworkTransport=*/"network.transport",
              sc::NetTransportValues::kIpTcp)),
          SpanHasEvents(MakeReadMatcher(16, 16), MakeReadMatcher(16, 16),
                        MakeReadMatcher(16, 11), MakeReadMatcher(16, 0)))));
}

TEST(TracingHttpPayload, Failure) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();

  RestRequest request("https://example.com/ignored");
  auto span = MakeSpanHttp(request, "GET");

  auto impl = std::make_unique<MockHttpPayload>();
  EXPECT_CALL(*impl, Read)
      .WillOnce(Return(16))
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")));

  TracingHttpPayload payload(std::move(impl), std::move(span));
  std::vector<char> buffer(16);
  auto status = payload.Read(absl::Span<char>(buffer.data(), buffer.size()));
  ASSERT_STATUS_OK(status);
  EXPECT_EQ(*status, 16);
  status = payload.Read(absl::Span<char>(buffer.data(), buffer.size()));
  EXPECT_THAT(status, StatusIs(StatusCode::kUnavailable));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      UnorderedElementsAre(AllOf(
          SpanNamed("HTTP/GET"), SpanHasInstrumentationScope(),
          SpanKindIsClient(),
          SpanHasAttributes(
              OTelAttribute<std::string>(
                  /*sc::kNetworkTransport=*/"network.transport",
                  sc::NetTransportValues::kIpTcp),
              OTelAttribute<int>("gl-cpp.status_code",
                                 static_cast<int>(StatusCode::kUnavailable))),
          SpanHasEvents(MakeReadMatcher(16, 16), MakeReadMatcher(16)))));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
