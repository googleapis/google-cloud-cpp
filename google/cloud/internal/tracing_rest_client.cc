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
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/internal/rest_opentelemetry.h"
#include "google/cloud/internal/tracing_http_payload.h"
#include "google/cloud/internal/tracing_rest_response.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include <opentelemetry/trace/semantic_conventions.h>
#include <array>
#include <cstdint>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

namespace {

// We always inject the CloudTraceContext. Typically, these requests are going
// to GCP where the X-Cloud-Trace-Context header can connect the request with
// the internal tracing service.
void InjectCloudTraceContext(RestContext& ctx,
                             opentelemetry::trace::Span const& span) {
  auto context = span.GetContext();
  if (!context.IsValid()) return;

  using opentelemetry::trace::SpanId;
  using opentelemetry::trace::TraceId;
  std::array<char, 2 * TraceId::kSize> trace_id;
  context.trace_id().ToLowerBase16(trace_id);
  std::array<char, 2 * SpanId::kSize> span_id;
  context.span_id().ToLowerBase16(span_id);
  std::uint64_t value;
  if (!absl::SimpleHexAtoi(absl::string_view{span_id.data(), span_id.size()},
                           &value)) {
    return;
  }
  ctx.AddHeader(
      "X-Cloud-Trace-Context",
      absl::StrCat(absl::string_view{trace_id.data(), trace_id.size()}, "/",
                   value, ";o=1"));
}

}  // namespace

void ExtractAttributes(
    RestContext const& context,
    StatusOr<std::unique_ptr<RestResponse>> const& request_result,
    opentelemetry::trace::Span& span) {
  namespace sc = opentelemetry::trace::SemanticConventions;
  if (context.primary_ip_address() && context.primary_port()) {
    span.SetAttribute(sc::kNetPeerName, *context.primary_ip_address());
    span.SetAttribute(sc::kNetPeerPort, *context.primary_port());
  }

  if (context.local_ip_address() && context.local_port()) {
    span.SetAttribute(sc::kNetHostName, *context.local_ip_address());
    span.SetAttribute(sc::kNetHostPort, *context.local_port());
  }

  if (context.namelookup_time()) {
    span.SetAttribute(
        "curl.namelookup_time",
        static_cast<std::int64_t>(context.namelookup_time()->count()));
  }
  if (context.connect_time()) {
    span.SetAttribute(
        "curl.connect_time",
        static_cast<std::int64_t>(context.connect_time()->count()));
  }
  if (context.appconnect_time()) {
    span.SetAttribute(
        "curl.appconnect_time",
        static_cast<std::int64_t>(context.appconnect_time()->count()));
  }

  if (!request_result) return;
  auto const& response = *request_result;
  if (!response) return;
  for (auto const& kv : context.headers()) {
    auto const name = "http.request.header." + kv.first;
    if (kv.second.empty()) {
      span.SetAttribute(name, "");
      continue;
    }
    if (absl::EqualsIgnoreCase(kv.first, "authorization")) {
      span.SetAttribute(name, kv.second.front().substr(0, 32));
      continue;
    }
    span.SetAttribute(name, kv.second.front());
  }

  // There are only 32 attributes available per span, and excess attributes are
  // discarded. First add the `x-*` headers. They tend to have more important
  // information.
  for (auto const& kv : response->Headers()) {
    if (!absl::StartsWith(kv.first, "x-")) continue;
    span.SetAttribute("http.response.header." + kv.first, kv.second);
  }
  // Then add all other headers.
  for (auto const& kv : response->Headers()) {
    if (absl::StartsWith(kv.first, "x-")) continue;
    span.SetAttribute("http.response.header." + kv.first, kv.second);
  }
}

/**
 * Extracts information from @p value, and adds it to a span.
 *
 * The span is ended. The original value is returned, for the sake of
 * composition.
 *
 * Note that this function should be called after the RPC has finished.
 */
StatusOr<std::unique_ptr<RestResponse>> EndResponseSpan(
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span,
    RestContext& context, StatusOr<std::unique_ptr<RestResponse>> value) {
  if (!value) return internal::EndSpan(*span, std::move(value));
  ExtractAttributes(context, value, *span);
  return std::unique_ptr<RestResponse>(std::make_unique<TracingRestResponse>(
      *std::move(value), std::move(span)));
}

TracingRestClient::TracingRestClient(std::unique_ptr<RestClient> impl)
    : impl_(std::move(impl)) {}

StatusOr<std::unique_ptr<RestResponse>> TracingRestClient::Delete(
    RestContext& context, RestRequest const& request) {
  auto span = MakeSpanHttp(request, "DELETE");
  auto scope = opentelemetry::trace::Scope(span);
  InjectTraceContext(context, internal::CurrentOptions());
  InjectCloudTraceContext(context, *span);
  return EndResponseSpan(std::move(span), context,
                         impl_->Delete(context, request));
}

StatusOr<std::unique_ptr<RestResponse>> TracingRestClient::Get(
    RestContext& context, RestRequest const& request) {
  auto span = MakeSpanHttp(request, "GET");
  auto scope = opentelemetry::trace::Scope(span);
  InjectTraceContext(context, internal::CurrentOptions());
  InjectCloudTraceContext(context, *span);
  return EndResponseSpan(std::move(span), context,
                         impl_->Get(context, request));
}

StatusOr<std::unique_ptr<RestResponse>> TracingRestClient::Patch(
    RestContext& context, RestRequest const& request,
    std::vector<absl::Span<char const>> const& payload) {
  auto span = MakeSpanHttp(request, "PATCH");
  auto scope = opentelemetry::trace::Scope(span);
  InjectTraceContext(context, internal::CurrentOptions());
  InjectCloudTraceContext(context, *span);
  return EndResponseSpan(std::move(span), context,
                         impl_->Patch(context, request, payload));
}

StatusOr<std::unique_ptr<RestResponse>> TracingRestClient::Post(
    RestContext& context, RestRequest const& request,
    std::vector<absl::Span<char const>> const& payload) {
  auto span = MakeSpanHttp(request, "POST");
  auto scope = opentelemetry::trace::Scope(span);
  InjectTraceContext(context, internal::CurrentOptions());
  InjectCloudTraceContext(context, *span);
  return EndResponseSpan(std::move(span), context,
                         impl_->Post(context, request, payload));
}

StatusOr<std::unique_ptr<RestResponse>> TracingRestClient::Post(
    RestContext& context, RestRequest const& request,
    std::vector<std::pair<std::string, std::string>> const& form_data) {
  auto span = MakeSpanHttp(request, "POST");
  auto scope = opentelemetry::trace::Scope(span);
  InjectTraceContext(context, internal::CurrentOptions());
  InjectCloudTraceContext(context, *span);
  return EndResponseSpan(std::move(span), context,
                         impl_->Post(context, request, form_data));
}

StatusOr<std::unique_ptr<RestResponse>> TracingRestClient::Put(
    RestContext& context, RestRequest const& request,
    std::vector<absl::Span<char const>> const& payload) {
  auto span = MakeSpanHttp(request, "PUT");
  auto scope = opentelemetry::trace::Scope(span);
  InjectTraceContext(context, internal::CurrentOptions());
  InjectCloudTraceContext(context, *span);
  return EndResponseSpan(std::move(span), context,
                         impl_->Put(context, request, payload));
}

std::unique_ptr<RestClient> MakeTracingRestClient(
    std::unique_ptr<RestClient> client) {
  return std::make_unique<TracingRestClient>(std::move(client));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#else

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::unique_ptr<RestClient> MakeTracingRestClient(
    std::unique_ptr<RestClient> client) {
  return client;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
