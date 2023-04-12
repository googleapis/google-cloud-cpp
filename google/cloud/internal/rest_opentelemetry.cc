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
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/internal/opentelemetry_options.h"
#include "google/cloud/options.h"
#include "absl/strings/match.h"
#include <opentelemetry/context/propagation/global_propagator.h>
#include <opentelemetry/context/propagation/text_map_propagator.h>
#include <opentelemetry/trace/provider.h>
#include <opentelemetry/trace/semantic_conventions.h>
#include <opentelemetry/trace/span_metadata.h>
#include <opentelemetry/trace/span_startoptions.h>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> MakeSpanHttp(
    RestRequest const& request, opentelemetry::nostd::string_view method) {
  namespace sc = opentelemetry::trace::SemanticConventions;
  opentelemetry::trace::StartSpanOptions options;
  options.kind = opentelemetry::trace::SpanKind::kClient;
  auto span =
      internal::GetTracer(internal::CurrentOptions())
          ->StartSpan(absl::StrCat("HTTP/", absl::string_view{method.data(),
                                                              method.size()}),
                      {{sc::kNetTransport, sc::NetTransportValues::kIpTcp},
                       {sc::kHttpMethod, method},
                       {sc::kHttpUrl, request.path()}},
                      options);
  for (auto const& kv : request.headers()) {
    if (kv.second.empty()) {
      span->SetAttribute("http.header." + kv.first, "");
      continue;
    }
    if (absl::EqualsIgnoreCase(kv.first, "authorization")) {
      span->SetAttribute("http.header.authorization",
                         kv.second.front().substr(0, 32));
      continue;
    }
    span->SetAttribute("http.header." + kv.first, kv.second.front());
  }
  return span;
}

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>
MakeSpanHttpPayload(opentelemetry::trace::Span const& request_span) {
  namespace sc = opentelemetry::trace::SemanticConventions;
  opentelemetry::trace::StartSpanOptions options;
  options.kind = opentelemetry::trace::SpanKind::kClient;
  return internal::GetTracer(internal::CurrentOptions())
      ->StartSpan(absl::StrCat("HTTP/Response"),
                  {{sc::kNetTransport, sc::NetTransportValues::kIpTcp}},
                  {{request_span.GetContext(), {/*no attributes*/}}}, options);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
