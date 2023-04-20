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
#include "google/cloud/internal/tracing_rest_client.h"
#include "google/cloud/testing_util/mock_http_payload.h"
#include "google/cloud/testing_util/mock_rest_client.h"
#include "google/cloud/testing_util/mock_rest_response.h"
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
using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::MakeMockHttpPayloadSuccess;
using ::google::cloud::testing_util::MockRestClient;
using ::google::cloud::testing_util::MockRestResponse;
using ::google::cloud::testing_util::SpanAttribute;
using ::google::cloud::testing_util::SpanHasAttributes;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsClient;
using ::google::cloud::testing_util::SpanNamed;
using ::testing::AllOf;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

std::multimap<std::string, std::string> MockHeaders() {
  return {{"x-test-header-1", "value1"}, {"x-test-header-2", "value2"}};
}

std::string MockContents() {
  return "The quick brown fox jumps over the lazy dog";
}

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
          AllOf(SpanNamed("HTTP/DELETE"), SpanHasInstrumentationScope(),
                SpanKindIsClient(),
                SpanHasAttributes(
                    SpanAttribute<std::string>(sc::kNetTransport,
                                               sc::NetTransportValues::kIpTcp),
                    SpanAttribute<std::string>(sc::kHttpMethod, "DELETE"),
                    SpanAttribute<std::string>(sc::kHttpUrl, kUrl),
                    SpanAttribute<std::string>(
                        "http.request.header.x-test-header-3", "value3"),
                    SpanAttribute<std::string>(
                        "http.response.header.x-test-header-1", "value1"),
                    SpanAttribute<std::string>(
                        "http.response.header.x-test-header-2", "value2"))),
          // Read span on the HttpPayload
          SpanNamed("Read"), SpanNamed("Read")));
}

TEST(TracingRestClient, HasScope) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
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
  auto r = client->Get(context, request);
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
          AllOf(SpanNamed("HTTP/GET"), SpanHasInstrumentationScope(),
                SpanKindIsClient(),
                SpanHasAttributes(
                    SpanAttribute<std::string>(sc::kNetTransport,
                                               sc::NetTransportValues::kIpTcp),
                    SpanAttribute<std::string>(sc::kHttpMethod, "GET"),
                    SpanAttribute<std::string>(sc::kHttpUrl, kUrl),
                    SpanAttribute<std::string>("test.attribute", "test.value"),
                    SpanAttribute<std::string>(
                        "http.request.header.x-test-header-3", "value3"),
                    SpanAttribute<std::string>(
                        "http.response.header.x-test-header-1", "value1"),
                    SpanAttribute<std::string>(
                        "http.response.header.x-test-header-2", "value2"))),
          // Read span on the HttpPayload
          SpanNamed("Read"), SpanNamed("Read")));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
