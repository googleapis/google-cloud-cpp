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

#include "google/cloud/internal/tracing_rest_client.h"
#include "google/cloud/testing_util/mock_http_payload.h"
#include "google/cloud/testing_util/mock_rest_client.h"
#include "google/cloud/testing_util/mock_rest_response.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include <opentelemetry/context/propagation/global_propagator.h>
#include <opentelemetry/trace/propagation/http_trace_context.h>
#include <opentelemetry/trace/semantic_conventions.h>
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::MakeMockHttpPayloadSuccess;
using ::google::cloud::testing_util::MockRestClient;
using ::google::cloud::testing_util::MockRestResponse;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::NotNull;
using ::testing::Return;

std::multimap<std::string, std::string> MockHeaders() {
  return {{"x-test-header-1", "value1"}, {"x-test-header-2", "value2"}};
}

std::string MockContents() {
  return "The quick brown fox jumps over the lazy dog";
}

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
using ::google::cloud::testing_util::EventNamed;
using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::OTelAttribute;
using ::google::cloud::testing_util::SpanHasAttributes;
using ::google::cloud::testing_util::SpanHasEvents;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsClient;
using ::google::cloud::testing_util::SpanNamed;
using ::testing::_;
using ::testing::AllOf;
using ::testing::AtLeast;
using ::testing::Contains;
using ::testing::Pair;
using ::testing::ResultOf;
using ::testing::UnorderedElementsAre;

TEST(TracingRestClient, Delete) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();

  auto impl = std::make_unique<MockRestClient>();
  EXPECT_CALL(*impl, Delete).WillOnce([](RestContext&, RestRequest const&) {
    auto response = std::make_unique<MockRestResponse>();
    EXPECT_CALL(*response, StatusCode)
        .WillRepeatedly(Return(HttpStatusCode::kOk));
    EXPECT_CALL(*response, Headers).WillRepeatedly(Return(MockHeaders()));
    EXPECT_CALL(std::move(*response), ExtractPayload).WillOnce([] {
      return MakeMockHttpPayloadSuccess(MockContents());
    });
    return std::unique_ptr<RestResponse>(std::move(response));
  });

  auto constexpr kUrl = "https://storage.googleapis.com/storage/v1/b/my-bucket";
  RestRequest request(kUrl);
  request.AddHeader("x-test-header-3", "value3");

  auto client = MakeTracingRestClient(std::move(impl));
  rest_internal::RestContext context;
  auto r = client->Delete(context, request);
  ASSERT_STATUS_OK(r);
  auto response = *std::move(r);
  ASSERT_THAT(response, NotNull());
  EXPECT_THAT(response->StatusCode(), Eq(HttpStatusCode::kOk));
  EXPECT_THAT(response->Headers(), ElementsAreArray(MockHeaders()));
  auto contents = ReadAll(std::move(*response).ExtractPayload());
  EXPECT_THAT(contents, IsOkAndHolds(MockContents()));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      UnorderedElementsAre(
          // Request span
          AllOf(
              SpanNamed("HTTP/DELETE"), SpanHasInstrumentationScope(),
              SpanKindIsClient(),
              SpanHasAttributes(
                  OTelAttribute<std::string>(
                      /*sc::kNetworkTransport=*/"network.transport",
                      sc::NetTransportValues::kIpTcp),
                  OTelAttribute<std::string>(
                      /*sc::kHttpRequestMethod=*/"http.request.method",
                      "DELETE"),
                  OTelAttribute<std::string>(/*sc::kUrlFull=*/"url.full", kUrl),
                  OTelAttribute<std::string>(
                      "http.request.header.x-test-header-3", "value3"),
                  OTelAttribute<std::string>(
                      "http.response.header.x-test-header-1", "value1"),
                  OTelAttribute<std::string>(
                      "http.response.header.x-test-header-2", "value2")),
              SpanHasEvents(EventNamed("gl-cpp.read"),
                            EventNamed("gl-cpp.read"))),
          SpanNamed("SendRequest")));
}

