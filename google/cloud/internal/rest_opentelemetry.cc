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
#include "google/cloud/internal/rest_context.h"
#include "google/cloud/internal/trace_propagator.h"
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
namespace {

/**
 * A [carrier] for RestClient.
 *
 * [carrier]:
 * https://opentelemetry.io/docs/reference/specification/context/api-propagators/#carrier
 */
class RestClientCarrier
    : public opentelemetry::context::propagation::TextMapCarrier {
 public:
  explicit RestClientCarrier(RestContext& context) : context_(context) {}

  // Unneeded by clients.
  opentelemetry::nostd::string_view Get(
      opentelemetry::nostd::string_view) const noexcept override {
    return "";
  }

  void Set(opentelemetry::nostd::string_view key,
           opentelemetry::nostd::string_view value) noexcept override {
    context_.AddHeader({key.data(), key.size()}, {value.data(), value.size()});
  }

 private:
  RestContext& context_;
};

}  // namespace

void InjectTraceContext(
    RestContext& context,
    opentelemetry::context::propagation::TextMapPropagator& propagator) {
  auto current = opentelemetry::context::RuntimeContext::GetCurrent();
  RestClientCarrier carrier(context);
  propagator.Inject(carrier, current);
}

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> MakeSpanHttp(
    RestRequest const& request, opentelemetry::nostd::string_view method) {
  namespace sc = opentelemetry::trace::SemanticConventions;
  opentelemetry::trace::StartSpanOptions options;
  options.kind = opentelemetry::trace::SpanKind::kClient;
  auto span = internal::MakeSpan(
      absl::StrCat("HTTP/", absl::string_view{method.data(), method.size()}),
      {{/*sc::kNetworkTransport=*/"network.transport",
        sc::NetTransportValues::kIpTcp},
       {/*sc::kHttpRequestMethod=*/"http.request.method", method},
       {/*sc::kUrlFull=*/"url.full", request.path()}},
      options);
  for (auto const& kv : request.headers()) {
    auto const name = "http.request.header." + kv.first;
    if (kv.second.empty()) {
      span->SetAttribute(name, "");
      continue;
    }
    if (absl::EqualsIgnoreCase(kv.first, "authorization")) {
      span->SetAttribute(name, kv.second.front().substr(0, 32));
      continue;
    }
    span->SetAttribute(name, kv.second.front());
  }
  return span;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
