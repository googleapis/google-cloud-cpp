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
#include "google/cloud/internal/opentelemetry_options.h"
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
using ::google::cloud::testing_util::SpanAttribute;
using ::google::cloud::testing_util::SpanHasAttributes;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsClient;
using ::google::cloud::testing_util::SpanKindIsConsumer;
using ::google::cloud::testing_util::SpanNamed;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Gt;

TEST(RestOpentelemetry, MakeSpanHttp) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();

  auto constexpr kUrl = "https://storage.googleapis.com/storage/v1/b/my-bucket";
  RestRequest request(kUrl);
  auto const secret =
      std::string{"Bearer secret-0123456789aaaaabbbbbcccccddddd"};
  ASSERT_THAT(secret.size(), Gt(32));
  request.AddHeader("Authorization", secret);
  request.AddHeader("X-Goog-Foo", "bar");

  auto span = MakeSpanHttp(request, "GET");
  span->End();

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("HTTP/GET"),
          SpanHasAttributes(
              SpanAttribute<std::string>(sc::kNetTransport,
                                         sc::NetTransportValues::kIpTcp),
              SpanAttribute<std::string>(sc::kHttpMethod, "GET"),
              SpanAttribute<std::string>(sc::kHttpUrl, kUrl),
              SpanAttribute<std::string>("http.request.header.authorization",
                                         secret.substr(0, 32))))));
}

TEST(RestOpentelemetry, MakeSpanHttpPayload) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();

  RestRequest request("https://example.com/ignored");
  auto request_span = MakeSpanHttp(request, "GET");

  auto span = MakeSpanHttpPayload(*request_span);
  request_span->End();
  span->End();

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      Contains(AllOf(SpanHasInstrumentationScope(), SpanKindIsConsumer(),
                     SpanNamed("HTTP/Response"),
                     SpanHasAttributes(SpanAttribute<std::string>(
                         sc::kNetTransport, sc::NetTransportValues::kIpTcp)))));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