TEST(TracingRestClient, HasScope) {
  auto span_catcher = InstallSpanCatcher();

  auto impl = std::make_unique<MockRestClient>();
  EXPECT_CALL(*impl, Get).WillOnce([](RestContext&, RestRequest const&) {
    // Inject an attribute to the current span, which should be the request
    // span.
    auto span = opentelemetry::trace::Tracer::GetCurrentSpan();
    span->SetAttribute("test.attribute", "test.value");
    auto response = std::make_unique<MockRestResponse>();
    EXPECT_CALL(*response, StatusCode)
        .WillRepeatedly(Return(HttpStatusCode::kOk));
    EXPECT_CALL(*response, Headers).Times(AtLeast(1));
    EXPECT_CALL(std::move(*response), ExtractPayload).WillOnce([] {
      return MakeMockHttpPayloadSuccess(MockContents());
    });
    return std::unique_ptr<RestResponse>(std::move(response));
  });

  auto constexpr kUrl = "https://storage.googleapis.com/storage/v1/b/my-bucket";
  RestRequest request(kUrl);

  auto client = MakeTracingRestClient(std::move(impl));
  rest_internal::RestContext context;
  auto r = client->Get(context, request);
  ASSERT_STATUS_OK(r);
  auto response = *std::move(r);
  ASSERT_THAT(response, NotNull());
  EXPECT_THAT(response->StatusCode(), Eq(HttpStatusCode::kOk));
  auto contents = ReadAll(std::move(*response).ExtractPayload());
  EXPECT_THAT(contents, IsOkAndHolds(MockContents()));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              UnorderedElementsAre(
                  AllOf(SpanNamed("HTTP/GET"), SpanHasInstrumentationScope(),
                        SpanKindIsClient(),
                        SpanHasAttributes(OTelAttribute<std::string>(
                            "test.attribute", "test.value")),
                        SpanHasEvents(EventNamed("gl-cpp.read"),
                                      EventNamed("gl-cpp.read"))),
                  SpanNamed("SendRequest")));
}

TEST(TracingRestClient, PropagatesTraceContext) {
  auto span_catcher = InstallSpanCatcher();

  auto impl = std::make_unique<MockRestClient>();
  auto expected_headers_matcher = [] {
    return ResultOf(
        "headers have x-cloud-trace-context, traceparent",
        [](RestContext const& c) { return c.headers(); },
        AllOf(Contains(Pair("x-cloud-trace-context", _)),
              Contains(Pair("traceparent", _))));
  };
  EXPECT_CALL(*impl, Patch(expected_headers_matcher(), _, _))
      .WillOnce([](auto&, auto const&, auto const&) {
        auto response = std::make_unique<MockRestResponse>();
        EXPECT_CALL(*response, StatusCode)
            .WillRepeatedly(Return(HttpStatusCode::kOk));
        EXPECT_CALL(*response, Headers).Times(AtLeast(1));
        EXPECT_CALL(std::move(*response), ExtractPayload).WillOnce([] {
          return MakeMockHttpPayloadSuccess(MockContents());
        });
        return std::unique_ptr<RestResponse>(std::move(response));
      });

  auto constexpr kUrl = "https://storage.googleapis.com/storage/v1/b/my-bucket";
  RestRequest request(kUrl);

  auto client = MakeTracingRestClient(std::move(impl));
  rest_internal::RestContext context;
  auto r = client->Patch(context, request, {});
  ASSERT_STATUS_OK(r);
  auto response = *std::move(r);
  ASSERT_THAT(response, NotNull());
  EXPECT_THAT(response->StatusCode(), Eq(HttpStatusCode::kOk));
  auto contents = ReadAll(std::move(*response).ExtractPayload());
  EXPECT_THAT(contents, IsOkAndHolds(MockContents()));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, UnorderedElementsAre(SpanNamed("HTTP/PATCH"),
                                          SpanNamed("SendRequest")));
}

