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
#include "google/cloud/internal/rest_opentelemetry.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <opentelemetry/trace/semantic_conventions.h>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::OTelAttribute;
using ::google::cloud::testing_util::SpanHasAttributes;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsClient;
using ::google::cloud::testing_util::SpanNamed;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Gt;
using ::testing::Pair;

TEST(RestOpentelemetry, MakeSpanHttp) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();

  auto constexpr kUrl = "https://storage.googleapis.com/storage/v1/b/my-bucket";
  RestRequest request(kUrl, RestRequest::HttpHeaders{{"empty", {}}});
  auto const secret =
      std::string{"Bearer secret-0123456789aaaaabbbbbcccccddddd"};
  ASSERT_THAT(secret.size(), Gt(static_cast<std::size_t>(32)));
  request.AddHeader("Authorization", secret);
  request.AddHeader("X-Goog-Foo", "bar");
  auto constexpr kLongValue =
      "0123456789aaaaabbbbb0123456789aaaaabbbbb0123456789aaaaabbbbb0123456789aa"
      "aaabbbbb";
  request.AddHeader("X-Goog-Bar", kLongValue);

  auto span = MakeSpanHttp(request, "GET");
  span->End();

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("HTTP/GET"),
          SpanHasAttributes(
              OTelAttribute<std::string>(
                  /*sc::kNetworkTransport=*/"network.transport",
                  sc::NetTransportValues::kIpTcp),
              OTelAttribute<std::string>(
                  /*sc::kHttpRequestMethod=*/"http.request.method", "GET"),
              OTelAttribute<std::string>(/*sc::kUrlFull=*/"url.full", kUrl),
              OTelAttribute<std::string>("http.request.header.empty", ""),
              OTelAttribute<std::string>("http.request.header.x-goog-foo",
                                         "bar"),
              OTelAttribute<std::string>("http.request.header.x-goog-bar",
                                         kLongValue),
              OTelAttribute<std::string>("http.request.header.authorization",
                                         secret.substr(0, 32))))));
}

TEST(RestOpentelemetry, InjectTraceContext) {
  auto span_catcher = InstallSpanCatcher();
  auto constexpr kUrl = "https://storage.googleapis.com/storage/v1/b/my-bucket";
  RestRequest request(kUrl, RestRequest::HttpHeaders{{"empty", {}}});

  auto span = MakeSpanHttp(request, "GET");
  auto scope = opentelemetry::trace::Scope(span);
  RestContext context;
  auto propagator = internal::MakePropagator();
  InjectTraceContext(context, *propagator);

  span->End();
  EXPECT_THAT(context.headers(),
              AllOf(Contains(Pair("x-cloud-trace-context", _)),
                    Contains(Pair("traceparent", _))));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