TEST(TracingRestClient, WithRestContextDetails) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();

  auto impl = std::make_unique<MockRestClient>();
  using PostPayloadType = std::vector<std::pair<std::string, std::string>>;
  auto disambiguate_overload = ::testing::An<PostPayloadType const&>();
  EXPECT_CALL(*impl, Post(_, _, disambiguate_overload))
      .WillOnce([](RestContext& context, auto const&, auto const&) {
        context.set_namelookup_time(std::chrono::microseconds(12345));
        context.set_connect_time(std::chrono::microseconds(23456));
        context.set_appconnect_time(std::chrono::microseconds(34567));
        context.set_local_ip_address("127.0.0.1");
        context.set_local_port(32000);
        context.set_primary_ip_address("192.168.1.1");
        context.set_primary_port(443);
        auto response = std::make_unique<MockRestResponse>();
        EXPECT_CALL(*response, StatusCode)
            .WillRepeatedly(Return(HttpStatusCode::kOk));
        EXPECT_CALL(*response, Headers).Times(AtLeast(1));
        EXPECT_CALL(std::move(*response), ExtractPayload).WillOnce([] {
          return MakeMockHttpPayloadSuccess(MockContents());
        });
        return std::unique_ptr<RestResponse>(std::move(response));
      });

  auto constexpr kUrl = "https://storage.googleapis.com/storage/v1/b/my-bucket";
  RestRequest request(kUrl);

  auto client = MakeTracingRestClient(std::move(impl));
  rest_internal::RestContext context;
  auto r = client->Post(context, request, PostPayloadType{});
  ASSERT_STATUS_OK(r);
  auto response = *std::move(r);
  ASSERT_THAT(response, NotNull());
  EXPECT_THAT(response->StatusCode(), Eq(HttpStatusCode::kOk));
  auto contents = ReadAll(std::move(*response).ExtractPayload());
  EXPECT_THAT(contents, IsOkAndHolds(MockContents()));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      UnorderedElementsAre(
          AllOf(
              SpanNamed("HTTP/POST"),
              SpanHasAttributes(
                  OTelAttribute<std::string>(
                      /*sc::kNetworkTransport=*/"network.transport",
                      sc::NetTransportValues::kIpTcp),
                  OTelAttribute<std::string>(
                      /*sc::kHttpRequestMethod=*/"http.request.method", "POST"),
                  OTelAttribute<std::string>(/*sc::kUrlFull=*/"url.full", kUrl),
                  OTelAttribute<std::string>(
                      /*sc::kServerAddress=*/"server.address", "192.168.1.1"),
                  OTelAttribute<std::int32_t>(/*sc::kServerPort=*/"server.port",
                                              443),
                  OTelAttribute<std::string>(
                      /*sc::kClientAddress=*/"client.address", "127.0.0.1"),
                  OTelAttribute<std::int32_t>(/*sc::kClientPort=*/"client.port",
                                              32000)),
              SpanHasEvents(EventNamed("gl-cpp.read"),
                            EventNamed("gl-cpp.read"))),
          AllOf(SpanNamed("SendRequest"),
                SpanHasAttributes(
                    OTelAttribute<bool>("gl-cpp.cached_connection", false)),
                SpanHasEvents(EventNamed("gl-cpp.curl.namelookup"),
                              EventNamed("gl-cpp.curl.connected"),
                              EventNamed("gl-cpp.curl.ssl.handshake")))));
}

TEST(TracingRestClient, CensorsAuthFields) {
  auto span_catcher = InstallSpanCatcher();

  auto impl = std::make_unique<MockRestClient>();
  EXPECT_CALL(*impl, Delete).WillOnce([](RestContext&, RestRequest const&) {
    auto response = std::make_unique<MockRestResponse>();
    EXPECT_CALL(*response, StatusCode)
        .WillRepeatedly(Return(HttpStatusCode::kOk));
    EXPECT_CALL(*response, Headers).WillRepeatedly(Return(MockHeaders()));
    EXPECT_CALL(std::move(*response), ExtractPayload).WillOnce([] {
      return MakeMockHttpPayloadSuccess(MockContents());
    });
    return std::unique_ptr<RestResponse>(std::move(response));
  });

  auto constexpr kUrl = "https://storage.googleapis.com/storage/v1/b/my-bucket";
  RestRequest request(kUrl);

  auto client = MakeTracingRestClient(std::move(impl));
  rest_internal::RestContext context;
  context.AddHeader("authorization", "bearer: ABCDEFGHIJKLMNOPQRSTUVWXYZ");
  context.AddHeader("x-goog-api-key", "ABCDEFGHIJKLMNOPQRSTUVWXYZ");

  auto r = client->Delete(context, request);
  ASSERT_STATUS_OK(r);
  auto response = *std::move(r);
  ASSERT_THAT(response, NotNull());
  EXPECT_THAT(response->StatusCode(), Eq(HttpStatusCode::kOk));
  EXPECT_THAT(response->Headers(), ElementsAreArray(MockHeaders()));
  auto contents = ReadAll(std::move(*response).ExtractPayload());
  EXPECT_THAT(contents, IsOkAndHolds(MockContents()));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      Contains(SpanHasAttributes(
          OTelAttribute<std::string>("http.request.header.authorization",
                                     "bearer: ABCDEFGHIJKLMNOPQRSTUVWX"),
          OTelAttribute<std::string>("http.request.header.x-goog-api-key",
                                     "ABCDEFGHIJKL..."))));
}

TEST(TracingRestClient, CachedConnection) {
  auto span_catcher = InstallSpanCatcher();

  auto impl = std::make_unique<MockRestClient>();
  EXPECT_CALL(*impl, Put)
      .WillOnce([](RestContext& context, auto const&, auto const&) {
        context.set_connect_time(std::chrono::microseconds(0));
        auto response = std::make_unique<MockRestResponse>();
        EXPECT_CALL(*response, StatusCode)
            .WillRepeatedly(Return(HttpStatusCode::kOk));
        EXPECT_CALL(*response, Headers).Times(AtLeast(1));
        EXPECT_CALL(std::move(*response), ExtractPayload).WillOnce([] {
          return MakeMockHttpPayloadSuccess(MockContents());
        });
        return std::unique_ptr<RestResponse>(std::move(response));
      });

  auto constexpr kUrl = "https://storage.googleapis.com/storage/v1/b/my-bucket";
  RestRequest request(kUrl);

  auto client = MakeTracingRestClient(std::move(impl));
  rest_internal::RestContext context;
  auto r = client->Put(context, request, {});
  ASSERT_STATUS_OK(r);
  auto response = *std::move(r);
  ASSERT_THAT(response, NotNull());
  EXPECT_THAT(response->StatusCode(), Eq(HttpStatusCode::kOk));
  auto contents = ReadAll(std::move(*response).ExtractPayload());
  EXPECT_THAT(contents, IsOkAndHolds(MockContents()));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              UnorderedElementsAre(
                  SpanNamed("HTTP/PUT"),
                  AllOf(SpanNamed("SendRequest"),
                        SpanHasAttributes(OTelAttribute<bool>(
                            "gl-cpp.cached_connection", true)),
                        SpanHasEvents(EventNamed("gl-cpp.curl.connected")))));
}

#else

TEST(TracingRestClient, NoOpenTelemetry) {
  auto impl = std::make_unique<MockRestClient>();
  EXPECT_CALL(*impl, Delete).WillOnce([](RestContext&, RestRequest const&) {
    auto response = std::make_unique<MockRestResponse>();
    EXPECT_CALL(*response, StatusCode)
        .WillRepeatedly(Return(HttpStatusCode::kOk));
    EXPECT_CALL(*response, Headers).WillRepeatedly(Return(MockHeaders()));
    EXPECT_CALL(std::move(*response), ExtractPayload).WillOnce([] {
      return MakeMockHttpPayloadSuccess(MockContents());
    });
    return std::unique_ptr<RestResponse>(std::move(response));
  });

  auto constexpr kUrl = "https://storage.googleapis.com/storage/v1/b/my-bucket";
  RestRequest request(kUrl);
  request.AddHeader("x-test-header-3", "value3");

  auto client = MakeTracingRestClient(std::move(impl));
  rest_internal::RestContext context;
  auto r = client->Delete(context, request);
  ASSERT_STATUS_OK(r);
  auto response = *std::move(r);
  ASSERT_THAT(response, NotNull());
  EXPECT_THAT(response->StatusCode(), Eq(HttpStatusCode::kOk));
  EXPECT_THAT(response->Headers(), ElementsAreArray(MockHeaders()));
  auto contents = ReadAll(std::move(*response).ExtractPayload());
  EXPECT_THAT(contents, IsOkAndHolds(MockContents()));
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
